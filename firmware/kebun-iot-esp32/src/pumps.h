#ifndef PUMPS_H
#define PUMPS_H

#include "config.h"

void initPumps();
void setPump(int idx, bool state);
void checkPumpTimers();
void autoWaterPumps();

void initPumps() {
  for (int i = 0; i < g_plantCount; i++) {
    pinMode(g_plants[i].pumpPin, OUTPUT);
    digitalWrite(g_plants[i].pumpPin, HIGH);
    g_readings[i].pumpActive = false;
    g_readings[i].pumpStartMs = 0;
    g_readings[i].pumpOffMs = 0;
  }
}

void setPump(int idx, bool state) {
  if (idx < 0 || idx >= g_plantCount) return;
  uint8_t pin = g_plants[idx].pumpPin;
  if (state) {
    digitalWrite(pin, LOW);
    g_readings[idx].pumpActive = true;
    g_readings[idx].pumpStartMs = millis();
  } else {
    digitalWrite(pin, HIGH);
    g_readings[idx].pumpActive = false;
    g_readings[idx].pumpStartMs = 0;
    g_readings[idx].pumpOffMs = millis();
  }
}

void checkPumpTimers() {
  unsigned long now = millis();
  for (int i = 0; i < g_plantCount; i++) {
    if (!g_readings[i].pumpActive) continue;
    unsigned long elapsed = now - g_readings[i].pumpStartMs;
    uint32_t durationMs = (uint32_t)g_plants[i].pumpDuration * 1000UL;
    if (elapsed >= durationMs) {
      setPump(i, false);
    }
  }
}

void autoWaterPumps() {
  unsigned long now = millis();
  for (int i = 0; i < g_plantCount; i++) {
    if (g_readings[i].pumpActive) continue;
    if (now - g_readings[i].pumpOffMs < PUMP_COOLDOWN_MS) continue;
    if (g_readings[i].moisture < g_plants[i].threshold) {
      setPump(i, true);
    }
  }
}

#endif
