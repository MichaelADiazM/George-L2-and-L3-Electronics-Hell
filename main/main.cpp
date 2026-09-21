/**
 * main.cpp
 * --------
 * ESP32 + BMP390 + RFM69 sensor setup with HTTP server for wireless logging and SD port for local logging.
 *
 * Dependencies (install via Arduino Library Manager):
 *   - Adafruit Unified Sensor
 *   - Adafruit BMP3XX
 *   - RadioHead
 *   - ESP32 board support package
 *   - SD
 *   - SPI
 *   - WiFi
 *   - ESPmDNS
 *   - WebServer
 * 
 * File structure:
 *   main.cpp       ← Orchestration (delegates to sensor, server, logger, and radio)
 *   config.h       ← Configuration constants (pins, WiFi credentials, etc.)
 *   sensor.h/.cpp  ← BMP390 read logic
 *   server.h/.cpp  ← HTTP server
 *   logger.h/.cpp  ← SD card logging
 *   radio.h/.cpp   ← RFM69 radio transmission
 **/
 
#include "config.h"
#include "sensor.h"
#include "server.h"
#include "logger.h"
#include "radio.h"

static unsigned long lastBMPReadAt  = 0;
static unsigned long lastLogFlushAt = 0;
static int           readCount      = 0;

// I'm avoiding code duplication by factoring the prints and delays.
void initModule(bool (*initFunc)(), const char* module, const char* hint) {
  if (!initFunc()) {
    Serial.printf("\n[main: %s] Init failed. %s\n", module, hint);
    while (true) delay(1000);
  }
}

//Kickstarts the sensor and the server, checks whether they're working
void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] Starting...");

  //Pauses program if  isn't initialized
  initModule(initSensor, "sensor", "Check wiring and reset.");
  // initModule(initServer, "server", "Check WiFi credentials and reset.");
  initModule(initLogger, "logger", "Check SD card wiring and reset.");
  initModule(initRadio,  "radio",  "Check wiring and reset.");
}


void loop() {
  handleClients(); //Keeps server active

  unsigned long now = millis();

  if (now - lastBMPReadAt >= BMP_READ_INTERVAL_MS) {
    lastBMPReadAt = now;

    BmpReading data = readBmp();
    printBmpReading(data);
    if (data.valid) {
      logReading(now, data);
      updateBMPReading(data);

      if (++readCount % 2 == 0) {
        RadioPacket packet = {
          .timestamp_ms = now,
          .temperatureC = data.temperatureC,
          .pressureHpa  = data.pressure,
          .altitudeM    = data.altitudeM
        };
        transmitReading(packet);
      }
    }

    if (now - lastLogFlushAt >= 1000) { //Flush logs every 1s
      lastLogFlushAt = now;
      flushLogs();
    }
  }
}