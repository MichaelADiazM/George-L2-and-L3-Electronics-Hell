/**
 * config.h
 * --------
 * Central configuration. Edit this file only — nothing else needs touching
 * for basic setup changes.
 */


#pragma once
#include <Arduino.h> 

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define NETWORK_SSID   "networkname"
#define NETWORK_PASS   "password"
#define AP_SSID        "rocketfinder"
#define AP_PASS        "findmefather"
#define WIFI_TIMEOUT_MS 10000

// ── Sensor ───────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN 28
#define I2C_SCL_PIN 27

#define MISO_PIN 2
#define MOSI_PIN 7
#define SCK_PIN 6
#define RFM69_CS_PIN 5
#define RFM69_INT_PIN 8
#define SD_CS_PIN 4

// Poll interval: BMP390 output data rate at current OSR settings
#define BMP_READ_INTERVAL_MS 20

// Threshold for AP shutoff according to altitude
#define AP_SHUTOFF_ALTITUDE_M 0

// ── Radio ───────────────────────────────────────────────────────────────────
constexpr float RFM69_FREQUENCY_MHZ = 915.0f;

// ── Server ───────────────────────────────────────────────────────────────────
#define HTTP_PORT 8080
