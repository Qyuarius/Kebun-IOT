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
    digitalWrite(g_plants[i].pumpPin, HIGH);  // HIGH = OFF (active LOW relay)
    g_readings[i].pumpActive          = false;
    g_readings[i].pumpStartMs         = 0;
    g_readings[i].pumpOffMs           = millis(); // FIX: was 0 — caused instant re-trigger on boot
    g_readings[i].manualOverrideUntil = 0;
  }
}

void setPump(int idx, bool state) {
  if (idx < 0 || idx >= g_plantCount) return;

  uint8_t pin = g_plants[idx].pumpPin;

  // FIX: Ensure pin is in OUTPUT mode before writing
  pinMode(pin, OUTPUT);

  if (state) {
    // FIX: guard against zero duration — default to 5s
    if (g_plants[idx].pumpDuration == 0) {
      DEBUG_SERIAL.println("[PUMP] Warning: pumpDuration is 0, defaulting to 5s");
      g_plants[idx].pumpDuration = 5;
    }

    digitalWrite(pin, LOW);                    // LOW = ON (active LOW relay)
    g_readings[idx].pumpActive            = true;
    g_readings[idx].pumpStartMs           = millis();
    g_readings[idx].manualOverrideUntil   = 0;

    DEBUG_SERIAL.print("[PUMP ON] ");
    DEBUG_SERIAL.print(g_plants[idx].name);
    DEBUG_SERIAL.print(" — will run for ");
    DEBUG_SERIAL.print(g_plants[idx].pumpDuration);
    DEBUG_SERIAL.println("s");

  } else {
    digitalWrite(pin, HIGH);                   // HIGH = OFF
    g_readings[idx].pumpActive  = false;
    g_readings[idx].pumpStartMs = 0;
    g_readings[idx].pumpOffMs   = millis();    // record OFF time for cooldown

    DEBUG_SERIAL.print("[PUMP OFF] ");
    DEBUG_SERIAL.println(g_plants[idx].name);
  }
}

// SAFETY: Absolute maximum pump run time (seconds) - hard limit regardless of config
#define MAX_PUMP_DURATION_S 30

void checkPumpTimers() {
  unsigned long now = millis();
  for (int i = 0; i < g_plantCount; i++) {
    if (!g_readings[i].pumpActive) continue;

    // FIX: guard against zero duration
    uint16_t dur = g_plants[i].pumpDuration;
    if (dur == 0) dur = 5;

    // SAFETY: Enforce hard limit of 30 seconds max
    if (dur > MAX_PUMP_DURATION_S) dur = MAX_PUMP_DURATION_S;

    uint32_t durationMs = (uint32_t)dur * 1000UL;
    uint32_t elapsed = now - g_readings[i].pumpStartMs;

    // FIX: Handle millis() overflow and force shutoff if running too long
    if (elapsed >= durationMs || elapsed > (MAX_PUMP_DURATION_S * 1000UL)) {
      DEBUG_SERIAL.print("[PUMP TIMER] Auto-off: ");
      DEBUG_SERIAL.print(g_plants[i].name);
      DEBUG_SERIAL.print(" (ran for ");
      DEBUG_SERIAL.print(elapsed / 1000);
      DEBUG_SERIAL.println("s)");
      setPump(i, false);
    }
  }
}

void autoWaterPumps() {
  unsigned long now = millis();
  for (int i = 0; i < g_plantCount; i++) {
    if (g_readings[i].pumpActive)                          continue; // already running
    if (g_readings[i].manualOverrideUntil > now)           continue; // manual override active
    if (now - g_readings[i].pumpOffMs < PUMP_COOLDOWN_MS) continue; // cooldown not elapsed

    if (g_readings[i].moisture < g_plants[i].threshold) {
      DEBUG_SERIAL.print("[AUTO WATER] ");
      DEBUG_SERIAL.print(g_plants[i].name);
      DEBUG_SERIAL.print(" moisture=");
      DEBUG_SERIAL.print(g_readings[i].moisture);
      DEBUG_SERIAL.print("% < threshold=");
      DEBUG_SERIAL.print(g_plants[i].threshold);
      DEBUG_SERIAL.println("%");
      setPump(i, true);
    }
  }
}

#endif