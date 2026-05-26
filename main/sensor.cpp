/**
 * sensor.cpp
 * ----------
 * Implementation of the DHT20 sensor interface.
 * Requires: Adafruit DHT20 library (install via Library Manager).
 */

#include "config.h"
#include "sensor.h"
#include <Adafruit_BMP3XX.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>

#define SEALEVELPRESSURE_HPA (1013.25)

static Adafruit_BMP3XX bmp;
static Adafruit_AHTX0 dht20;

bool initSensor() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    
    if (!dht20.begin()) {
        Serial.println("[sensor] ERROR: DHT20 not found. Check wiring.");
        return false;
    }
    
    if (!bmp.begin_I2C()) {
        Serial.println("Could not find a valid BMP3 sensor, check wiring!");
        return false;
    }
    
    // Set up oversampling and filter initialization
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_25_HZ);
    
    Serial.println("[sensor] DHT20 Ready.");
    Serial.println("[sensor] BMP390 Ready.");
    return true;
}

bool sanityCheck(const DhtReading& reading) {

    bool tempOK = reading.temperatureC >= -40.0f 
                && reading.temperatureC <= 80.0f;

    bool humOK = reading.humidity >= 0.0f
                && reading.humidity <= 100.0f;

    if(!tempOK || !humOK) {
        Serial.println("[sensor] WARNING: Reading out of range, discarding.");
        return false;
    }

    return true;
}

DhtReading readDht() {
    DhtReading result = {0.0f, 0.0f, 0.0f, false};
    sensors_event_t humidity, temperature;          // 
    dht20.getEvent(&humidity, &temperature);
    
    result.temperatureC = temperature.temperature;
    result.temperatureF = result.temperatureC * 9.0f / 5.0f + 32.0f;
    result.humidity     = humidity.relative_humidity;

    // Testing whether readings are within expected range
    if (!sanityCheck(result)) {
        return result;
    }

    result.valid        = true;
    return result;
}

BmpReading readBmp() {
    BmpReading result = {0.0f, 0.0f, false};
    
    if (!bmp.performReading()) {
        Serial.println("[sensor] BMP reading failed.");
        return result;
    }
    
    result.pressure     = bmp.pressure / 100.0f;
    result.altitudeM    = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    result.valid        = true;
    
    return result;
}

void printDhtReading(const DhtReading& reading) {
    if (!reading.valid) {
        Serial.println("[Dht Reading] No valid reading to print.");
        return;
    }
    
    Serial.printf("[DHT20] Temp: %.1f °C / %.1f °F  |  Humidity: %.1f %% |  \n",
        reading.temperatureC,
        reading.temperatureF,
        reading.humidity);
    }
    
void printBmpReading(const BmpReading& reading) {
    if (!reading.valid) {
        Serial.println("[Bmp Reading] No valid reading to print.");
        return;
    }
    
    Serial.printf("[BMP390] Pressure: %.1f hPa  |  Altitude: %.1f m\n",
        reading.pressure,
        reading.altitudeM);
    }
    