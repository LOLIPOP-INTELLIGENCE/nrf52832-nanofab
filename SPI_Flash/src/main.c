#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/spi.h>
#include <stdio.h>
#include <string.h>

#define TEST_OFFSET     0x1000
#define TEST_BUF_SIZE   32
#define FLASH_SIZE      0x80000    // 512KB
#define SECTOR_SIZE     4096       // 4KB sector size for AT25SF041

static const struct device *flash_dev;
static uint8_t test_data[TEST_BUF_SIZE] = "SPI Flash Test Data!";
static uint8_t read_back[TEST_BUF_SIZE];

static void flash_test(void)
{
    int ret;
    
    /* Initialize buffers */
    memset(read_back, 0, sizeof(read_back));

    /* Verify flash device is ready */
    if (!device_is_ready(flash_dev)) {
        printk("Flash device not ready!\n");
        return;
    }
    printk("Flash device ready\n");

    /* Get flash device info */
    // const struct flash_parameters *flash_params = flash_get_parameters(flash_dev);
    // if (flash_params) {
    //     printk("Flash size: %zu bytes\n", flash_params->size);
    //     printk("Page size: %zu bytes\n", flash_params->page_size);
    // }

    /* Erase sector */
    printk("Erasing sector at offset 0x%x...\n", TEST_OFFSET);
    ret = flash_erase(flash_dev, TEST_OFFSET, SECTOR_SIZE);
    if (ret != 0) {
        printk("Flash erase failed! (err: %d)\n", ret);
        return;
    }
    printk("Sector erased successfully\n");

    /* Write data */
    printk("Writing data to offset 0x%x...\n", TEST_OFFSET);
    ret = flash_write(flash_dev, TEST_OFFSET, test_data, sizeof(test_data));
    if (ret != 0) {
        printk("Flash write failed! (err: %d)\n", ret);
        return;
    }
    printk("Data written successfully\n");

    /* Read data back */
    printk("Reading data from offset 0x%x...\n", TEST_OFFSET);
    ret = flash_read(flash_dev, TEST_OFFSET, read_back, sizeof(read_back));
    if (ret != 0) {
        printk("Flash read failed! (err: %d)\n", ret);
        return;
    }

    /* Verify data */
    if (memcmp(test_data, read_back, sizeof(test_data)) == 0) {
        printk("Data verification successful!\n");
        printk("Read back: %s\n", read_back);
    } else {
        printk("Data verification failed!\n");
        printk("Expected: %s\n", test_data);
        printk("Got: %s\n", read_back);
    }
}

void main(void)
{
    printk("SPI Flash example started\n");

    /* Get flash device */
    flash_dev = DEVICE_DT_GET(DT_NODELABEL(at25sf041));
    
    /* Add initialization delay */
    k_sleep(K_MSEC(100));

    /* Run flash test */
    flash_test();

    printk("Test complete\n");
}