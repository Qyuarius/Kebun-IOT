const Database = require('better-sqlite3');
const path = require('path');

const DB_PATH = path.join(__dirname, 'data', 'kebun.db');

let db;

function getDb() {
  if (!db) {
    const fs = require('fs');
    const dir = path.dirname(DB_PATH);
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });

    db = new Database(DB_PATH);
    db.pragma('journal_mode = WAL');
    db.pragma('foreign_keys = ON');
    initSchema();
    seedDefaults();
  }
  return db;
}

function initSchema() {
  db.exec(`
    CREATE TABLE IF NOT EXISTS plants (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL,
      gpio_sensor INTEGER NOT NULL,
      gpio_pump INTEGER NOT NULL,
      threshold INTEGER NOT NULL DEFAULT 40,
      color TEXT NOT NULL DEFAULT '#4ade80',
      pump_duration INTEGER NOT NULL DEFAULT 5,
      dry_value INTEGER NOT NULL DEFAULT 3500,
      wet_value INTEGER NOT NULL DEFAULT 1200,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    );

    CREATE TABLE IF NOT EXISTS sensor_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id INTEGER NOT NULL,
      moisture REAL NOT NULL,
      temperature REAL,
      humidity REAL,
      pump INTEGER NOT NULL DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now','localtime')),
      FOREIGN KEY (plant_id) REFERENCES plants(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS logs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id INTEGER NOT NULL,
      plant_name TEXT NOT NULL,
      moisture REAL NOT NULL,
      temperature REAL,
      humidity REAL,
      pump INTEGER NOT NULL DEFAULT 0,
      status TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now','localtime')),
      FOREIGN KEY (plant_id) REFERENCES plants(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS settings (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );

    CREATE INDEX IF NOT EXISTS idx_sensor_data_plant ON sensor_data(plant_id, created_at);
    CREATE INDEX IF NOT EXISTS idx_logs_plant ON logs(plant_id, created_at);
    CREATE INDEX IF NOT EXISTS idx_logs_status ON logs(status);
  `);
}

function seedDefaults() {
  const count = db.prepare('SELECT COUNT(*) as c FROM plants').get().c;
  if (count > 0) return;

  const insertPlant = db.prepare(`
    INSERT INTO plants (name, gpio_sensor, gpio_pump, threshold, color, pump_duration, dry_value, wet_value)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  `);
  const insertSettings = db.prepare(`
    INSERT OR IGNORE INTO settings (key, value) VALUES (?, ?)
  `);

  const tx = db.transaction(() => {
    insertPlant.run('Daun Bawang', 34, 25, 40, '#4ade80', 5, 3200, 2400);

    insertSettings.run('esp_url', 'http://192.168.4.1');
    insertSettings.run('fetch_interval', '30000');
    insertSettings.run('wifi_mode', 'ap');
    insertSettings.run('next_plant_id', '2');
  });
  tx();
}

// ── Plants ──────────────────────────────────
function getAllPlants() {
  return db.prepare('SELECT * FROM plants ORDER BY id').all();
}

function getPlant(id) {
  return db.prepare('SELECT * FROM plants WHERE id = ?').get(id);
}

function addPlant({ name, gpio_sensor, gpio_pump, threshold, color, pump_duration, dry_value, wet_value }) {
  const stmt = db.prepare(`
    INSERT INTO plants (name, gpio_sensor, gpio_pump, threshold, color, pump_duration, dry_value, wet_value)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  `);
  const result = stmt.run(name, gpio_sensor, gpio_pump, threshold || 40, color || '#4ade80', pump_duration || 5, dry_value || 3500, wet_value || 1200);
  return getPlant(result.lastInsertRowid);
}

function updatePlant(id, fields) {
  const allowed = ['name', 'gpio_sensor', 'gpio_pump', 'threshold', 'color', 'pump_duration', 'dry_value', 'wet_value'];
  const sets = [];
  const vals = [];
  for (const [k, v] of Object.entries(fields)) {
    if (allowed.includes(k)) { sets.push(`${k} = ?`); vals.push(v); }
  }
  if (!sets.length) return getPlant(id);
  vals.push(id);
  db.prepare(`UPDATE plants SET ${sets.join(', ')} WHERE id = ?`).run(...vals);
  return getPlant(id);
}

