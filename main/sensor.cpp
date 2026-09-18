/**
 * sensor.cpp
 * ----------
 * Implementation of the BMP390 sensor interface.
 * Requires: Adafruit BMP390 library (install via Library Manager).
 */

#include "config.h"
#include "sensor.h"
#include <Adafruit_BMP3XX.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

#define SEALEVELPRESSURE_HPA (1013.25)

static Adafruit_BMP3XX bmp;

bool initSensor() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    if (!bmp.begin_I2C()) {
        return false;
    }
    
    // Set up oversampling and filter initialization
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_25_HZ);
    
    Serial.println("[sensor] BMP390 Ready.");
    return true;
}

BmpReading readBmp() {
    BmpReading result = {};
    
    if (!bmp.performReading()) {
        Serial.println("[sensor] BMP reading failed.");
        return result;
    }
    
    result.pressure     = bmp.pressure / 100.0f;
    result.temperatureC = bmp.temperature;
    result.altitudeM    = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    result.valid        = true;
    
    return result;
}

void printBmpReading(const BmpReading& reading) {
    if (!reading.valid) {
        Serial.println("[Bmp Reading] No valid reading to print.");
        return;
    }
    
    Serial.printf("[BMP390] Pressure: %.1f hPa  |  Temperature: %.1f C  |  Altitude: %.1f m\n",
        reading.pressure,
        reading.temperatureC,
        reading.altitudeM);
    }
    