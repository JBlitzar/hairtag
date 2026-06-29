#ifdef HAIRTAG_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_sleep.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

#define DELAY_IN_S      30
#define ADV_DURATION_MS 50
#define REUSE_CYCLES    30

static const char *LOG_TAG = "macless_haystack";

RTC_DATA_ATTR static uint8_t key_index = 0;
RTC_DATA_ATTR static uint8_t cycle     = 0;
RTC_DATA_ATTR static uint8_t key_count = 0;
RTC_DATA_ATTR static bool    rtc_valid = false;

static uint8_t public_key[28];
static uint8_t g_adv_data[31];
static size_t  g_adv_len = 0;
static uint8_t g_rnd_addr[6];
static SemaphoreHandle_t adv_started;

static void deep_sleep(uint64_t seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}

static int load_bytes_from_partition(uint8_t *dst, size_t size, int offset) {
    const esp_partition_t *keypart = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS, "key");
    if (keypart == NULL) {
        ESP_LOGE(LOG_TAG, "Could not find key partition");
        return ESP_FAIL;
    }
    esp_err_t status = esp_partition_read(keypart, offset, dst, size);
    if (status != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Could not read key from partition: %s", esp_err_to_name(status));
    }
    return status;
}

static uint8_t get_key_count() {
    uint8_t count[1];
    if (load_bytes_from_partition(count, sizeof(count), 0) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Could not read key count");
        return 0;
    }
    ESP_LOGI(LOG_TAG, "Found %d keys in key partition", count[0]);
    return count[0];
}

static void set_addr_from_key(uint8_t *addr, const uint8_t *key) {
    addr[0] = key[0] | 0b11000000;
    addr[1] = key[1];
    addr[2] = key[2];
    addr[3] = key[3];
    addr[4] = key[4];
    addr[5] = key[5];
}

static void build_adv_data(const uint8_t *key) {
    g_adv_data[0] = 0x1e; // length of the AD element that follows (30 bytes)
    g_adv_data[1] = 0xff; // manufacturer specific data
    g_adv_data[2] = 0x4c; // Apple company id (LE)
    g_adv_data[3] = 0x00;
    g_adv_data[4] = 0x12; // offline finding type
    g_adv_data[5] = 0x19; // offline finding length
    g_adv_data[6] = 0x00; // status
    for (int i = 0; i < 22; i++) {
        g_adv_data[7 + i] = key[6 + i];
    }
    g_adv_data[29] = key[0] >> 6;
    g_adv_data[30] = 0x00; // hint byte
    g_adv_len = 31;
}

static void on_sync(void) {
    int rc = ble_hs_id_set_rnd(g_rnd_addr);
    if (rc != 0) {
        ESP_LOGE(LOG_TAG, "Failed to set random address: %d", rc);
        xSemaphoreGive(adv_started);
        return;
    }

    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

    rc = ble_gap_adv_set_data(g_adv_data, (int)g_adv_len);
    if (rc != 0) {
        ESP_LOGE(LOG_TAG, "Failed to set adv data: %d", rc);
        xSemaphoreGive(adv_started);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_NON;
    adv_params.itvl_min  = 0x0800;
    adv_params.itvl_max  = 0x0800;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(LOG_TAG, "Failed to start advertising: %d", rc);
    } else {
        ESP_LOGI(LOG_TAG, "Sending beacon (key index %d, cycle %d)", key_index, cycle);
    }
    xSemaphoreGive(adv_started);
}

static void host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

extern "C" void app_main(void) {
#ifdef HAIRTAG_DEBUG
    esp_log_level_set("*", ESP_LOG_INFO);
#endif

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!rtc_valid) {
        // First boot: read key count and pick a random starting key.
        key_count = get_key_count();
        if (key_count == 0) {
            ESP_LOGE(LOG_TAG, "No keys found, sleeping 60s");
            deep_sleep(60);
        }
        key_index = esp_random() % key_count;
        cycle     = 0;
        rtc_valid = true;
    } else if (cycle >= REUSE_CYCLES) {
        // Woke from deep sleep: rotate to a different key.
        uint8_t next;
        do {
            next = esp_random() % key_count;
        } while (next == key_index && key_count > 1);
        key_index = next;
        cycle = 0;
    } else {
        cycle++;
    }

    int address = 1 + (key_index * sizeof(public_key));
    if (load_bytes_from_partition(public_key, sizeof(public_key), address) != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Could not read key, sleeping 5s");
        deep_sleep(5);
    }

    set_addr_from_key(g_rnd_addr, public_key);
    build_adv_data(public_key);

    adv_started = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);

    xSemaphoreTake(adv_started, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(ADV_DURATION_MS));

    ESP_LOGI(LOG_TAG, "Sleeping %ds", DELAY_IN_S);
    deep_sleep(DELAY_IN_S);
}
