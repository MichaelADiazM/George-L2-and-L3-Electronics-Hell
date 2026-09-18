/**
 * ground_station_rx.ino
 * ----
 * Real-time telemetry ground station: receives RFM69 radio packets transmitted
 * by the flight computer during flight and outputs them as NDJSON over serial.
 *
 * Designed to pair with the flight computer at:
 * C:\Users\diazm\OneDrive\Documents\Arduino\Georges_RocketSensor\main\
 *
 * RF config MUST match main/radio.cpp exactly:
 *   - Frequency: 915.0 MHz
 *   - Modem: RH_RF69::GFSK_Rb4_8Fd9_6 (4.8 kbps, 9.6 kHz Fdev)
 *   - No encryption, no addressing (raw RH_RF69 driver)
 *
 * Serial output (115200 baud): NDJSON lines
 *   Telemetry: {"type":"telemetry","seq":1234,"rx_ms":456789,"t_ms":123456,"tempC":22.53,"pressHpa":1013.25,"altM":125.02,"rssi":-67}
 *   Heartbeat: {"type":"heartbeat","rx_ms":456789}
 *   Boot:      {"type":"boot","freq_mhz":915.0,"modem":"GFSK_Rb4_8Fd9_6"}
 *   Error:     {"type":"error","seq":-1,"rx_ms":456789,"reason":"bad_length"}
 */

#include <SPI.h>
#include <RH_RF69.h>
#include "config.h"
#include "radio_packet.h"

// RadioHead driver instance
static RH_RF69 rf(RFM_CS_PIN, RFM_INT_PIN);

// Session state
static uint16_t packet_seq = 0;
static unsigned long last_heartbeat_ms = 0;
static const unsigned long HEARTBEAT_INTERVAL_MS = 5000;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);

  // Init SPI and radio
  SPI.begin();
  if (!rf.init()) {
    Serial.println(F("{\"type\":\"error\",\"reason\":\"rf_init_failed\"}"));
    while (true) delay(1000);
  }

  // RF config MUST match flight computer exactly
  if (!rf.setFrequency(RFM_FREQUENCY_MHZ)) {
    Serial.println(F("{\"type\":\"error\",\"reason\":\"setFrequency_failed\"}"));
    while (true) delay(1000);
  }

  // Critical: modem config override (library default is 250kbps; we use 4.8kbps)
  rf.setModemConfig(RH_RF69::GFSK_Rb4_8Fd9_6);

  // Emit boot event
  Serial.print(F("{\"type\":\"boot\",\"freq_mhz\":"));
  Serial.print(RFM_FREQUENCY_MHZ);
  Serial.println(F(",\"modem\":\"GFSK_Rb4_8Fd9_6\"}"));

  last_heartbeat_ms = millis();
}

void loop() {
  unsigned long now = millis();

  // Check for incoming packet
  if (rf.available()) {
    uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf.recv(buf, &len)) {
      last_heartbeat_ms = now;  // reset heartbeat timer on any activity

      // Validate packet size
      if (len != sizeof(RadioPacket)) {
        Serial.print(F("{\"type\":\"error\",\"seq\":"));
        Serial.print((int)packet_seq);
        Serial.print(F(",\"rx_ms\":"));
        Serial.print(now);
        Serial.print(F(",\"reason\":\"bad_length\",\"got\":"));
        Serial.print(len);
        Serial.println(F("}"));
        return;
      }

      packet_seq++;
      const RadioPacket *pkt = (const RadioPacket *)buf;
      int8_t rssi = rf.lastRssi();

      // Emit telemetry as NDJSON
      Serial.print(F("{\"type\":\"telemetry\",\"seq\":"));
      Serial.print(packet_seq);
      Serial.print(F(",\"rx_ms\":"));
      Serial.print(now);
      Serial.print(F(",\"t_ms\":"));
      Serial.print(pkt->timestamp_ms);
      Serial.print(F(",\"tempC\":"));
      Serial.print(pkt->temperatureC, 2);
      Serial.print(F(",\"pressHpa\":"));
      Serial.print(pkt->pressureHpa, 2);
      Serial.print(F(",\"altM\":"));
      Serial.print(pkt->altitudeM, 2);
      Serial.print(F(",\"rssi\":"));
      Serial.print(rssi);
      Serial.println(F("}"));
    }
  }

  // Emit heartbeat if silent for too long (lets relay distinguish "radio quiet" from "disconnected")
  if (now - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
    Serial.print(F("{\"type\":\"heartbeat\",\"rx_ms\":"));
    Serial.print(now);
    Serial.println(F("}"));
    last_heartbeat_ms = now;
  }
}
