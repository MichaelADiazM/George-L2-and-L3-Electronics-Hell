/**
 * 
 * 
 * 
 * 
 */

#pragma once
#include "sensor.h"

bool initLogger();

void logBmp(const BmpReading& reading);

void logDht(const DhtReading& reading);

