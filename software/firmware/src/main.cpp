#include <NimBLEDevice.h>

uint8_t key[28] = { 0x1c, 0x4f, 0xe7, 0xea, 0x90, 0x86, 0xc2, 0x5d, 0xf7, 0x68, 0xb7, 0x9d, 0x57, 0x34, 0x5b, 0x5e, 0x52, 0xeb, 0xe6, 0xb7, 0xc4, 0xaf, 0xa4, 0x59, 0xb7, 0xdc, 0xe7, 0x10 };

int i;

void setup() {
    Serial.begin(9600);
    Serial.println("Starting...");

    i=0;
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_N12);

    uint8_t addr[6] = {
        key[5], key[4], key[3], key[2], key[1],
        (uint8_t)(key[0] | 0xC0)
    };

    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    int rc = ble_hs_id_set_rnd(addr);
    Serial.printf("ble_hs_id_set_rnd rc=%d\n", rc);

    NimBLEAdvertisementData adv;

    std::string mfg;
    mfg.append("\x4c\x00", 2);   // Apple
    mfg.append("\x12\x19", 2);   // Find My / Offline Finding
    mfg.append("\x00", 1);       // state
    mfg.append((char *)&key[6], 22);
    mfg += (char)(key[0] >> 6);
    mfg.append("\x00", 1);       // hint

    Serial.printf("mfg len=%d (expect 29)\n", mfg.size());
    for (size_t n = 0; n < mfg.size(); n++) Serial.printf("%02x ", (uint8_t)mfg[n]);
    Serial.println();

    adv.setManufacturerData(mfg);

    NimBLEAdvertising *a = NimBLEDevice::getAdvertising();
    a->setAdvertisementData(adv);
    a->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
    a->setMinInterval(0x0640);   // 1000ms (0.625ms units)
    a->setMaxInterval(0x0C80);   // 2000ms (0.625ms units)
    bool started = a->start();

    uint8_t used[6];
    ble_hs_id_copy_addr(BLE_ADDR_RANDOM, used, NULL);
    Serial.printf("started=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n", started,
                  used[5], used[4], used[3], used[2], used[1], used[0]);
}


void loop() {
  delay(2000);
  Serial.printf("Hello, ohs! %d\n", i);
  i++;
}
