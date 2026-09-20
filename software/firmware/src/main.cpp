#include <Arduino.h>
#include <NimBLEDevice.h>

#define LED_PIN 10

uint8_t key[28] = { 0x1c, 0x4f, 0xe7, 0xea, 0x90, 0x86, 0xc2, 0x5d, 0xf7, 0x68, 0xb7, 0x9d, 0x57, 0x34, 0x5b, 0x5e, 0x52, 0xeb, 0xe6, 0xb7, 0xc4, 0xaf, 0xa4, 0x59, 0xb7, 0xdc, 0xe7, 0x10 };

void setup() {
    Serial.begin(9600);
    Serial.println("Starting...");

    pinMode(LED_PIN, OUTPUT);

    for (int b = 0; b < 2; b++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
    delay(500);

    NimBLEDevice::init("");
    for (int b = 0; b < 3; b++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
    Serial.println("NimBLE init OK");

    NimBLEDevice::setPower(ESP_PWR_LVL_N6);

    uint8_t addr[6] = {
        key[5], key[4], key[3], key[2], key[1],
        (uint8_t)(key[0] | 0xC0)
    };
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    ble_hs_id_set_rnd(addr);

    NimBLEAdvertisementData adv;
    std::string mfg;
    mfg.append("\x4c\x00", 2);
    mfg.append("\x12\x19", 2);
    mfg.append("\x00", 1);
    mfg.append((char *)&key[6], 22);
    mfg += (char)(key[0] >> 6);
    mfg.append("\x00", 1);
    adv.setManufacturerData(mfg);

    NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
    a->setAdvertisementData(adv);
    a->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
    bool started = a->start();

    Serial.printf("started=%d\n", started);
    for (int b = 0; b < 4; b++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
}
