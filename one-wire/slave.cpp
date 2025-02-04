#include <Arduino.h>
const int dataPin = 5;

volatile byte receivedData[256];
volatile int bitCount = 0;
volatile int byteIndex = 0;
volatile byte currentByte = 0;
volatile bool messageReceived = false;

// This ISR is called on the falling edge of the data line.
IRAM_ATTR void oneWireISR()
{
    ets_delay_us(30);
    byte bitVal = digitalRead(dataPin);
    currentByte = (currentByte << 1) | bitVal; // Build up the byte from the incoming bits.
    bitCount++;

    // Once 8 bits have been received, store the byte in the buffer.
    if (bitCount % 8 == 0)
    {
        if (byteIndex < 256)
            receivedData[byteIndex++] = currentByte;
        currentByte = 0;
    }

    // When 256 bytes have been received, flag that the message is complete.
    if (byteIndex >= 256)
        messageReceived = true;
}

void setup()
{
    Serial.begin(115200);
    pinMode(dataPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(dataPin), oneWireISR, FALLING);
    Serial.println("one-wire slave started...");
}

void loop()
{
    // Once the full message has been received, process it.
    if (messageReceived)
    {
        Serial.println("Received message:");
        for (int i = 0; i < 256; i++)
        {
            // Print each byte in hexadecimal
            if (receivedData[i] < 16)
                Serial.print("0"); // Leading zero for single-digit values
            Serial.print(receivedData[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        // Reset variables for the next message.
        noInterrupts();
        bitCount = 0;
        byteIndex = 0;
        currentByte = 0;
        messageReceived = false;
        interrupts();
    }
}
