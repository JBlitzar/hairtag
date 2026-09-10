#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/addr.h>
#include <string.h>

static uint8_t key[28] = {
    0x1c, 0x4f, 0xe7, 0xea, 0x90, 0x86, 0xc2, 0x5d, 0xf7, 0x68, 0xb7, 0x9d,
    0x57, 0x34, 0x5b, 0x5e, 0x52, 0xeb, 0xe6, 0xb7, 0xc4, 0xaf, 0xa4, 0x59,
    0xb7, 0xdc, 0xe7, 0x10
};

static int i;
static uint8_t mfg_data[29];

int main(void)
{
    int err;

    printk("Starting...\n");

    i = 0;

    

    /* addr[6] built as { key[5], key[4], key[3], key[2], key[1],
     *                    (key[0] | 0xC0) } — matches the corrected
     * NimBLE array (top two bits of the top byte set = static addr). */
    bt_addr_le_t addr;

    addr.type = BT_ADDR_LE_RANDOM;
    addr.a.val[0] = key[5];
    addr.a.val[1] = key[4];
    addr.a.val[2] = key[3];
    addr.a.val[3] = key[2];
    addr.a.val[4] = key[1];
    addr.a.val[5] = key[0] | 0xC0;

    err = bt_id_create(&addr, NULL);
    printk("ble_hs_id_set_rnd rc=%d\n", err);

    err = bt_enable(NULL);
    if (err) {
        printk("bt_enable failed (err %d)\n", err);
        return 0;
    }

    /* mfg.append(...) calls, in order */
    size_t idx = 0;

    mfg_data[idx++] = 0x4c;
    mfg_data[idx++] = 0x00;   /* Apple */
    mfg_data[idx++] = 0x12;
    mfg_data[idx++] = 0x19;   /* Find My / Offline Finding */
    mfg_data[idx++] = 0x00;   /* state */
    memcpy(&mfg_data[idx], &key[6], 22);
    idx += 22;
    mfg_data[idx++] = (uint8_t)(key[0] >> 6);
    mfg_data[idx++] = 0x00;   /* hint */

    printk("mfg len=%d (expect 29)\n", (int)idx);
    for (size_t n = 0; n < idx; n++) {
        printk("%02x ", mfg_data[n]);
    }
    printk("\n");

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, idx),
    };

    struct bt_le_adv_param adv_param = *BT_LE_ADV_NCONN_IDENTITY;

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    bool started = (err == 0);

    uint8_t used[6];
    bt_addr_le_t used_addrs[CONFIG_BT_ID_MAX];
    size_t count = 1;

    bt_id_get(used_addrs, &count);
    if (count > 0) {
        memcpy(used, used_addrs[0].a.val, 6);
    }

    printk("started=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n", started,
           used[5], used[4], used[3], used[2], used[1], used[0]);

    /* loop() */
    while (1) {
        k_sleep(K_MSEC(2000));
        printk("Hello, ohs! %d\n", i);
        i++;
    }

    return 0;
}