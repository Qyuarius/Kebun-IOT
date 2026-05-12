require('dotenv').config();
const express = require('express');
const cors = require('cors');
const http = require('http');
const { WebSocketServer } = require('ws');
const path = require('path');
const db = require('./db');
const _db = db.getDb();

const PORT = process.env.PORT || 3000;
const app = express();

app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname)));

// ── WebSocket ──────────────────────────────
const server = http.createServer(app);
const wss = new WebSocketServer({ server });
const clients = new Set();

wss.on('connection', (ws) => {
  clients.add(ws);
  ws.on('close', () => clients.delete(ws));
});

function broadcast(data) {
  const msg = JSON.stringify(data);
  for (const ws of clients) {
    if (ws.readyState === 1) ws.send(msg);
  }
}

// ── ESP32 Polling Service ──────────────────
const espState = {
  isLive: false,
  consecutiveFails: 0,
  moistureDrift: {},
  configSynced: false,
};

function generateMockData() {
  const plants = db.getAllPlants();
  const temp = 27 + Math.random() * 5;
  const humidity = 70 + Math.random() * 15;
  return {
    temp: Math.round(temp * 10) / 10,
    humidity_air: Math.round(humidity),
    plants: plants.map(p => {
      if (!espState.moistureDrift[p.id]) espState.moistureDrift[p.id] = 40 + Math.random() * 40;
      let m = espState.moistureDrift[p.id];
      m += (Math.random() - 0.5) * 4;
      m = Math.max(5, Math.min(100, m));
      espState.moistureDrift[p.id] = m;
      const moisture = Math.round(m);
      const pump = moisture < p.threshold;
      return {
        id: p.id, name: p.name, moisture, threshold: p.threshold,
        pump, water_count: Math.floor(Math.random() * 5),
        gpio_sensor: p.gpio_sensor, gpio_pump: p.gpio_pump,
      };
    }),
  };
}

async function fetchFromESP() {
  let espUrl = db.getSetting('esp_url') || 'http://192.168.4.1';
  if (!espUrl.startsWith('http')) espUrl = 'http://' + espUrl;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 5000);
  try {
    const res = await fetch(`${espUrl}/data`, { signal: controller.signal });
    clearTimeout(timeout);
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return await res.json();
  } catch (e) {
    clearTimeout(timeout);
    return null;
  }
}

function getStatusLabel(moisture, threshold, pump) {
  if (pump) return 'MENYIRAM';
  if (moisture < threshold - 10) return 'KERING';
  if (moisture < threshold) return 'PERLU SIRAM';
  return 'NORMAL';
}

async function pollESP() {
  let data = await fetchFromESP();
  const wasLive = espState.isLive;

  if (data && data.plants) {
    const wasLive = espState.isLive;
    espState.isLive = true;
    espState.consecutiveFails = 0;
    if (!wasLive && !espState.configSynced) {
      syncConfigToESP();
      espState.configSynced = true;
    }
  } else {
    espState.consecutiveFails++;
    if (espState.consecutiveFails >= 3 && wasLive) {
      console.log(`[${new Date().toTimeString().slice(0, 8)}] ESP32 offline — using mock data`);
    }
    espState.isLive = false;
    espState.configSynced = false;
    data = generateMockData();
  }

  const plants = db.getAllPlants();
  const now = new Date();

  // Store sensor data & logs
  data.plants.forEach(p => {
    const cfg = plants.find(c => c.id === p.id);
    const threshold = cfg ? cfg.threshold : (p.threshold || 40);
    const pump = espState.isLive ? p.pump : (p.moisture < threshold);
    const status = getStatusLabel(p.moisture, threshold, pump);

    db.insertSensorData(p.id, p.moisture, data.temp, data.humidity_air, pump);
    db.insertLog({
      plantId: p.id, plantName: p.name,
      moisture: p.moisture, temperature: data.temp,
      humidity: data.humidity_air, pump, status,
    });

    p.threshold = threshold;
    p.pump = pump;
    p.status = status;
    p.temperature = data.temp;
    p.humidity = data.humidity_air;
  });

  // Clean old data once a day
  if (now.getHours() === 0 && now.getMinutes() < 5) {
    db.cleanOldData();
  }

  // Broadcast to all WebSocket clients
  broadcast({
    type: 'data_update',
    isLive: espState.isLive,
    temperature: data.temp,
    humidity: data.humidity_air,
    plants: data.plants,
    timestamp: now.toTimeString().slice(0, 8),
  });
}

