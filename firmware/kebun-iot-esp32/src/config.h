#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define DEBUG_SERIAL Serial

// ── DHT22 ──
#define DHT_PIN 4
#define DHT_TYPE DHT22

// ── Max plants ──
#define MAX_PLANTS 10
#define DEFAULT_PLANT_COUNT 3

// ── Default hardware mapping ──
const uint8_t DEFAULT_SENSOR_PINS[] = {34, 35, 32};
const uint8_t DEFAULT_PUMP_PINS[] = {26, 27, 25};
const uint16_t DEFAULT_THRESHOLDS[] = {40, 45, 50};
const uint16_t DEFAULT_DRY_VALS[] = {3500, 3500, 3500};
const uint16_t DEFAULT_WET_VALS[] = {1200, 1200, 1200};
const uint16_t DEFAULT_DURATIONS[] = {5, 5, 5};
const char* DEFAULT_NAMES[] = {"Daun Bawang", "Kemangi", "Tomat Ceri"};
const char* DEFAULT_COLORS[] = {"#4ade80", "#fbbf24", "#60a5fa"};

// ── Network ──
#define AP_SSID "Kebun-IoT-AP"
#define AP_PASSWORD "12345678"
#define HTTP_PORT 80
#define DNS_PORT 53

// ── Timing (ms) ──
#define SENSOR_INTERVAL 5000
#define PUMP_CHECK_INTERVAL 500
#define CAPTIVE_PORTAL_TIMEOUT 300000

// ── Data structures ──
struct PlantConfig {
  uint8_t id;
  char name[32];
  uint8_t sensorPin;
  uint8_t pumpPin;
  uint16_t threshold;
  uint16_t dryValue;
  uint16_t wetValue;
  uint16_t pumpDuration;
  char color[8];
};

struct PlantReading {
  uint16_t moisture;       // 0–100%
  bool pumpActive;
  unsigned long pumpStartMs;
};

// ── Global state (defined in .ino) ──
extern PlantConfig g_plants[MAX_PLANTS];
extern PlantReading g_readings[MAX_PLANTS];
extern int g_plantCount;
extern float g_temp;
extern float g_humidity;
extern bool g_wifiConnected;
extern bool g_apMode;

#endif
