#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE,
            "XIAO-TEST", 9),
};

int main(void)
{
    bt_enable(NULL);

    bt_le_adv_start(
        BT_LE_ADV_CONN,
        ad, ARRAY_SIZE(ad),
        NULL, 0);

    return 0;
}
