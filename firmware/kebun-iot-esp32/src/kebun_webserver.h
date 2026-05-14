#ifndef KEBUN_WEBSERVER_H
#define KEBUN_WEBSERVER_H

#include <WebServer.h>

#include "config.h"
#include "sensors.h"
#include "pumps.h"
#include "wifimanager.h"
#include <LittleFS.h>

WebServer server(HTTP_PORT);

// ── Forward declarations ──
void handleRoot();
void handleData();
void handlePump();
void handleConfigGet();
void handleConfigPost();
void handleConfigSync();
void handleWiFiPost();
void handleReset();
void handleSensorRead();
void handleNotFound();
void sendCORS();

// ── Helpers ──
void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJSON(int code, const String& json) {
  sendCORS();
  server.send(code, "application/json", json);
}

// ── Routes ──
void handleRoot() {
  if (g_apMode) {
    // Serve captive portal page
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server.send(200, "text/html", "<html><body><h1>Kebun IoT Config</h1><p>Portal page not found. Upload LittleFS data.</p></body></html>");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"sta\",\"ip\":\"" + WiFi.localIP().toString() + "\"}");
  }
}

void handleData() {
  // Force a fresh sensor read
  readAllSensors();

  String json = "{";
  json += "\"temp\":" + String(g_temp, 1) + ",";
  json += "\"humidity_air\":" + String(g_humidity, 0) + ",";
  json += "\"plants\":[";
  for (int i = 0; i < g_plantCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(g_plants[i].id) + ",";
    json += "\"name\":\"" + String(g_plants[i].name) + "\",";
    json += "\"moisture\":" + String(g_readings[i].moisture) + ",";
    json += "\"threshold\":" + String(g_plants[i].threshold) + ",";
    json += "\"temperature\":" + String(g_temp, 1) + ",";
    json += "\"pump\":" + String(g_readings[i].pumpActive ? "true" : "false");
    json += "}";
  }
  json += "]}";
  sendJSON(200, json);
}

void handlePump() {
  if (server.method() != HTTP_POST && server.method() != HTTP_OPTIONS) {
    sendJSON(405, "{\"error\":\"POST required\"}");
    return;
  }
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  int id = server.arg("id").toInt();
  bool state = server.arg("state") == "1";

  // Find plant index by ID
  int idx = -1;
  for (int i = 0; i < g_plantCount; i++) {
    if (g_plants[i].id == id) { idx = i; break; }
  }

  if (idx < 0) {
    sendJSON(404, "{\"error\":\"plant not found\"}");
    return;
  }

  setPump(idx, state);

  String json = "{\"ok\":true,\"id\":" + String(id) + ",\"state\":" + String(state ? "true" : "false") + "}";
  sendJSON(200, json);
}

void handleConfigGet() {
  String json = "{\"mode\":\"" + String(g_apMode ? "ap" : "sta") + "\",";
  json += "\"ip\":\"" + String(g_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"ssid\":\"" + prefs.getString(KEY_SSID, "") + "\",";
  json += "\"plants\":[";
  for (int i = 0; i < g_plantCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(g_plants[i].id) + ",";
    json += "\"name\":\"" + String(g_plants[i].name) + "\",";
    json += "\"sensor\":" + String(g_plants[i].sensorPin) + ",";
    json += "\"pump\":" + String(g_plants[i].pumpPin) + ",";
    json += "\"threshold\":" + String(g_plants[i].threshold) + ",";
    json += "\"dry\":" + String(g_plants[i].dryValue) + ",";
    json += "\"wet\":" + String(g_plants[i].wetValue) + ",";
    json += "\"duration\":" + String(g_plants[i].pumpDuration) + ",";
    json += "\"color\":\"" + String(g_plants[i].color) + "\"";
    json += "}";
  }
  json += "]}";
  sendJSON(200, json);
}

void handleConfigPost() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  // Parse URL-encoded body: key=value&key=value...
  // Support: id=N&key=value&key=value...
  // Or: plant=IDX&name=X&sensor=X&pump=X...
  int id = server.arg("id").toInt();
  if (id <= 0) {
    sendJSON(400, "{\"error\":\"id required\"}");
    return;
  }

  int idx = -1;
  for (int i = 0; i < g_plantCount; i++) {
    if (g_plants[i].id == id) { idx = i; break; }
  }
  if (idx < 0) {
    sendJSON(404, "{\"error\":\"plant not found\"}");
    return;
  }

  // Update fields
  for (int a = 0; a < server.args(); a++) {
    String key = server.argName(a);
    String val = server.arg(a);
    // Skip 'id' as it's the identifier
    if (key == "id") continue;
    updatePlantFromJson(idx, key, val);
  }

  sendJSON(200, "{\"ok\":true}");
}

void handleWiFiPost() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("password") ? server.arg("password") : "";

  // Also accept URL-encoded body
  if (ssid.length() == 0 && server.hasArg("plain")) {
    String body = server.arg("plain");
    // Try JSON parse
    int s = body.indexOf("\"ssid\":\"");
    if (s >= 0) {
      s += 8;
      int e = body.indexOf("\"", s);
      if (e >= 0) ssid = body.substring(s, e);
    }
    s = body.indexOf("\"password\":\"");
    if (s >= 0) {
      s += 12;
      int e = body.indexOf("\"", s);
      if (e >= 0) pass = body.substring(s, e);
    }
  }

  if (ssid.length() == 0) {
    sendJSON(400, "{\"error\":\"ssid required\"}");
    return;
  }

  String json = "{\"ok\":true,\"ssid\":\"" + ssid + "\",\"message\":\"Credentials saved. Restarting...\"}";
  sendJSON(200, json);

  // Flush response before restart
  server.client().flush();
  delay(100);
  saveWiFiAndRestart(ssid, pass);
}