function deletePlant(id) {
  db.prepare('DELETE FROM plants WHERE id = ?').run(id);
}

// ── Sensor Data ─────────────────────────────
function insertSensorData(plantId, moisture, temperature, humidity, pump) {
  db.prepare(`
    INSERT INTO sensor_data (plant_id, moisture, temperature, humidity, pump)
    VALUES (?, ?, ?, ?, ?)
  `).run(plantId, moisture, temperature, humidity, pump ? 1 : 0);
}

function getLatestSensorData() {
  const plants = getAllPlants();
  return plants.map(p => {
    const row = db.prepare(`
      SELECT moisture, temperature, humidity, pump, created_at
      FROM sensor_data WHERE plant_id = ? ORDER BY created_at DESC LIMIT 1
    `).get(p.id);
    return {
      id: p.id,
      name: p.name,
      gpio_sensor: p.gpio_sensor,
      gpio_pump: p.gpio_pump,
      threshold: p.threshold,
      color: p.color,
      pump_duration: p.pump_duration,
      dry_value: p.dry_value,
      wet_value: p.wet_value,
      moisture: row ? row.moisture : 50 + Math.round(Math.random() * 30),
      temperature: row ? row.temperature : 27 + Math.random() * 5,
      humidity: row ? row.humidity : 70 + Math.random() * 15,
      pump: row ? Boolean(row.pump) : false,
      last_update: row ? row.created_at : null,
    };
  });
}

function getHistory(plantId, rangeMinutes) {
  return db.prepare(`
    SELECT moisture, temperature, created_at
    FROM sensor_data WHERE plant_id = ?
    AND datetime(created_at) >= datetime('now','localtime','-${rangeMinutes} minutes')
    ORDER BY created_at ASC
  `).all(plantId);
}

function cleanOldData() {
  db.prepare("DELETE FROM sensor_data WHERE julianday('now','localtime') - julianday(created_at) > 7").run();
  db.prepare("DELETE FROM logs WHERE julianday('now','localtime') - julianday(created_at) > 7").run();
}

// ── Logs ────────────────────────────────────
function insertLog({ plantId, plantName, moisture, temperature, humidity, pump, status }) {
  db.prepare(`
    INSERT INTO logs (plant_id, plant_name, moisture, temperature, humidity, pump, status)
    VALUES (?, ?, ?, ?, ?, ?, ?)
  `).run(plantId, plantName, moisture, temperature || 0, humidity || 0, pump ? 1 : 0, status);
}

function getLogs({ filter, page, pageSize }) {
  page = page || 0;
  pageSize = pageSize || 50;
  let where = '';
  const params = [];
  if (filter && filter !== 'all') {
    if (filter === 'pump') {
      where = 'WHERE pump = 1';
    } else {
      where = 'WHERE plant_name = ?';
      params.push(filter);
    }
  }
  const total = db.prepare(`SELECT COUNT(*) as c FROM logs ${where}`).get(...params).c;
  const offset = page * pageSize;
  const rows = db.prepare(`
    SELECT * FROM logs ${where} ORDER BY created_at DESC LIMIT ? OFFSET ?
  `).all(...params, pageSize, offset);
  return { total, page, pageSize, rows };
}

// ── Settings ────────────────────────────────
function getSetting(key) {
  const row = db.prepare('SELECT value FROM settings WHERE key = ?').get(key);
  return row ? row.value : null;
}

function setSetting(key, value) {
  db.prepare('INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)').run(key, String(value));
}

function getAllSettings() {
  const rows = db.prepare('SELECT * FROM settings').all();
  const obj = {};
  rows.forEach(r => { obj[r.key] = r.value; });
  return obj;
}

// Force initialization on module load
getDb();

module.exports = {
  getDb,
  getAllPlants, getPlant, addPlant, updatePlant, deletePlant,
  insertSensorData, getLatestSensorData, getHistory, cleanOldData,
  insertLog, getLogs,
  getSetting, setSetting, getAllSettings,
};
