#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define MAX30205_NODE DT_NODELABEL(max30205)
#define MAX30205_TEMP_REG 0x00
#define MAX30205_CONFIG_REG 0x01

void main(void)
{
    static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(MAX30205_NODE);
    int ret;
    
    printk("=== MAX30205 Temperature Sensor Test ===\n");
    
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus is not ready!\n");
        return;
    }

    while (1) {
        // Read temperature data (2 bytes)
        uint8_t temp_reg = MAX30205_TEMP_REG;
        uint8_t temp_data[2];
        ret = i2c_write_read_dt(&dev_i2c, &temp_reg, 1, temp_data, 2);
        
        if (ret == 0) {
            // Convert to temperature
            // MAX30205 sends temperature in two's complement format
            int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
            float temp_c = temp_raw * 0.00390625f; // 2^-8 = 0.00390625
            
            printk("Raw temperature: 0x%04X\n", (uint16_t)temp_raw);
            printk("Temperature: %.3f C\n", temp_c);
        } else {
            printk("Failed to read temperature\n");
        }

        // Wait for 2 seconds before next reading
        k_sleep(K_SECONDS(2));
    }
}