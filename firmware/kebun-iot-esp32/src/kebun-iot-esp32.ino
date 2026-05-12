/*
  Kebun IoT — ESP32 Firmware v2.0.0
  Greenhouse monitoring & automation system
  Hardware: ESP32 + DHT22 + Capacitive soil moisture sensors + relay pumps
*/

#include "config.h"
#include "sensors.h"
#include "pumps.h"
#include "wifimanager.h"
#include <WebServer.h>
#include "kebun_webserver.h"
#include <DHT.h>

// ── Global state definitions ──
PlantConfig g_plants[MAX_PLANTS];
PlantReading g_readings[MAX_PLANTS];
int g_plantCount = 0;
float g_temp = 0;
float g_humidity = 0;

// ── DHT22 ──
DHT dht(DHT_PIN, DHT_TYPE);

// ── Timing ──
unsigned long lastSensorRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastSerialPrint = 0;
const unsigned long DHT_INTERVAL = 10000;   // every 10s
const unsigned long SERIAL_INTERVAL = 30000;

// ── Forward declarations ──
void readDHT();

void setup() {
  DEBUG_SERIAL.begin(115200);
  delay(500);
  DEBUG_SERIAL.println("\n\n=== Kebun IoT ESP32 v2.0.0 ===");

  // LittleFS for portal pages
  if (!LittleFS.begin(true)) {
    DEBUG_SERIAL.println("LittleFS mount failed");
  }

  // Load config & connect WiFi
  initWiFi();

  // Init hardware
  dht.begin();
  initSensors();
  initPumps();
  initWebServer();

  DEBUG_SERIAL.println("Setup complete");
}

void loop() {
  unsigned long now = millis();

  // ── Handle DNS (captive portal) ──
  processDNS();

  // ── Handle HTTP requests ──
  server.handleClient();

  // ── Read soil moisture sensors ──
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readAllSensors();
    autoWaterPumps();
  }

  // ── Read DHT22 (slower interval) ──
  if (now - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = now;
    readDHT();
  }

  // ── Check pump auto-off timers ──
  if (now - lastSensorRead < PUMP_CHECK_INTERVAL) {
    // Check more frequently when pumps might be active
    checkPumpTimers();
  }
  static unsigned long lastPumpCheck = 0;
  if (now - lastPumpCheck >= PUMP_CHECK_INTERVAL) {
    lastPumpCheck = now;
    checkPumpTimers();
  }

  // ── Print status ──
  if (now - lastSerialPrint >= SERIAL_INTERVAL) {
    lastSerialPrint = now;
    DEBUG_SERIAL.print("[STATUS] WiFi: ");
    DEBUG_SERIAL.print(g_wifiConnected ? "STA" : (g_apMode ? "AP" : "OFF"));
    DEBUG_SERIAL.print(" | Plants: ");
    DEBUG_SERIAL.print(g_plantCount);
    DEBUG_SERIAL.print(" | Temp: ");
    DEBUG_SERIAL.print(g_temp, 1);
    DEBUG_SERIAL.print("°C | Hum: ");
    DEBUG_SERIAL.print(g_humidity, 0);
    DEBUG_SERIAL.println("%");
  }
}

// ── Read DHT22 with error handling ──
void readDHT() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    DEBUG_SERIAL.println("DHT22 read failed, using last values");
    return;
  }

  g_temp = t;
  g_humidity = h;
}
