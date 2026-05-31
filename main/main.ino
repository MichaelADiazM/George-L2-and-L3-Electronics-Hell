/**
 * main.ino
 * --------
 * ESP32 + BMP390 sensor with HTTP server.
 *
 * Dependencies (install via Arduino Library Manager):
 *   - Adafruit Unified Sensor
 *   - Adafruit BMP3XX
 * 
 * File structure:
 *   main.ino   ← orchestration
 *   config.h   ← WiFi credentials, pins, timing
 *   sensor.h/.cpp ← DHT20 + BMP390 read logic
 *   server.h/.cpp  ← HTTP server
 **/
 
#include "config.h"
#include "sensor.h"
#include "server.h"
#include "logger.h"
#include "radio.h"

static unsigned long lastBMPReadAt  = 0;
static unsigned long lastLogFlushAt = 0;
static int           readCount      = 0;

//Kickstarts the sensor and the server, checks whether they're working
void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] Starting...");

  //Pauses program if sensor isn't initialized
  if (!initSensor()) {
    Serial.println("\n[main: sensor] Sensor init failed. Check wiring and reset.");
    while (true) delay(1000);
  }
  if (!initServer()) {
    Serial.println("\n[main: server] Server init failed. Check WiFi credentials and reset.");
    while (true) delay(1000);
  }
  if (!initLogger()) {
    Serial.println("\n[main: logger] Logger init failed. Check SD card wiring and reset.");
    while (true) delay(1000);
  }
  if (!initRadio()) {
    Serial.println("\n[main: radio] Radio init failed. Check wiring and reset.");
    while (true) delay(1000);
  }
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