#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>

Preferences prefs;
DNSServer dnsServer;
bool g_wifiConnected = false;
bool g_apMode = false;

// ── Preferences keys ──
#define PREF_NAMESPACE "kebun"
#define KEY_SSID "ssid"
#define KEY_PASS "pass"
#define KEY_CONFIG "config"

// ── Save / Load ──
void saveConfig() {
  String json = "{\"plants\":[";
  for (int i = 0; i < g_plantCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(g_plants[i].id) + ",";
    json += "\"name\":\"" + String(g_plants[i].name) + "\",";
    json += "\"sensor\":" + String(g_plants[i].sensorPin) + ",";
    json += "\"pump\":" + String(g_plants[i].pumpPin) + ",";
    json += "\"thr\":" + String(g_plants[i].threshold) + ",";
    json += "\"dry\":" + String(g_plants[i].dryValue) + ",";
    json += "\"wet\":" + String(g_plants[i].wetValue) + ",";
    json += "\"dur\":" + String(g_plants[i].pumpDuration) + ",";
    json += "\"color\":\"" + String(g_plants[i].color) + "\"";
    json += "}";
  }
  json += "]}";
  prefs.putString(KEY_CONFIG, json);
}

bool loadConfig() {
  String json = prefs.getString(KEY_CONFIG, "");
  if (json.length() == 0) return false;

  // Simple JSON parser without ArduinoJson dependency
  // Look for "id":N patterns
  int idx = 0;
  int pos = 0;
  while (true) {
    int idPos = json.indexOf("\"id\":", pos);
    if (idPos < 0 || idx >= MAX_PLANTS) break;

    g_plants[idx].id = json.substring(idPos + 5).toInt();

    int s = json.indexOf("\"name\":\"", idPos);
    if (s >= 0) {
      s += 8;
      int e = json.indexOf("\"", s);
      if (e >= 0) {
        String n = json.substring(s, e);
        n.toCharArray(g_plants[idx].name, sizeof(g_plants[idx].name));
      }
    }

    auto readInt = [&](const char* key) -> int {
      int p = json.indexOf(key, idPos);
      if (p < 0) return 0;
      p += strlen(key);
      while (p < (int)json.length() && (json[p] == ':' || json[p] == ' ')) p++;
      return json.substring(p).toInt();
    };

    auto readStr = [&](const char* key, char* buf, int bufLen) {
      int p = json.indexOf(key, idPos);
      if (p < 0) return;
      p += strlen(key);
      while (p < (int)json.length() && json[p] != '\"') p++;
      p++;
      int e = json.indexOf("\"", p);
      if (e >= 0) {
        json.substring(p, e).toCharArray(buf, bufLen);
      }
    };

    g_plants[idx].sensorPin = readInt("\"sensor\"");
    g_plants[idx].pumpPin = readInt("\"pump\"");
    g_plants[idx].threshold = readInt("\"thr\"");
    g_plants[idx].dryValue = readInt("\"dry\"");
    g_plants[idx].wetValue = readInt("\"wet\"");
    g_plants[idx].pumpDuration = readInt("\"dur\"");
    readStr("\"color\"", g_plants[idx].color, sizeof(g_plants[idx].color));

    pos = idPos + 1;
    idx++;
  }

  g_plantCount = idx;
  return idx > 0;
}

void resetToDefaults() {
  g_plantCount = DEFAULT_PLANT_COUNT;
  for (int i = 0; i < DEFAULT_PLANT_COUNT; i++) {
    g_plants[i].id = i + 1;
    strncpy(g_plants[i].name, DEFAULT_NAMES[i], sizeof(g_plants[i].name) - 1);
    g_plants[i].name[sizeof(g_plants[i].name) - 1] = '\0';
    g_plants[i].sensorPin = DEFAULT_SENSOR_PINS[i];
    g_plants[i].pumpPin = DEFAULT_PUMP_PINS[i];
    g_plants[i].threshold = DEFAULT_THRESHOLDS[i];
    g_plants[i].dryValue = DEFAULT_DRY_VALS[i];
    g_plants[i].wetValue = DEFAULT_WET_VALS[i];
    g_plants[i].pumpDuration = DEFAULT_DURATIONS[i];
    strncpy(g_plants[i].color, DEFAULT_COLORS[i], sizeof(g_plants[i].color) - 1);
    g_plants[i].color[sizeof(g_plants[i].color) - 1] = '\0';
  }
}

// ── WiFi connection ──
bool connectSTA(const char* ssid, const char* pass) {
  DEBUG_SERIAL.print("Connecting to STA: ");
  DEBUG_SERIAL.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    DEBUG_SERIAL.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_SERIAL.println("\nConnected! IP: " + WiFi.localIP().toString());
    g_wifiConnected = true;
    g_apMode = false;
    return true;
  }

  DEBUG_SERIAL.println("\nSTA connection failed");
  g_wifiConnected = false;
  return false;
}

void startAP() {
  DEBUG_SERIAL.println("Starting AP mode: " + String(AP_SSID));
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  DEBUG_SERIAL.println("AP IP: " + WiFi.softAPIP().toString());

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  g_apMode = true;
  g_wifiConnected = false;
}

void initWiFi() {
  prefs.begin(PREF_NAMESPACE, false);

  String ssid = prefs.getString(KEY_SSID, "");
  String pass = prefs.getString(KEY_PASS, "");

  if (!loadConfig()) {
    DEBUG_SERIAL.println("No saved config, loading defaults");
    resetToDefaults();
    saveConfig();
  }

  if (ssid.length() > 0) {
    if (connectSTA(ssid.c_str(), pass.c_str())) {
      return;
    }
  }

  startAP();
}

// ── Captive portal processing ──
void processDNS() {
  if (g_apMode) {
    dnsServer.processNextRequest();
  }
}

// ── Handle WiFi config from HTTP API ──
void saveWiFiAndRestart(const String& ssid, const String& pass) {
  prefs.putString(KEY_SSID, ssid);
  prefs.putString(KEY_PASS, pass);
  DEBUG_SERIAL.println("WiFi saved, restarting...");
  delay(500);
  ESP.restart();
}

// ── Handle plant config from HTTP API ──
void updatePlantFromJson(int idx, const String& key, const String& value) {
  if (idx < 0 || idx >= g_plantCount) return;
  if (key == "name") {
    value.toCharArray(g_plants[idx].name, sizeof(g_plants[idx].name));
  } else if (key == "sensor") {
    g_plants[idx].sensorPin = value.toInt();
  } else if (key == "pump") {
    g_plants[idx].pumpPin = value.toInt();
  } else if (key == "threshold" || key == "thr") {
    g_plants[idx].threshold = value.toInt();
  } else if (key == "dry") {
    g_plants[idx].dryValue = value.toInt();
  } else if (key == "wet") {
    g_plants[idx].wetValue = value.toInt();
  } else if (key == "duration" || key == "dur") {
    g_plants[idx].pumpDuration = value.toInt();
  } else if (key == "color") {
    value.toCharArray(g_plants[idx].color, sizeof(g_plants[idx].color));
  }
  saveConfig();
}

#endif
