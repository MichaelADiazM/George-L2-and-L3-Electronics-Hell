/**
 * config.h
 * --------
 * Ground station configuration. Pin assignments, RF frequency, serial baud.
 * Edit this file only for hardware changes (different pins, different board, etc).
 */

#pragma once

// ── Serial ────────────────────────────────────────────────────────────────
#define SERIAL_BAUD 115200

// ── RFM69 Radio ──────────────────────────────────────────────────────────
// Pin assignments: connect these GPIO to the RFM69HCW module
#define RFM_CS_PIN   10   // Chip Select
#define RFM_INT_PIN  9    // Interrupt (G0/DIO0)

// Frequency must match flight computer's main/config.h RFM69_FREQUENCY_MHZ
#define RFM_FREQUENCY_MHZ 915.0f
