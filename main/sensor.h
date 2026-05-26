/**
 * sensor.h
 * --------
 * DHT20 sensor interface. Owns all I2C communication with the sensor.
 * The rest of the codebase only calls these three functions.
 */

#pragma once

#include <stdbool.h>

// Holds a single reading from the DHT20
struct DhtReading {
    float temperatureC;
    float temperatureF;
    float humidity;
    bool valid;
};

//Holds a single reading from the BMP390
struct BmpReading {
    float pressure;
    float altitudeM;
    bool valid;
};

BmpReading readBmp();
DhtReading readDht();

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

/**
 * printDhtReading()
 * Formats and prints a dht reading to Serial.
 * No-op if reading.valid is false.
 */
void printDhtReading(const DhtReading& r);

/**
 * sanityCheck()
 * Verifies whether the readings from the DHT20 are within expected range
 */
bool sanityCheck(DhtReading temp, DhtReading hum);