void handleConfigSync() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  String body = server.arg("plain");
  if (body.length() == 0) {
    sendJSON(400, "{\"error\":\"empty body\"}");
    return;
  }

  int count = 0;
  int pos = 0;
  while (true) {
    int idPos = body.indexOf("\"id\":", pos);
    if (idPos < 0 || count >= MAX_PLANTS) break;
    pos = idPos + 1;
    count++;
  }

  if (count == 0) {
    sendJSON(400, "{\"error\":\"no plants in body\"}");
    return;
  }

  g_plantCount = count;
  pos = 0;

  for (int i = 0; i < count; i++) {
    int idPos = body.indexOf("\"id\":", pos);
    if (idPos < 0) break;

    g_plants[i].id = body.substring(idPos + 5).toInt();

    auto readInt = [&](const char* key) -> int {
      int p = body.indexOf(key, idPos);
      if (p < 0) return 0;
      p += strlen(key);
      while (p < (int)body.length() && (body[p] == ':' || body[p] == ' ')) p++;
      return body.substring(p).toInt();
    };

    auto readStr = [&](const char* key, char* buf, int bufLen) {
      int p = body.indexOf(key, idPos);
      if (p < 0) return;
      p += strlen(key);
      while (p < (int)body.length() && body[p] != '\"') p++;
      p++;
      int e = body.indexOf("\"", p);
      if (e >= 0) {
        body.substring(p, e).toCharArray(buf, bufLen);
      }
    };

    int s = body.indexOf("\"name\":\"", idPos);
    if (s >= 0) {
      s += 8;
      int e = body.indexOf("\"", s);
      if (e >= 0) {
        body.substring(s, e).toCharArray(g_plants[i].name, sizeof(g_plants[i].name));
      }
    }

    g_plants[i].sensorPin = readInt("\"sensor\"");
    g_plants[i].pumpPin = readInt("\"pump\"");
    g_plants[i].threshold = readInt("\"threshold\"");
    g_plants[i].dryValue = readInt("\"dry\"");
    g_plants[i].wetValue = readInt("\"wet\"");
    g_plants[i].pumpDuration = readInt("\"duration\"");
    readStr("\"color\"", g_plants[i].color, sizeof(g_plants[i].color));

    pos = idPos + 1;
  }

  saveConfig();
  initPumps();
  sendJSON(200, "{\"ok\":true,\"count\":" + String(g_plantCount) + ",\"action\":\"synced\"}");
}

void handleReset() {
  prefs.clear();
  resetToDefaults();
  saveConfig();
  String json = "{\"ok\":true,\"message\":\"Reset to defaults. Restarting...\"}";
  sendJSON(200, json);
  server.client().flush();
  delay(100);
  ESP.restart();
}

void handleSensorRead() {
  int id = server.arg("id").toInt();
  if (id <= 0) {
    sendJSON(400, "{\"error\":\"id required\"}");
    return;
  }
  int idx = -1;
  for (int i = 0; i < g_plantCount; i++) {
    if (g_plants[i].id == id) { idx = i; break; }
  }
  if (idx < 0) {
    sendJSON(404, "{\"error\":\"plant not found\"}");
    return;
  }
  readSoilMoisture(idx);
  int raw = analogRead(g_plants[idx].sensorPin);
  String json = "{";
  json += "\"id\":" + String(id) + ",";
  json += "\"adc\":" + String(raw) + ",";
  json += "\"moisture\":" + String(g_readings[idx].moisture);
  json += "}";
  sendJSON(200, json);
}

void handleNotFound() {
  // In AP mode, serve captive portal for any request
  if (g_apMode) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString());
    server.send(302);
    return;
  }
  sendJSON(404, "{\"error\":\"not found\"}");
}

// ── Init ──
void initWebServer() {
  // CORS preflight
  server.on("/data", HTTP_OPTIONS, []() { sendCORS(); server.send(204); });
  server.on("/pump", HTTP_OPTIONS, []() { sendCORS(); server.send(204); });
  server.on("/config", HTTP_OPTIONS, []() { sendCORS(); server.send(204); });
  server.on("/config/sync", HTTP_OPTIONS, []() { sendCORS(); server.send(204); });
  server.on("/wifi", HTTP_OPTIONS, []() { sendCORS(); server.send(204); });

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/pump", handlePump);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/config", HTTP_POST, handleConfigPost);
  server.on("/config/sync", HTTP_POST, handleConfigSync);
  server.on("/wifi", HTTP_POST, handleWiFiPost);
  server.on("/sensor/read", handleSensorRead);
  server.on("/reset", handleReset);
  server.onNotFound(handleNotFound);

  server.begin();
  DEBUG_SERIAL.println("HTTP server started on port " + String(HTTP_PORT));
}

#endif