let pollTimer = null;

async function syncConfigToESP() {
  if (!espState.isLive) return;
  let espUrl = db.getSetting('esp_url') || 'http://192.168.4.1';
  if (!espUrl.startsWith('http')) espUrl = 'http://' + espUrl;
  const plants = db.getAllPlants();
  const payload = { plants: plants.map(p => ({
    id: p.id,
    name: p.name,
    sensor: p.gpio_sensor,
    pump: p.gpio_pump,
    threshold: p.threshold,
    dry: p.dry_value,
    wet: p.wet_value,
    duration: p.pump_duration,
    color: p.color,
  }))};
  try {
    const res = await fetch(`${espUrl}/config/sync`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
      signal: AbortSignal.timeout(5000),
    });
    if (res.ok) console.log(`[ESP] Config synced (${plants.length} plants)`);
  } catch (e) {
    console.log('[ESP] Config sync failed:', e.message);
  }
}

function startPolling() {
  if (pollTimer) clearInterval(pollTimer);
  const interval = parseInt(db.getSetting('fetch_interval') || '30000', 10);
  pollTimer = setInterval(pollESP, interval);
  pollESP(); // immediate first poll
}

// ── REST API ────────────────────────────────

// Plants
app.get('/api/plants', (req, res) => {
  res.json(db.getAllPlants());
});

app.post('/api/plants', (req, res) => {
  const { name, gpio_sensor, gpio_pump, threshold, color, pump_duration, dry_value, wet_value } = req.body;
  if (!name || gpio_sensor == null || gpio_pump == null) {
    return res.status(400).json({ error: 'name, gpio_sensor, gpio_pump required' });
  }
  if (gpio_sensor < 0 || gpio_sensor > 39 || gpio_pump < 0 || gpio_pump > 39) {
    return res.status(400).json({ error: 'GPIO must be 0–39' });
  }
  const plant = db.addPlant({ name, gpio_sensor, gpio_pump, threshold, color, pump_duration, dry_value, wet_value });
  broadcast({ type: 'plants_changed' });
  syncConfigToESP();
  res.status(201).json(plant);
});

app.put('/api/plants/:id', (req, res) => {
  const id = parseInt(req.params.id, 10);
  const plant = db.updatePlant(id, req.body);
  if (!plant) return res.status(404).json({ error: 'Plant not found' });
  broadcast({ type: 'plants_changed' });
  syncConfigToESP();
  res.json(plant);
});

app.delete('/api/plants/:id', (req, res) => {
  db.deletePlant(parseInt(req.params.id, 10));
  broadcast({ type: 'plants_changed' });
  syncConfigToESP();
  res.json({ ok: true });
});

// Current data
app.get('/api/data', (req, res) => {
  const plants = db.getLatestSensorData();
  const totalWater = _db.prepare('SELECT COUNT(*) as c FROM logs WHERE pump = 1 AND date(created_at) = date(?)').get(new Date().toISOString()).c;
  const needWater = plants.filter(p => p.moisture < p.threshold).length;
  const pumpsOn = plants.filter(p => p.pump).length;
  res.json({
    isLive: espState.isLive,
    temperature: plants.length > 0 ? plants[0].temperature : 28,
    humidity: plants.length > 0 ? plants[0].humidity : 75,
    plants,
    summary: {
      active: plants.length,
      need_water: needWater,
      pumps_on: pumpsOn,
      total_water_today: totalWater,
    },
    timestamp: new Date().toTimeString().slice(0, 8),
  });
});

// History
app.get('/api/data/history', (req, res) => {
  const rangeMap = { '30min': 30, '1jam': 60, '6jam': 360, '24jam': 1440 };
  const range = rangeMap[req.query.range] || 30;
  const plants = db.getAllPlants();
  const result = {};
  plants.forEach(p => {
    const rows = db.getHistory(p.id, range);
    result[p.id] = {
      name: p.name,
      color: p.color,
      data: rows.map(r => ({
        time: new Date(r.created_at).toTimeString().slice(0, 5),
        moisture: r.moisture,
        temperature: r.temperature,
      })),
    };
  });
  res.json(result);
});

