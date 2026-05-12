# Kebun IoT

Greenhouse monitoring & automation system berbasis ESP32, Node.js, dan SQLite.

## Arsitektur

```
┌─────────────────┐     HTTP/WS      ┌──────────────┐     HTTP     ┌──────────┐
│  Dashboard Web  │ ←──────────────→ │ Node.js       │ ←──────────→ │  ESP32   │
│  (index.html)   │   localhost:3000 │ Backend +     │  192.168.x.x │ Firmware │
│  Tailwind+Chart │                  │ SQLite        │              │ Arduino  │
└─────────────────┘                  └──────────────┘              └──────────┘
```

## Komponen Hardware (default)

| Komponen | Pin GPIO |
|---|---|
| DHT22 suhu & kelembaban | 4 |
| Soil moisture sensor 1 | 34 (ADC) |
| Soil moisture sensor 2 | 35 (ADC) |
| Soil moisture sensor 3 | 32 (ADC) |
| Relay pompa 1 | 26 |
| Relay pompa 2 | 27 |
| Relay pompa 3 | 25 |

## Cara Menjalankan

### 1. ESP32 — Upload firmware

```bash
cd firmware/kebun-iot-esp32
pio run --target upload       # Compile + flash firmware
pio run --target uploadfs     # Upload captive portal ke LittleFS
```

### 2. ESP32 — Konfigurasi WiFi

- Nyalakan ESP32 → otomatis AP mode `Kebun-IoT-AP` (password: `12345678`)
- Connect dari HP/laptop ke AP tersebut
- Buka `http://192.168.4.1`
- Masukkan SSID & password WiFi rumah → ESP32 restart, connect ke WiFi

### 3. Backend

```bash
npm install
npm start
```

Buka `http://localhost:3000` di browser.

### 4. Set IP ESP32 di Dashboard

- Tab **Settings** → **ESP32 IP Address**
- Isi `http://<ip-esp32>` (cek IP lewat serial monitor atau router)
- Klik **Simpan**

## Fitur

- ✅ Monitoring suhu, kelembaban udara & tanah real-time
- ✅ Auto-watering berdasarkan threshold kelembaban
- ✅ Kontrol pompa manual dari dashboard
- ✅ Chart historis (30 menit s.d. 24 jam)
- ✅ Log events dengan filter dan export CSV
- ✅ Konfigurasi threshold, durasi pompa, interval polling
- ✅ Captive portal untuk setup WiFi pertama kali
- ✅ Sinkronisasi konfigurasi otomatis antara backend ↔ ESP32
- ✅ Fallback ke data simulasi jika ESP32 offline

## API Endpoints

| Method | Endpoint | Deskripsi |
|---|---|---|
| GET | `/api/data` | Data terbaru semua tanaman |
| GET | `/api/data/history?range=30min` | Riwayat sensor |
| GET | `/api/plants` | Daftar tanaman |
| POST | `/api/plants` | Tambah tanaman |
| PUT | `/api/plants/:id` | Update tanaman |
| DELETE | `/api/plants/:id` | Hapus tanaman |
| POST | `/api/pump/:id` | Toggle pompa `{state: bool}` |
| GET | `/api/logs?filter=&page=` | Log events |
| GET | `/api/export/csv` | Export CSV |
| GET | `/api/settings` | Pengaturan |
| PUT | `/api/settings` | Simpan pengaturan |
| GET | `/api/system` | Info sistem |

## Memulai Ulang (setelah mati)

1. Colok power ESP32 → otomatis connect WiFi (tersimpan)
2. Jalankan backend:
   ```bash
   cd "D:\WEB\Kebun IOT"
   npm start
   ```
3. Buka `http://localhost:3000`

## Catatan

- Jika IP ESP32 berubah (DHCP), update di Settings dashboard
- Untuk IP tetap, set DHCP reservation di router
- Maksimal 10 tanaman per ESP32
- Database: SQLite (`data/kebun.db`)
