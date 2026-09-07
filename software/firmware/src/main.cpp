#include <NimBLEDevice.h>

uint8_t key[28] = { 0x1c, 0x4f, 0xe7, 0xea, 0x90, 0x86, 0xc2, 0x5d, 0xf7, 0x68, 0xb7, 0x9d, 0x57, 0x34, 0x5b, 0x5e, 0x52, 0xeb, 0xe6, 0xb7, 0xc4, 0xaf, 0xa4, 0x59, 0xb7, 0xdc, 0xe7, 0x10 };

int i;

void setup() {
    i=0;
    NimBLEDevice::init("");

    uint8_t addr[6] = {
        (uint8_t)(key[0] | 0xC0),
        key[1], key[2], key[3], key[4], key[5]
    };

    NimBLEAdvertisementData adv;

    std::string mfg;
    mfg += "\x4c\x00";       // Apple
    mfg += "\x12\x19";       // Find My / Offline Finding
    mfg += "\x00";           // state
    mfg.append((char *)&key[6], 22);
    mfg += (char)(key[0] >> 6);
    mfg += "\x00";            // hint

    adv.setManufacturerData(mfg);

    NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
    a->setAdvertisementData(adv);
    a->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
    a->start();
}


void loop() {
  delay(2000);
  Serial.printf("Hello, ohs! %d\n", i);
  i++;
}
