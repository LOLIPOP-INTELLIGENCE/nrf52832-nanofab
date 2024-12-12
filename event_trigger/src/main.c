#include <stdio.h>
#include <string.h>
#include <zephyr/console/console.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ADVERTISING_UUID128 BT_UUID_128_ENCODE(0x038a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)
#define CUSTOM_SERVICE_UUID BT_UUID_128_ENCODE(0x938a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)
#define CUSTOM_CHARACTERISTIC_UUID BT_UUID_128_ENCODE(0xa38a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)

/* LED configuration */
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static struct bt_conn *current_conn;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, ADVERTISING_UUID128),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(CUSTOM_SERVICE_UUID);
static struct bt_uuid_128 custom_characteristic_uuid = BT_UUID_INIT_128(CUSTOM_CHARACTERISTIC_UUID);

// Callback for handling LED control commands
static ssize_t write_led_control(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags)
{
    const uint8_t *value = buf;
    
    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    // Control LED based on received value
    if (value[0] == '1') {
        gpio_pin_set_dt(&led, 1);
        printk("LED ON\n");
    } else if (value[0] == '0') {
        gpio_pin_set_dt(&led, 0);
        printk("LED OFF\n");
    }
    
    return len;
}

// Define the GATT service
BT_GATT_SERVICE_DEFINE(custom_svc,
    BT_GATT_PRIMARY_SERVICE(&custom_service_uuid),
    BT_GATT_CHARACTERISTIC(&custom_characteristic_uuid.uuid,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_led_control, NULL),
);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }
    current_conn = bt_conn_ref(conn);
    printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready()
{
    int err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,
                                        BT_GAP_ADV_FAST_INT_MIN_2,
                                        BT_GAP_ADV_FAST_INT_MAX_2,
                                        NULL),
                         ad, ARRAY_SIZE(ad),
                         sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
    printk("Advertising successfully started\n");
}

int main(void)
{
    int err;

    // Initialize LED
    if (!gpio_is_ready_dt(&led)) {
        printk("LED device not ready\n");
        return 0;
    }

    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        printk("LED configuration failed\n");
        return 0;
    }

    // Initialize Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    bt_ready();

    // Main loop
    for (;;) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}