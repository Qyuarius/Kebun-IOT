#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

void initSensors();
void readAllSensors();

// ADC reference voltage helper
#define ADC_MAX 4095

void initSensors() {
  for (int i = 0; i < g_plantCount; i++) {
    pinMode(g_plants[i].sensorPin, INPUT);
    g_readings[i].moisture = 50;
    g_readings[i].pumpActive = false;
  }
}

// Map raw ADC value to moisture percentage
// dryValue = ADC reading in dry air (e.g. 3500)
// wetValue = ADC reading in water (e.g. 1200)
// Capacitive sensors: drier soil = higher ADC value
uint16_t adcToPercent(uint16_t raw, uint16_t dryVal, uint16_t wetVal) {
  if (raw <= wetVal) return 100;
  if (raw >= dryVal) return 0;
  if (dryVal <= wetVal) return 50; // safety: invalid cal
  return (uint16_t)(100UL * (dryVal - raw) / (dryVal - wetVal));
}

void readSoilMoisture(int idx) {
  PlantConfig& cfg = g_plants[idx];
  int raw = analogRead(cfg.sensorPin);
  raw = constrain(raw, 0, ADC_MAX);
  g_readings[idx].moisture = adcToPercent(raw, cfg.dryValue, cfg.wetValue);
}

void readAllSensors() {
  for (int i = 0; i < g_plantCount; i++) {
    readSoilMoisture(i);
    DEBUG_SERIAL.print("[SENSOR] Plant ");
    DEBUG_SERIAL.print(g_plants[i].id);
    DEBUG_SERIAL.print(" (");
    DEBUG_SERIAL.print(g_plants[i].name);
    DEBUG_SERIAL.print(") | ADC: ");
    DEBUG_SERIAL.print(analogRead(g_plants[i].sensorPin));
    DEBUG_SERIAL.print(" | Moisture: ");
    DEBUG_SERIAL.print(g_readings[i].moisture);
    DEBUG_SERIAL.println("%");
  }
}

#endif
