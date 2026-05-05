/**
 * sensor.cpp
 * ----------
 * Implementation of the DHT20 sensor interface.
 * Requires: Adafruit DHT20 library (install via Library Manager).
 */

#include "config.h"
#include "sensor.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

static Adafruit_AHTX0 dht20;

bool initSensor() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!dht20.begin()) {
        Serial.println("[sensor] ERROR: DHT20 not found. Check wiring.");
        return false;
    }

    Serial.println("[sensor] DHT20 Ready.");
    return true;
}

SensorReading readSensor() {
    SensorReading result = {0.0f, 0.0f, 0.0f, false};
    sensors_event_t humidity, temperature; 
    dht20.getEvent(&humidity, &temperature);

    Serial.printf("[debug] Raw temp: %.2f  Raw humidity: %.2f\n",
    temperature.temperature,
    humidity.relative_humidity);

    //Sanity check
    bool tempOK = temperature.temperature >= -40.0f 
                && temperature.temperature <= 80.0f;

    bool humOK = humidity.relative_humidity >= 0.0f
                && humidity.relative_humidity <= 100.0f;

    if(!tempOK || !humOK) {
        Serial.println("[sensor] WARNING: Reading out of range, discarding.");
        return result;
    }

    result.temperatureC = temperature.temperature;
    result.temperatureF = result.temperatureC * 9.0f / 5.0f + 32.0f;
    result.humidity     = humidity.relative_humidity;
    result.valid        = true;

    return result;
}


void printReading(const SensorReading &reading) {
   if (!reading.valid) {
    Serial.println("[sensor] No valid reading to print.");
    return;
   }

   Serial.printf("[sensor] Temp: %.1f °C / %.1f °F  |  Humidity: %.1f %%\n",
    reading.temperatureC,
    reading.temperatureF,
    reading.humidity);
}
