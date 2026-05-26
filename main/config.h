/**
 * config.h
 * --------
 * Central configuration. Edit this file only — nothing else needs touching
 * for basic setup changes.
 */


#pragma once
#include <Arduino.h> 

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define NETWORK_SSID   "network_name"
#define NETWORK_PASS   "password"
#define AP_SSID        "George's Rocket Finder"
#define AP_PASS        "FINDMEFATHER!"
#define WIFI_TIMEOUT_MS 10000

// ── Sensor ───────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Poll interval: DHT20 needs ≥1 s between reads (datasheet §4.4)
#define DHT_READ_INTERVAL_MS 2000
// Poll interval: BMP390 output data rate at current OSR settings
#define BMP_READ_INTERVAL_MS 20

// ── Server ───────────────────────────────────────────────────────────────────
#define HTTP_PORT 8080
