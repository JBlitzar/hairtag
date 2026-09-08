#include <Arduino.h>

#define LED_PIN 10

void setup() {
    Serial.begin(9600);
    Serial.println("Blinky smoke-test");
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
}
