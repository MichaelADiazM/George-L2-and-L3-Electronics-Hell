/**
 * radio.cpp
 * --------
 * 
 * 
 */

#include "config.h"
#include "radio.h"
#include <SPI.h>
#include <RH_RF69.h>

static RH_RF69 rf = {RFM69_CS_PIN, RFM69_INT_PIN};

bool initRadio() {
    if (!rf.init()){
        Serial.println("[radio] init failed. Check wiring.");
        return false;
    }

    if (!rf.setFrequency(RFM69_FREQUENCY_MHZ)) {
        Serial.println("[radio] Set frequency failed. Check wiring.");
        return false;
    }

    rf.setTxPower(20, true);
    rf.setModemConfig(RH_RF69::GFSK_Rb4_8Fd9_6);

    return true;
}

void transmitReading(const RadioPacket& packet) {
    rf.send((const uint8_t*)&packet, sizeof(packet));
    rf.waitPacketSent();
}
