#define DT_DRV_COMPAT maxim_max7219

#include <device.h>
#include <drivers/spi.h>
#include <drivers/display.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <zephyr.h>
#include <devicetree.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <stdlib.h>
#include <fsl_iomuxc.h>
#include <drivers/pwm.h>
#include <devicetree/pwms.h>
#include <drivers/spi.h>
#include <string.h>

/*  Config struct for max7219   */
struct max7219_config {
	const char *spi_name;
	struct spi_config spi_config;
	uint16_t height;
	uint16_t width;
};
/*  Data struct for max7219    */
struct max7219_data {
	const struct max7219_config *config;
	const struct device *spi_dev;
};

/*  Init max7219    */
static int max7219_init(const struct device *dev)
{
	struct max7219_data *data = (struct max7219_data *)dev->data;
	struct max7219_config *config = (struct max7219_config *)dev->config;

	data->spi_dev = device_get_binding(config->spi_name);
	if (data->spi_dev == NULL) {
		return -1;
	}
    return 0;
}

/*  Max7219 blinking on function    */
static int display_blanking_on_my(const struct device *dev)
{
    struct max7219_data *data = (struct max7219_data *)dev->data;
    int cmd = 0x0c00;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
    tx_buf.buf = &cmd;
    tx_buf.len = 2;
	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;
    return spi_write(data->spi_dev, &data->config->spi_config, &tx_bufs);
}

/*  Max7219 blinking off function    */
static int display_blanking_off_my(const struct device *dev)
{
    struct max7219_data *data = (struct max7219_data *)dev->data;
    int cmd = 0x0c01;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
    tx_buf.buf = &cmd;
    tx_buf.len = 2;
	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;
    return spi_write(data->spi_dev, &data->config->spi_config, &tx_bufs);
}

/*  Max7219 display function    */
static int display_write_my(const struct device *dev,
			 const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
    printk("Inside display\n");

    int ret = 0;
	uint16_t *arr = (uint16_t *)buf;
    int len = y;

    struct max7219_data *data = (struct max7219_data *)dev->data;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;
    for (int i = 0; i < len; i++)
    {
        printk("%x\r\n", arr[i]);

        tx_buf.buf = &arr[i];
        tx_buf.len = 2;
        ret = spi_write(data->spi_dev, &data->config->spi_config, &tx_bufs);
    }
    return ret;
}

/*  APIS for MAX7219    */
static const struct display_driver_api max7219_api = {
	.blanking_on = display_blanking_on_my,
	.blanking_off = display_blanking_off_my,
	.write = display_write_my,
};

#define MAX7219_INIT(inst)							\
	static struct max7219_data max7219_data_ ## inst;			\
										\
	const static struct max7219_config max7219_config_ ## inst = {		\
		.spi_name = DT_INST_BUS_LABEL(inst),				\
		.spi_config.slave = DT_INST_REG_ADDR(inst),			\
		.spi_config.frequency = UTIL_AND(				\
			DT_HAS_PROP(inst, spi_max_frequency),			\
			DT_INST_PROP(inst, spi_max_frequency)),			\
		.spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(16) | SPI_MODE_CPOL | SPI_TRANSFER_MSB,	\
		.spi_config.cs = UTIL_AND(					\
			DT_INST_SPI_DEV_HAS_CS_GPIOS(inst),			\
			&(max7219_data_ ## inst.cs_ctrl)),			\
		.width = DT_INST_PROP(inst, width),				\
		.height = DT_INST_PROP(inst, height),				\
	};									\
										\
	static struct max7219_data max7219_data_ ## inst = {			\
		.config = &max7219_config_ ## inst,				\
	};									\
	DEVICE_DT_INST_DEFINE(inst, max7219_init, NULL,		\
			      &max7219_data_ ## inst, &max7219_config_ ## inst,	\
			      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY,	\
			      &max7219_api);

DT_INST_FOREACH_STATUS_OKAY(MAX7219_INIT)
