#include <Arduino.h>
#include <NimBLEDevice.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#include <esp_partition.h>
#include <esp_log.h>
#include <esp_random.h>
#include <driver/gpio.h>

#define DELAY_IN_S      30
#define ADV_DURATION_MS 50
#define REUSE_CYCLES    30

static const char *LOG_TAG = "macless_haystack";

RTC_DATA_ATTR static uint8_t key_index = 0;
RTC_DATA_ATTR static uint8_t cycle     = 0;
RTC_DATA_ATTR static uint8_t key_count = 0;
RTC_DATA_ATTR static bool    rtc_valid = false;

static uint8_t public_key[28];

#define LED_PIN 10



int load_bytes_from_partition(uint8_t *dst, size_t size, int offset) {
    const esp_partition_t *keypart = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "key");
    if (keypart == NULL) {
        Serial.println("ERROR: Could not find key partition");
        return ESP_FAIL;
    }
    esp_err_t status = esp_partition_read(keypart, offset, dst, size);
    if (status != ESP_OK) {
        Serial.printf("ERROR: Could not read key from partition: %s\n", esp_err_to_name(status));
    }
    return status;
}

uint8_t get_key_count() {
    uint8_t keyCount[1];
    if (load_bytes_from_partition(keyCount, sizeof(keyCount), 0) != ESP_OK) {
        Serial.println("ERROR: Could not read key count");
        return 0;
    }
    Serial.printf("Found %d keys in key partition\n", keyCount[0]);
    return keyCount[0];
}

void set_addr_from_key(uint8_t *addr, uint8_t *key) {
    addr[0] = key[0] | 0b11000000;
    addr[1] = key[1];
    addr[2] = key[2];
    addr[3] = key[3];
    addr[4] = key[4];
    addr[5] = key[5];
}

void setup() {
    Serial.begin(9600);


    gpio_hold_dis((gpio_num_t)LED_PIN);
    pinMode((gpio_num_t)LED_PIN, OUTPUT);
    digitalWrite((gpio_num_t)LED_PIN, HIGH);
    delay(1000);
    digitalWrite((gpio_num_t)LED_PIN, LOW);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!rtc_valid) {
        // First boot: read key count from partition and pick a random starting key
        key_count = get_key_count();
        if (key_count == 0) {
            Serial.println("ERROR: No keys found, sleeping 60s");
            esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
            esp_deep_sleep_start();
        }
        key_index = esp_random() % key_count;
        cycle     = 0;
        rtc_valid = true;
    } else {
        // Woke from deep sleep: rotate key
        if (cycle >= REUSE_CYCLES) {
            uint8_t next;
            do {
                next = esp_random() % key_count;
            } while (next == key_index && key_count > 1);
            key_index = next;
            cycle = 0;
        } else {
            cycle++;
        }
    }

    // Load key from partition
    int address = 1 + (key_index * sizeof(public_key));
    if (load_bytes_from_partition(public_key, sizeof(public_key), address) != ESP_OK) {
        Serial.println("ERROR: Could not read key, sleeping 5s");
        esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);
        esp_deep_sleep_start();
    }

    // Build BLE address from key
    uint8_t rnd_addr[6];
    set_addr_from_key(rnd_addr, public_key);

    // Init NimBLE
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setOwnAddrType(BLE_ADDR_RANDOM);

    if (BLE_HS_ECONTROLLER == ble_hs_id_set_rnd(rnd_addr)) {
        Serial.println("ERROR: Failed to set random address");
        NimBLEDevice::deinit(true);
        esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);
        esp_deep_sleep_start();
    }

    // Build manufacturer data payload
    std::string mfgData;
    mfgData.push_back(0x4c);
    mfgData.push_back(0x00);
    mfgData.push_back(0x12);
    mfgData.push_back(0x19);
    mfgData.push_back(0x00);
    for (int i = 6; i < 28; i++) {
        mfgData.push_back(public_key[i]);
    }
    mfgData.push_back(public_key[0] >> 6);
    mfgData.push_back(0x00);

    NimBLEAdvertisementData advData;
    advData.setManufacturerData(mfgData);

    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
    pAdv->setAdvertisementData(advData);
    pAdv->setMinInterval(0x0800);
    pAdv->setMaxInterval(0x0800);
    pAdv->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
    pAdv->setScanResponse(false);

    Serial.printf("Sending beacon (key index %d, cycle %d)\n", key_index, cycle);
    pAdv->start();
    delay(ADV_DURATION_MS);
    pAdv->stop();

    // Tear down BLE before sleeping
    NimBLEDevice::deinit(true);

    Serial.printf("Sleeping %ds\n", DELAY_IN_S);
    Serial.flush();


    gpio_hold_en((gpio_num_t)LED_PIN);
    esp_sleep_enable_timer_wakeup((uint64_t)DELAY_IN_S * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Never reached: deep sleep restarts from setup() on every wakeup
}
