/**
 * radio_packet.h
 * ----
 * Telemetry packet struct: MUST match the flight computer's definition exactly.
 *
 * Source of truth: C:\Users\diazm\OneDrive\Documents\Arduino\Georges_RocketSensor\main\radio.h
 * If the flight computer's RadioPacket changes, this file must be updated in sync.
 */

#pragma once
#include <stdint.h>

/**
 * Sensor reading packet transmitted by the flight computer via RFM69.
 * Packed struct: exactly 16 bytes, no padding, little-endian on both ESP32 and RISC-V (ESP32-C61).
 */
struct __attribute__((packed)) RadioPacket {
  uint32_t timestamp_ms;   // Milliseconds since flight computer boot
  float    temperatureC;   // Celsius
  float    pressureHpa;    // Pressure in hPa
  float    altitudeM;      // Altitude above sea level in meters
};

// Guard: if this fires, the struct is not 16 bytes and the wire protocol will break
static_assert(sizeof(RadioPacket) == 16, "RadioPacket must be exactly 16 bytes (4+4+4+4) for wire format");
