#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>

#define PMODIA_ADDR          0x0D
#define PMODIA_NODE          DT_NODELABEL(pmodia)

// Register addresses
#define CTRL_REG_HIGH        0x80
#define CTRL_REG_LOW         0x81
#define STATUS_REG           0x8F
#define TEMP_DATA_HIGH       0x92
#define TEMP_DATA_LOW        0x93

// Command bits
#define CMD_MEASURE_TEMP     0x90    // Measure temperature command

static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(PMODIA_NODE);

// Write a byte to a register
static int write_register(const struct i2c_dt_spec *dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    int ret = i2c_write_dt(dev, buf, sizeof(buf));
    if (ret != 0) {
        printk("Failed to write to register 0x%02X: %d\n", reg, ret);
    }
    return ret;
}

// Read a byte from a register
static int read_register(const struct i2c_dt_spec *dev, uint8_t reg, uint8_t *value)
{
    int ret = i2c_write_dt(dev, &reg, 1);
    if (ret != 0) {
        printk("Failed to write register address 0x%02X: %d\n", reg, ret);
        return ret;
    }

    ret = i2c_read_dt(dev, value, 1);
    if (ret != 0) {
        printk("Failed to read from register 0x%02X: %d\n", reg, ret);
    }
    return ret;
}

// Convert raw temperature data to celsius
float convert_to_celsius(uint16_t raw_temp)
{
    // The temperature value is 14-bit twos complement
    // First, mask off the don't care bits and extract the 14-bit value
    int16_t temp_val = (raw_temp & 0x3FFF);
    
    // If the sign bit (bit 13) is set, extend the sign
    if (temp_val & 0x2000) {
        temp_val |= 0xC000;  // Extend sign bit
    }
    
    // Each LSB = 1/32 degrees C
    return temp_val / 32.0f;
}

// Read temperature from the sensor
int read_temperature(const struct i2c_dt_spec *dev, float *temp_celsius)
{
    int ret;
    uint8_t status;
    uint8_t temp_high, temp_low;
    uint16_t raw_temp;

    // Step 1: Start temperature measurement
    printk("Starting temperature measurement...\n");
    ret = write_register(dev, CTRL_REG_HIGH, CMD_MEASURE_TEMP);
    if (ret != 0) {
        return ret;
    }

    // Step 2: Wait for measurement completion (poll status register)
    // Typically takes 800μs but we'll poll with a timeout
    printk("Waiting for measurement completion...\n");
    int timeout = 100;  // Number of attempts
    do {
        k_sleep(K_MSEC(1));  // Wait 1ms between polls
        ret = read_register(dev, STATUS_REG, &status);
        if (ret != 0) {
            return ret;
        }
        if (--timeout == 0) {
            printk("Timeout waiting for temperature measurement\n");
            return -ETIMEDOUT;
        }
    } while (!(status & 0x01));  // Bit 0 indicates valid temperature

    // Step 3: Read temperature data
    printk("Reading temperature data...\n");
    ret = read_register(dev, TEMP_DATA_HIGH, &temp_high);
    if (ret != 0) {
        return ret;
    }

    ret = read_register(dev, TEMP_DATA_LOW, &temp_low);
    if (ret != 0) {
        return ret;
    }

    // Combine the bytes into a 16-bit value
    raw_temp = (temp_high << 8) | temp_low;
    
    // Convert to celsius
    *temp_celsius = convert_to_celsius(raw_temp);
    return 0;
}

int main(void)
{
    float temperature;
    int ret;

    printk("Starting AD5933 Temperature Sensor Test\n");

    // Check if I2C is ready
    if (!device_is_ready(dev_i2c.bus)) {
        printk("I2C bus is not ready!\n");
        return -1;
    }
    printk("I2C bus is ready\n");

    // Read temperature
    ret = read_temperature(&dev_i2c, &temperature);
    if (ret == 0) {
        printk("Temperature: %.2f °C\n", temperature);
    } else {
        printk("Failed to read temperature: %d\n", ret);
    }

    return 0;
}