/**
 * logger.h
 * --------
 * 
 * 
 */

#pragma once

#include "sensor.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>

struct LogEntry {
    unsigned long timestamp;
    float temperatureC;
    float pressureHpa;
    float altitudeM;
};

static LogEntry logBuffer[25];
static int bufferCount = 0;
static File logFile;

bool initLogger();

void logReading(unsigned long timestamp, const BmpReading& reading);

void flushLogs();

