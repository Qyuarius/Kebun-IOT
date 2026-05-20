#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define DEBUG_SERIAL Serial

// ── DHT22 ──
#define DHT_PIN 32
#define DHT_TYPE DHT22

// ── Max plants ──
#define MAX_PLANTS 10
#define DEFAULT_PLANT_COUNT 1

// ── Default hardware mapping ──
const uint8_t DEFAULT_SENSOR_PINS[] = {34};
const uint8_t DEFAULT_PUMP_PINS[] = {25};
const uint16_t DEFAULT_THRESHOLDS[] = {40};
const uint16_t DEFAULT_DRY_VALS[] = {3200};
const uint16_t DEFAULT_WET_VALS[] = {2400};
const uint16_t DEFAULT_DURATIONS[] = {5};
const char* DEFAULT_NAMES[] = {"Daun Bawang"};
const char* DEFAULT_COLORS[] = {"#4ade80"};

// ── Network ──
#define AP_SSID "Kebun-IoT-AP"
#define AP_PASSWORD "12345678"
#define HTTP_PORT 80
#define DNS_PORT 53

// ── Static IP (set sesuai jaringan kamu) ──
#define USE_STATIC_IP true
IPAddress STATIC_IP(192, 168, 1, 11);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS1(192, 168, 1, 1);
IPAddress DNS2(8, 8, 8, 8);

// ── Timing (ms) ──
#define SENSOR_INTERVAL 2000
#define PUMP_CHECK_INTERVAL 500
#define PUMP_COOLDOWN_MS 60000
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
  unsigned long pumpOffMs;  // last time pump was turned off by timer
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
