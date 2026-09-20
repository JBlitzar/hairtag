#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

static uint8_t key[28] = {
    0x1c, 0x4f, 0xe7, 0xea, 0x90, 0x86, 0xc2, 0x5d, 0xf7, 0x68, 0xb7, 0x9d,
    0x57, 0x34, 0x5b, 0x5e, 0x52, 0xeb, 0xe6, 0xb7, 0xc4, 0xaf, 0xa4, 0x59,
    0xb7, 0xdc, 0xe7, 0x10
};

static int i;
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

int main(void)
{
    int err;

    i = 0;

    bt_addr_le_t addr;

    addr.type = BT_ADDR_LE_RANDOM;
    addr.a.val[0] = key[5];
    addr.a.val[1] = key[4];
    addr.a.val[2] = key[3];
    addr.a.val[3] = key[2];
    addr.a.val[4] = key[1];
    addr.a.val[5] = key[0] | 0xC0;

    err = bt_id_create(&addr, NULL);

    err = bt_enable(NULL);
    if (err) {
        return 0;
    }

    gpio_pin_configure(gpio0_dev, 14, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);

    size_t idx = 0;

    mfg_data[idx++] = 0x4c;
    mfg_data[idx++] = 0x00;   /* Apple */
    mfg_data[idx++] = 0x12;
    mfg_data[idx++] = 0x19;   /* Find My / Offline Finding */
    mfg_data[idx++] = 0x00;   /* status, filled below */
    memcpy(&mfg_data[idx], &key[6], 22);
    idx += 22;
    mfg_data[idx++] = (uint8_t)(key[0] >> 6);
    mfg_data[idx++] = 0x00;   /* hint */

    update_status_byte();

    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_USE_IDENTITY,
        1600,  // 1000 ms min
        3200,  // 2000ms max
        NULL
    );
    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    bool started = (err == 0);

    uint8_t used[6];
    bt_addr_le_t used_addrs[CONFIG_BT_ID_MAX];
    size_t count = 1;

    bt_id_get(used_addrs, &count);
    if (count > 0) {
        memcpy(used, used_addrs[0].a.val, 6);
    }

    while (1) {
        k_sleep(K_MINUTES(1));
        update_status_byte();
        bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
    }

    return 0;
}
