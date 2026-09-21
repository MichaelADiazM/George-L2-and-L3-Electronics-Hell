/**
 * server.h
 * --------
 * HTTP server interface. Manages WiFi connection and serves sensor data.
 *
 * Routes:
 *   GET /       → HTML page with live readings (auto-refreshes every 5 s)
 *   GET /data   → JSON: { "tempC": 23.1, "tempF": 73.6, "humidity": 55.0, "pressure": 1013.25 }
 */

#pragma once

#include "sensor.h"

/**
 * initServer()
 * Connects to WiFi and starts the HTTP server.
 */
bool initServer();


/**
 * updateBMPReading()
 * Pushes the latest BmpReading object values into the server so the data stays current.
 * Call this every time you take a new reading
 */
void updateBMPReading(const BmpReading& reading);

/**
 * handleClients()
 * Must be called on every loop() iteration to process incoming requests.
 */
void handleClients();