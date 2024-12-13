#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

// Temperature sensor definitions
#define MAX30205_NODE DT_NODELABEL(max30205)
#define MAX30205_TEMP_REG 0x00
#define MAX30205_CONFIG_REG 0x01

// BLE UUIDs
#define CUSTOM_SERVICE_UUID BT_UUID_128_ENCODE(0x938a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)
#define CONTROL_CHAR_UUID BT_UUID_128_ENCODE(0xa38a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)
#define TEMP_CHAR_UUID BT_UUID_128_ENCODE(0xb38a803f, 0xf6b3, 0x420b, 0xa95a, 0x10cc7b32b6db)

static struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(CUSTOM_SERVICE_UUID);
static struct bt_uuid_128 control_characteristic_uuid = BT_UUID_INIT_128(CONTROL_CHAR_UUID);
static struct bt_uuid_128 temp_characteristic_uuid = BT_UUID_INIT_128(TEMP_CHAR_UUID);

static struct bt_conn *current_conn;
static bool temp_reading_active = false;
static struct k_work_delayable temp_work;

// Forward declaration of the GATT service
extern const struct bt_gatt_service_static custom_svc;

static struct bt_gatt_attr *temp_attr;

// Temperature notification flag
static bool temp_notifications_enabled;

/* Store the handles for later use */
static uint16_t temp_value_handle;
static uint16_t temp_ccc_handle;

// I2C device specification
static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(MAX30205_NODE);

/* Function to initialize handles */
static void init_handles(void)
{
    /* The temperature value handle is at index 3 (counting from 1) */
    temp_value_handle = bt_gatt_attr_get_handle(&custom_svc.attrs[2]);
    /* The CCC handle is right after */
    temp_ccc_handle = bt_gatt_attr_get_handle(&custom_svc.attrs[3]);
    
    printk("Temperature value handle: %u\n", temp_value_handle);
    printk("Temperature CCC handle: %u\n", temp_ccc_handle);
}

// CCC descriptor callback for temperature characteristic
static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    temp_notifications_enabled = (value == BT_GATT_CCC_NOTIFY);
    printk("Temperature notifications %s (value: 0x%04x, handle: %d)\n", 
           temp_notifications_enabled ? "enabled" : "disabled", 
           value, temp_value_handle);
    
    if (temp_notifications_enabled) {
        // Send an initial notification to verify it works
        uint8_t test_data[2] = {0x19, 0x27};  // Example: 25.39°C
        int err = bt_gatt_notify(NULL, &custom_svc.attrs[2], test_data, sizeof(test_data));
        printk("Initial test notification %s (err: %d)\n", 
               err ? "failed" : "succeeded", err);
    }
}

// Temperature read callback
static ssize_t read_temp_cb(struct bt_conn *conn,
                        const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    uint8_t temp_buffer[2];
    int32_t temp_int = 2632; // Example fixed value, replace with actual temperature
    
    temp_buffer[0] = temp_int / 100;  // Whole number part
    temp_buffer[1] = temp_int % 100;  // Decimal part
    
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                            temp_buffer, sizeof(temp_buffer));
}

// Function to read temperature
static void read_temperature(struct k_work *work)
{
    if (!temp_reading_active) {
        return;
    }

    uint8_t temp_reg = MAX30205_TEMP_REG;
    uint8_t temp_data[2];
    int ret;

    ret = i2c_write_read_dt(&dev_i2c, &temp_reg, 1, temp_data, 2);
    
    if (ret == 0) {
        int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
        float temp_c = temp_raw * 0.00390625f;
        // Convert to int for printing (multiply by 100 to keep 2 decimal places)
        int32_t temp_int = (int32_t)(temp_c * 100);
        printk("Temperature: %d.%02d°C\n", temp_int / 100, temp_int % 100);

        // Send notification if enabled and we have a connection
        if (temp_notifications_enabled && temp_attr) {
            // Convert temperature to 2-byte format
            uint8_t temp_buffer[2];
            temp_buffer[0] = temp_int / 100;  // Whole number part
            temp_buffer[1] = temp_int % 100;  // Decimal part
            
            // Debug prints
            printk("Attempting notification - Handle: %d, Data: [%02X %02X]\n", 
                   temp_value_handle, temp_buffer[0], temp_buffer[1]);
            
            // Send notification using stored attribute
            int err = bt_gatt_notify(NULL, temp_attr, temp_buffer, sizeof(temp_buffer));
            if (err) {
                printk("Failed to send notification (err %d)\n", err);
            } else {
                printk("Notification sent successfully\n");
            }
        }
    }
    
    // Schedule next reading if still active
    if (temp_reading_active) {
        k_work_schedule(&temp_work, K_SECONDS(1));
    }
}

// Callback for handling control commands
static ssize_t write_control(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           const void *buf, uint16_t len, uint16_t offset,
                           uint8_t flags)
{
    const uint8_t *value = buf;

    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (value[0] == '1' && !temp_reading_active) {
        temp_reading_active = true;
        k_work_schedule(&temp_work, K_NO_WAIT); // Start immediate reading
        printk("Temperature reading started\n");
    } else if (value[0] == '0' && temp_reading_active) {
        temp_reading_active = false;
        k_work_cancel_delayable(&temp_work);
        printk("Temperature reading stopped\n");
    }
    
    return len;
}

// Define the GATT service
BT_GATT_SERVICE_DEFINE(custom_svc,
    BT_GATT_PRIMARY_SERVICE(&custom_service_uuid),
    BT_GATT_CHARACTERISTIC(&control_characteristic_uuid.uuid,
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_control, NULL),
    BT_GATT_CHARACTERISTIC(&temp_characteristic_uuid.uuid,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          read_temp_cb, NULL, NULL),
    BT_GATT_CCC(temp_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

// Connection callbacks
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
    // Stop temperature reading on disconnect
    temp_reading_active = false;
    k_work_cancel_delayable(&temp_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Bluetooth initialization
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, CUSTOM_SERVICE_UUID),
};

static void bt_ready(void)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
    printk("Advertising successfully started\n");
}

int main(void)
{
    int err;

    // Initialize I2C
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus is not ready!\n");
        return -1;
    }

    // Initialize work queue for temperature readings
    k_work_init_delayable(&temp_work, read_temperature);

    // Initialize Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }

    printk("Bluetooth initialized\n");
    
    // Initialize handles BEFORE starting advertising
    init_handles();
    printk("BLE handles initialized\n");

    bt_ready();
    printk("Advertising successfully started\n");

    // Main loop
    while (1) {
        k_sleep(K_FOREVER);
    }
    
    return 0;
}