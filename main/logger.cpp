/**
 * logger.cpp
 * ----------
 * Buffered CSV logger to SD card.
 */

#include "logger.h"
#include "config.h"

bool initLogger() {
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[logger] SD card initialization failed. Check wiring.");
        return false;
    }

    bool exists = SD.exists("/log.csv");
    logFile = SD.open("/log.csv", FILE_APPEND);

    if (!logFile) {
        Serial.println("[logger] Failed to open log file.");
        return false;
    }

    if (!exists) {
        logFile.println("timestamp_ms,temperatureC,pressureHpa,altitudeM");
        logFile.flush();
    }

    return true;
}

void logReading(unsigned long timestamp, const BmpReading& reading) {
    if (bufferCount >= 25) return;
    logBuffer[bufferCount++] = { timestamp, reading.temperatureC, reading.pressureHpa, reading.altitudeM };
}

void flushLogs() {
    for (int i = 0; i < bufferCount; i++) {
        LogEntry& entry = logBuffer[i];
        logFile.printf("%lu,%.2f,%.2f,%.2f\n",
            entry.timestamp,
            entry.temperatureC,
            entry.pressureHpa,
            entry.altitudeM);
    }
    logFile.flush();
    bufferCount = 0;
}
