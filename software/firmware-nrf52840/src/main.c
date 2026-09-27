#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kvss/nvs.h>
#include <string.h>

#include "keys.h"

#ifndef KEY_ROTATION_INTERVAL_MS
#define KEY_ROTATION_INTERVAL_MS (3 * 60 * 60 * 1000)
#endif

#define CHUNK_MS (5 * 60 * 1000)
#define CHUNKS_PER_ROT (KEY_ROTATION_INTERVAL_MS / CHUNK_MS)
#define NVS_ID_PROGRESS 3
#define PROGRESS_MAX (KEY_COUNT * CHUNKS_PER_ROT)
#define NVS_SECTOR_SIZE 4096

BUILD_ASSERT(PARTITION_SIZE(storage_partition) % NVS_SECTOR_SIZE == 0,
             "storage_partition must be a whole number of NVS sectors");
BUILD_ASSERT(PROGRESS_MAX <= UINT16_MAX, "progress counter must fit in uint16_t");

static struct nvs_fs nvs = {
    .flash_device = PARTITION_DEVICE(storage_partition),
    .offset = PARTITION_OFFSET(storage_partition),
    .sector_size = NVS_SECTOR_SIZE,
    .sector_count = PARTITION_SIZE(storage_partition) / NVS_SECTOR_SIZE,
};

static uint8_t mfg_data[29];

static struct bt_data ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

/* status byte: bits 7-4 + 1-0 = battery code (0-63, 2.5V-4.2V),
   bits 3-2 forced high (apple device + maintained => no UT alerts) */
#define STATUS_IDX 4
#define STATUS_FIXED 0x0c
#define BATT_MV_MIN 2500
#define BATT_MV_MAX 4200

/* XIAO battery divider: VBAT -> 1M -> P0.31/AIN7 -> 510k -> P0.14,
   enabled by driving P0.14 low, disconnected otherwise so the divider
   does not sink ~2.6 uA continuously */
#define DIVIDER_NUM 1510
#define DIVIDER_DEN 510
#define VBAT_ENABLE_PIN 14

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct device *ext_flash_dev = DEVICE_DT_GET(DT_NODELABEL(p25q16h));
static const struct adc_dt_spec vbat_channel =
    ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int battery_mv(void)
{
    int16_t raw;
    int32_t mv;
    int err;
    struct adc_sequence seq = {
        .channels = BIT(vbat_channel.channel_id),
        .buffer = &raw,
        .buffer_size = sizeof(raw),
        .resolution = 12,
    };

    gpio_pin_configure(gpio0_dev, VBAT_ENABLE_PIN,
                       GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
    k_msleep(1);

    err = adc_channel_setup_dt(&vbat_channel);
    if (!err) {
        err = adc_read(adc_dev, &seq);
    }

    gpio_pin_configure(gpio0_dev, VBAT_ENABLE_PIN, GPIO_DISCONNECTED);

    if (err) {
        return -1;
    }
    mv = raw;
    if (adc_raw_to_millivolts(adc_ref_internal(adc_dev),
                              vbat_channel.channel_cfg.gain, 12, &mv) < 0) {
        return -1;
    }
    return (int)(mv * DIVIDER_NUM / DIVIDER_DEN);
}

static void update_status_byte(void)
{
    int mv = battery_mv();
    int v;

    if (mv < 0) {
        return;
    }
    v = (mv - BATT_MV_MIN) * 63 / (BATT_MV_MAX - BATT_MV_MIN);
    if (v < 0) {
        v = 0;
    } else if (v > 63) {
        v = 63;
    }
    mfg_data[STATUS_IDX] = ((v & 0x3c) << 2) | STATUS_FIXED | (v & 0x03);
}

static void set_key(int idx, bt_addr_le_t *addr)
{
    const uint8_t *key = keys[idx];

    addr->type = BT_ADDR_LE_RANDOM;
    addr->a.val[0] = key[5];
    addr->a.val[1] = key[4];
    addr->a.val[2] = key[3];
    addr->a.val[3] = key[2];
    addr->a.val[4] = key[1];
    addr->a.val[5] = key[0] | 0xC0;

    mfg_data[0] = 0x4c;
    mfg_data[1] = 0x00;   /* Apple */
    mfg_data[2] = 0x12;
    mfg_data[3] = 0x19;   /* Find My / Offline Finding */
    /* mfg_data[4] is the battery status byte, managed separately */
    memcpy(&mfg_data[5], &key[6], 22);
    mfg_data[27] = (uint8_t)(key[0] >> 6);
    mfg_data[28] = 0x00;   /* hint */
}

int main(void)
{
    int err;

    bt_addr_le_t addr;
    int key_idx = 0;
    uint16_t progress = 0;
    int rot_id = -1;
    bool nvs_ok;
    int64_t base_ms = 0;

    pm_device_action_run(ext_flash_dev, PM_DEVICE_ACTION_SUSPEND);

    nvs_ok = (nvs_mount(&nvs) == 0);
    if (nvs_ok) {
        uint16_t v;

        if (nvs_read(&nvs, NVS_ID_PROGRESS, &v, sizeof(v)) == sizeof(v) &&
            v < PROGRESS_MAX) {
            progress = v;
        }
        key_idx = progress / CHUNKS_PER_ROT;
        base_ms = (int64_t)progress * CHUNK_MS;
    }

    set_key(key_idx, &addr);
    if (bt_id_create(&addr, NULL) < 0) {
        return 0;
    }

    err = bt_enable(NULL);
    if (err) {
        return 0;
    }

    mfg_data[STATUS_IDX] = STATUS_FIXED;
    update_status_byte();

    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_USE_IDENTITY,
        1600,  // 1000 ms min
        3200,  // 2000ms max
        NULL
    );
    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);

    while (1) {
        k_sleep(K_MINUTES(1));

        int64_t elapsed = base_ms + k_uptime_get();
        uint16_t next_progress = (uint16_t)((elapsed / CHUNK_MS) % PROGRESS_MAX);
        int next = next_progress / CHUNKS_PER_ROT;

        if (next_progress != progress) {
            progress = next_progress;
            if (nvs_ok) {
                nvs_write(&nvs, NVS_ID_PROGRESS, &progress, sizeof(progress));
            }
        }

        if (next != key_idx) {
            uint8_t use_id = BT_ID_DEFAULT;

            key_idx = next;
            bt_le_adv_stop();
            set_key(key_idx, &addr);

            if (rot_id < 0) {
                int id = bt_id_create(&addr, NULL);

                if (id >= 0) {
                    rot_id = id;
                    use_id = (uint8_t)id;
                }
            } else if (bt_id_reset(rot_id, &addr, NULL) >= 0) {
                use_id = (uint8_t)rot_id;
            }
            adv_param.id = use_id;

            update_status_byte();
            bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
        } else {
            update_status_byte();
            bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
        }
    }

    return 0;
}
