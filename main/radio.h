/**
 * radio.h
 * --------
 * RFM69 radio interface. Transmits sensor readings wirelessly to a receiver.
 * Owns all SPI communication with the radio module.
 */

#pragma once

#include "sensor.h"
#include <stdint.h>

struct __attribute__((packed)) RadioPacket {    // Ensures no padding between fields; required for reliable radio transmission
    uint32_t timestamp_ms;                      // Milliseconds since boot. Range: [0, 2^32 - 1]
    float temperatureC;                         // Temperature in Celsius
    float pressureHpa;                          // Atmospheric pressure in hPa
    float altitudeM;                            // Calculated altitude in meters
};

/**
 * initRadio()
 * Initializes the RFM69 radio module and configures it for transmission.
 * Returns true on success.
 */
bool initRadio();

/**
 * transmitReading()
 * Sends a RadioPacket wirelessly. Called periodically from main loop (every Nth reading).
 */
void transmitReading(const RadioPacket& packet);