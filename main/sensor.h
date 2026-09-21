/**
 * sensor.h
 * --------
 * BMP390 sensor interface. Owns all I2C communication with the sensor.
 * The rest of the codebase only calls these three functions.
 */

#pragma once

#include <stdbool.h>

struct BmpReading {
    float pressure;          // Atmospheric pressure in hPa
    float temperatureC;      // Temperature in Celsius
    float altitudeM;         // Calculated altitude in meters
    bool valid;              // Whether the reading succeeded
};

/**
 * readBmp()
 * Reads current temperature, pressure, and altitude from the BMP390.
 * Returns a BmpReading struct; check valid flag before using other fields.
 */
BmpReading readBmp();

/**
 * initSensor()
 * Call once in setup(). Returns true on success.
 */
bool initSensor();

/**
 * printBmpReading()
 * Formats and prints a bmp reading to Serial.
 * No-op if reading.valid is false.
 */
void printBmpReading(const BmpReading& r);