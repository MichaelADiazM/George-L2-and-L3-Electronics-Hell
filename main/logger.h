/**
 * logger.h
 * --------
 * SD card logging interface. Buffers sensor readings and flushes them to the SD card at regular intervals.
 * Owns all filesystem communication.
 */

#pragma once

#include "sensor.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>

struct LogEntry {
    unsigned long timestamp;  // Milliseconds since boot
    float temperatureC;       // Temperature in Celsius
    float pressureHpa;        // Atmospheric pressure in hPa
    float altitudeM;          // Calculated altitude in meters
};

/**
 * initLogger()
 * Initializes SD card and creates or opens the log file.
 * Returns true on success.
 */
bool initLogger();

/**
 * logReading()
 * Buffers a sensor reading in memory. Call flushLogs() periodically to write to SD card.
 */
void logReading(unsigned long timestamp, const BmpReading& reading);

/**
 * flushLogs()
 * Writes all buffered readings to SD card and clears the buffer.
 */
void flushLogs();

