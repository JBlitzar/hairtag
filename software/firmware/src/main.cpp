#include <NimBLEDevice.h>

void setup() {
  Serial.begin(9600);
  Serial.println("Starting NimBLE Advertiser...");

  NimBLEDevice::init("NimBLE_Test_Device");

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

  pAdvertising->setScanResponse(true);

  pAdvertising->start();
  Serial.println("Advertising started! Scan for 'NimBLE_Test_Device' on your phone.");
}

void loop() {
  delay(2000);
  Serial.println("Hello, nimble!");
}
