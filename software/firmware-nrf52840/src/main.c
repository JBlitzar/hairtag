#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

#include "keys.h"

#ifndef KEY_ROTATION_INTERVAL_MS
#define KEY_ROTATION_INTERVAL_MS (3 * 60 * 60 * 1000)
#endif

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

/* XIAO battery divider: VBAT -> 1M -> P0.31/AIN7 -> 510k -> GND,
   enabled by driving P0.14 low */
#define DIVIDER_NUM 1510
#define DIVIDER_DEN 510

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const struct adc_dt_spec vbat_channel =
    ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int battery_mv(void)
{
    int16_t raw;
    int32_t mv;
    struct adc_sequence seq = {
        .channels = BIT(vbat_channel.channel_id),
        .buffer = &raw,
        .buffer_size = sizeof(raw),
        .resolution = 12,
    };

    if (adc_channel_setup_dt(&vbat_channel) < 0) {
        return -1;
    }
    if (adc_read(adc_dev, &seq) < 0) {
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
    int rot_id = -1;

    set_key(0, &addr);
    bt_id_create(&addr, NULL);

    err = bt_enable(NULL);
    if (err) {
        return 0;
    }

    gpio_pin_configure(gpio0_dev, 14, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);

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

        int next = (int)((k_uptime_get() / KEY_ROTATION_INTERVAL_MS) % KEY_COUNT);

        if (next != key_idx) {
            key_idx = next;
            bt_le_adv_stop();
            set_key(key_idx, &addr);
            if (rot_id < 0) {
                rot_id = bt_id_create(&addr, NULL);
                adv_param.id = rot_id;
            } else {
                bt_id_reset(rot_id, &addr, NULL);
            }
            update_status_byte();
            bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
        } else {
            update_status_byte();
            bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
        }
    }

    return 0;
}
