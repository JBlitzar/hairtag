#include <Arduino.h>

#define LED_PIN 10

static uint32_t idx = 0;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    Serial.printf("alive! + %lu\n", (unsigned long)idx);
    idx++;
    delay(1000);
}
