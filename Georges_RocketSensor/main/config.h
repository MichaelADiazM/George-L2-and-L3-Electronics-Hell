/**
 * config.h
 * --------
 * Central configuration. Edit this file only — nothing else needs touching
 * for basic setup changes.
 */

#include <Arduino.h> 

#pragma once

// ── WiFi ─────────────────────────────────────────────────────────────────────
#define NETWORK_SSID "network_name";
#define NETWORK_PASS "password";

// ── Sensor ───────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Defines how often the sensor is checked
#define READ_INTERVAL_MS 2000

// ── Server ───────────────────────────────────────────────────────────────────
#define HTTP_PORT 8080
