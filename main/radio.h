/**
 * radio.h
 * --------
 * 
 * 
 */

#pragma once

#include "sensor.h"
#include <stdint.h>

struct __attribute__((packed)) RadioPacket {
    uint32_t timestamp_ms;
    float temperatureC;
    float pressureHpa;
    float altitudeM;
};

bool initRadio();

void transmitReading(const RadioPacket& packet);