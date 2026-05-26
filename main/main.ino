/**
 * main.ino
 * --------
 * ESP32 + DHT20 + BMP390 sensor with HTTP server.
 *
 * Dependencies (install via Arduino Library Manager):
 *   - Adafruit AHTX0  (covers the DHT20 / AHT20 family)
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

unsigned long lastDHTReadAt = 0;
unsigned long lastBMPReadAt = 0;

//Kickstarts the sensor and the server, checks whether they're working
void setup() {
  Serial.begin(9600);
  Serial.println("\n[main] Starting...");

  //Pauses program if sensor isn't initialized
  if (!initSensor()) {
    Serial.println("\n[main: sensor] Sensor init failed. Check wiring and reset.");
    while (true) delay(1000);
  }

  initServer();
}


void loop() {
  handleClients(); //Keeps server active

  unsigned long now = millis();

  if (now - lastBMPReadAt >= BMP_READ_INTERVAL_MS) {
    lastBMPReadAt = now;

    BmpReading data = readBmp();
    printBmpReading(data);
    updateBMPReading(data);
  }

  if (now - lastDHTReadAt >= DHT_READ_INTERVAL_MS) {
    lastDHTReadAt = now;

    DhtReading data = readDht();
    printDhtReading(data);
    updateDHTReading(data);
  }
}  