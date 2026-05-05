/**
 * sensor.h
 * --------
 * DHT20 sensor interface. Owns all I2C communication with the sensor.
 * The rest of the codebase only calls these three functions.
 */

#pragma once

#include <stdbool.h>

//Holds a single reading from the DHT20
struct SensorReading {
    float temperatureC;
    float temperatureF;
    float humidity;
    bool valid;
};

/**
 * initSensor()
 * Call once in setup(). Returns true on success.
 */
bool initSensor();

/**
 * readSensor()
 * Returns the latest temperature and humidity.
 * Check reading.valid before trusting the values.
 */
SensorReading readSensor();

/**
 * printReading()
 * Formats and prints a reading to Serial.
 * No-op if reading.valid is false.
 */
void printReading(const SensorReading& reading);