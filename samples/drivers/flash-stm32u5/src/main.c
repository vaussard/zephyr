#include <zephyr/drivers/flash.h>
#include <zephyr/random/rand32.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

/* On STM32U575 (2 MB) */
#define PAGE_BANK1		0x0FE000 /* last page of bank1 */
#define PAGE_BANK2		0x1FE000 /* last page of bank2 */
#define PAGE_SIZE		KB(8)

struct test_param {
	off_t page;
	size_t write_size;
};

const struct test_param params[] = {
	{
		/* flash_write() should return an error */
		.page = PAGE_BANK1,
		.write_size = 8,
	},
	{
		/* flash_write() should return an error */
		.page = PAGE_BANK2,
		.write_size = 8,
	},
	{
		/* should succeed */
		.page = PAGE_BANK1,
		.write_size = 16,
	},
	{
		/* should succeed */
		.page = PAGE_BANK2,
		.write_size = 16,
	},
};

static int test_flash(const struct device *flash_dev,
		       off_t page, size_t page_size,
		       const uint8_t *data, size_t data_len,
		       size_t write_size)
{
	uint8_t read_buffer[data_len];
	int err;

	/* Erase page */
	err = flash_erase(flash_dev, page, page_size);
	if (err) {
		printk("Erase error %d\n", err);
		return err;
	}

	/* Write in reverse order */
	for (off_t offset = data_len - write_size; offset >= 0; offset -= write_size) {
		printk(" - Writing %zu bytes at offset %ld\n", write_size, offset);
		err = flash_write(flash_dev, page + offset, data + offset, write_size);
		if (err) {
			printk(" -> Write error %d\n", err);
			return err;
		}
	}

	/* Readback */
	err = flash_read(flash_dev, page, read_buffer, sizeof(read_buffer));
	if (err) {
		printk("Read error %d\n", err);
		return err;
	}

	/* Compare */
	for (unsigned int i = 0; i < sizeof(read_buffer); i++) {
		if (read_buffer[i] != data[i]) {
			printk(" -> Error [%u] = %u (expected %u)\n", i, read_buffer[i], data[i]);
			err = -EINVAL;
		}
	}

	return err;
}

int main(void)
{
	const struct device *flash_dev;
	uint8_t data[32];
	int err;

	flash_dev = DEVICE_DT_GET(DT_NODELABEL(flash));
	if (flash_dev == NULL) {
		printk("Flash driver was not found!\n");
		return -ENODEV;
	}

	printk("Write block size: %zu\n", flash_get_write_block_size(flash_dev));

	for (unsigned int i = 0; i < ARRAY_SIZE(params); i++) {
		off_t offset = params[i].page;
		size_t write_size = params[i].write_size;

		/* new random data for each test */
		for (unsigned int i = 0; i < sizeof(data); i++) {
			data[i] = sys_rand32_get() & 0xff;
		}

		printk("\nTesting offset %ld, write size %zu:\n", offset, write_size);
		printk("---\n");
		err = test_flash(flash_dev, offset, PAGE_SIZE, data, sizeof(data), write_size);
		if (err) {
			printk("FAIL\n");
		} else {
			printk("PASS\n");
		}
	}
}
