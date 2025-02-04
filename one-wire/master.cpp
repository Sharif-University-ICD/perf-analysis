#include <Arduino.h>

const int dataPin = 7;

void setup() {
    pinMode(dataPin, OUTPUT);
    digitalWrite(dataPin, HIGH);
}

void sendBit(byte bitVal) {
    if (bitVal == 0) {
        digitalWrite(dataPin, LOW);
        delayMicroseconds(60);
        digitalWrite(dataPin, HIGH);
        delayMicroseconds(10);
    }
    else {
        digitalWrite(dataPin, LOW);
        delayMicroseconds(10);
        digitalWrite(dataPin, HIGH);
        delayMicroseconds(60);
    }
}

// Send a single byte (MSB first)
void sendByte(byte data) {
    for (int i = 7; i >= 0; i--) {
        byte bitVal = (data >> i) & 0x01;
        sendBit(bitVal);
    }
}

void loop() {
    for (int i = 0; i < 256; i++)
        sendByte((byte)i);
    delay(5000); // Wait 5 seconds between transmissions
}
