#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define PMOD_IA_NODE DT_NODELABEL(pmod_ia)

// AD5933 Register addresses
#define START_FREQ_REG     0x82    // Start frequency register
#define CTRL_REG_HB       0x80    // Control Register High Byte
#define CTRL_REG_LB       0x81    // Control Register Low Byte
#define STATUS_REG        0x8F    // Status Register
#define TEMP_REG          0x92    // Temperature Register

void main(void)
{
    static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(PMOD_IA_NODE);
    int ret;
    
    printk("=== AD5933 I2C Test ===\n");
    
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus is not ready!\n");
        return;
    }

    // Test 1: Read Temperature
    // First, set control register to measure temperature
    uint8_t temp_cmd[2] = {CTRL_REG_HB, 0x90};  // Command to measure temperature
    ret = i2c_write_dt(&dev_i2c, temp_cmd, 2);
    if (ret != 0) {
        printk("Failed to write temperature command\n");
        return;
    }
    
    // Wait a bit for temperature measurement
    k_msleep(100);
    
    // Read temperature data (2 bytes)
    uint8_t temp_reg = TEMP_REG;
    uint8_t temp_data[2];
    ret = i2c_write_read_dt(&dev_i2c, &temp_reg, 1, temp_data, 2);
    if (ret == 0) {
        int16_t temp_raw = (temp_data[0] << 8) | temp_data[1];
        // Convert to actual temperature
        float temp_c;
        if (temp_raw < 8192) {
            temp_c = temp_raw / 32.0f;
        } else {
            temp_raw -= 16384;
            temp_c = temp_raw / 32.0f;
        }
        printk("Raw temperature: 0x%04X\n", (uint16_t)temp_raw);
        printk("Temperature: %.2f C\n", temp_c);
    }

    // Test 2: Write/Read Register Test
    // Let's write to start frequency register (which is fully writable)
    uint8_t write_data[2] = {START_FREQ_REG, 0x55};  // Write 0x55 as test value
    ret = i2c_write_dt(&dev_i2c, write_data, 2);
    if (ret != 0) {
        printk("Failed to write test value\n");
        return;
    }

    // Read back the value
    uint8_t read_reg = START_FREQ_REG;
    uint8_t read_data;
    ret = i2c_write_read_dt(&dev_i2c, &read_reg, 1, &read_data, 1);
    if (ret == 0) {
        printk("Write/Read Test - Wrote: 0x55, Read back: 0x%02X\n", read_data);
        if (read_data == 0x55) {
            printk("Write/Read Test PASSED! ✅\n");
        } else {
            printk("Write/Read Test FAILED - values don't match\n");
        }
    }
}