// Pump toggle
app.post('/api/pump/:id', async (req, res) => {
  const id = parseInt(req.params.id, 10);
  const state = Boolean(req.body.state);
  const plant = db.getPlant(id);
  if (!plant) return res.status(404).json({ error: 'Plant not found' });

  if (espState.isLive) {
    let espUrl = db.getSetting('esp_url') || 'http://192.168.4.1';
    if (!espUrl.startsWith('http')) espUrl = 'http://' + espUrl;
    try {
      const r = await fetch(`${espUrl}/pump?id=${id}&state=${state ? 1 : 0}`, {
        method: 'POST', signal: AbortSignal.timeout(5000),
      });
      const data = await r.json();
      if (!data.ok) return res.status(502).json({ error: 'ESP32 rejected' });
    } catch (e) {
      return res.status(502).json({ error: 'ESP32 unreachable' });
    }
  }

  // Log pump action
  const moisture = _db.prepare('SELECT moisture FROM sensor_data WHERE plant_id = ? ORDER BY created_at DESC LIMIT 1').get(id);
  db.insertLog({
    plantId: id, plantName: plant.name,
    moisture: moisture ? moisture.moisture : 50,
    temperature: 0, humidity: 0, pump: state,
    status: state ? 'MANUAL ON' : 'MANUAL OFF',
  });

  broadcast({ type: 'data_update', isLive: espState.isLive });
  res.json({ ok: true, state });
});

// Logs
app.get('/api/logs', (req, res) => {
  const result = db.getLogs({
    filter: req.query.filter || 'all',
    page: parseInt(req.query.page || '0', 10),
    pageSize: parseInt(req.query.page_size || '50', 10),
  });
  res.json(result);
});

// Export CSV
app.get('/api/export/csv', (req, res) => {
  const filter = req.query.filter || 'all';
  const result = db.getLogs({ filter, page: 0, pageSize: 10000 });
  const headers = 'Waktu,Tanaman,Kelembaban (%),Suhu (C),Kelembaban Udara (%),Pompa,Status';
  const rows = result.rows.map(r =>
    `${r.created_at},${r.plant_name},${r.moisture},${r.temperature || ''},${r.humidity || ''},${r.pump ? 'ON' : 'OFF'},${r.status}`
  );
  const csv = '\uFEFF' + [headers, ...rows].join('\n');
  res.setHeader('Content-Type', 'text/csv; charset=utf-8');
  res.setHeader('Content-Disposition', `attachment; filename="kebun_iot_${new Date().toISOString().slice(0, 10)}.csv"`);
  res.send(csv);
});

// Settings
app.get('/api/settings', (req, res) => {
  res.json(db.getAllSettings());
});

app.put('/api/settings', (req, res) => {
  const body = req.body;
  if (body.esp_url != null) db.setSetting('esp_url', body.esp_url);
  if (body.fetch_interval != null) {
    db.setSetting('fetch_interval', String(body.fetch_interval));
    startPolling();
  }
  if (body.wifi_mode != null) db.setSetting('wifi_mode', body.wifi_mode);
  res.json({ ok: true });
});

// System info
app.get('/api/system', (req, res) => {
  const uptime = process.uptime();
  const h = String(Math.floor(uptime / 3600)).padStart(2, '0');
  const m = String(Math.floor((uptime % 3600) / 60)).padStart(2, '0');
  const s = String(Math.floor(uptime % 60)).padStart(2, '0');
  res.json({
    ip: db.getSetting('esp_url') || 'N/A',
    uptime: `${h}:${m}:${s}`,
    heap: `${(process.memoryUsage().heapUsed / 1024).toFixed(0)} KB`,
    firmware: 'v2.0.0',
    isLive: espState.isLive,
  });
});

// ── Start ──────────────────────────────────
server.listen(PORT, () => {
  console.log(`🌱 Kebun IoT backend running on http://localhost:${PORT}`);
  startPolling();
});
