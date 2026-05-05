/**
 * main.ino
 * --------
 * ESP32 + DHT20 sensor with HTTP server.
 *
 * Dependencies (install via Arduino Library Manager):
 *   - Adafruit AHTX0  (covers the DHT20 / AHT20 family)
 *   - Adafruit Unified Sensor
 *
 * File structure:
 *   main.ino   ← orchestration
 *   config.h   ← WiFi credentials, pins, timing
 *   sensor.h / sensor.cpp  ← DHT20 read logic
 *   server.h / server.cpp  ← HTTP server + WiFi
 */
 
 //Compiler will take these as if they were written here
 #include "config.h"
 #include "sensor.h"

//Gives an internal timeline for the program
unsigned long lastReadAt = 0;

//Kickstarts the sensor and the server, checks whether they're working
void setup() {
  Serial.begin(9600);
  Serial.println("\n[main] Starting program...");

  //Pauses program if sensor isn't initialized
  if (!initSensor()) {
    Serial.println("\n[main] Make sure the wires are well connected, then reset");
    while (true) delay(1000); 
  }

  // initServer();
  // Serial.println("Server initialized");
}


void loop() {
  // handleClients(); //Keeps server active

  unsigned long now = millis();
  if (now - lastReadAt >= READ_INTERVAL_MS) {
    lastReadAt = now;

    SensorReading reading = readSensor();
    printReading(reading);
    // updateServerReading(reading);
  }
}
