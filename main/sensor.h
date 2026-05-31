/**
 * sensor.h
 * --------
 * DHT20 sensor interface. Owns all I2C communication with the sensor.
 * The rest of the codebase only calls these three functions.
 */

#pragma once

#include <stdbool.h>

//Holds a single reading from the BMP390
struct BmpReading {
    float pressure;
    float temperatureC;
    float altitudeM;
    bool valid;
};

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