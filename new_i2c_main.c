#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define PMOD_IA_NODE DT_NODELABEL(pmod_ia)
#define STATUS_REG 0x8F

void main(void)
{
    // Get the I2C device specification
    static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(PMOD_IA_NODE);
    
    printk("Starting I2C test\n");
    printk("I2C Address: 0x%02X\n", dev_i2c.addr);
    
    // Check if I2C bus is ready
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus is not ready!\n");
        return;
    }
    printk("I2C bus is ready\n");

    // First try to reset the device
    uint8_t reset_reg = 0x81;  // Control register low byte
    uint8_t reset_val = 0x10;  // Reset bit
    int ret = i2c_write_dt(&dev_i2c, &reset_val, 1);
    if (ret != 0) {
        printk("Failed to write reset command: %d\n", ret);
    } else {
        printk("Reset command sent successfully\n");
    }

    // Wait a bit after reset
    k_msleep(100);

    // Try to read status register
    uint8_t reg = STATUS_REG;
    uint8_t data;
    ret = i2c_write_read_dt(&dev_i2c, &reg, 1, &data, 1);
    
    if (ret != 0) {
        printk("Failed to read status register: %d\n", ret);
    } else {
        printk("Status register value: 0x%02X\n", data);
    }

    // For debugging, let's also try to read temperature
    reg = 0x92;  // Temperature register
    ret = i2c_write_read_dt(&dev_i2c, &reg, 1, &data, 1);
    if (ret != 0) {
        printk("Failed to read temperature register: %d\n", ret);
    } else {
        printk("Temperature register value: 0x%02X\n", data);
    }
}