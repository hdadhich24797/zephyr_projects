/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 * Copyright (c) 2020 Endian Technologies AB
 * Copyright (c) 2020 Kim Bøndergaard <kim@fam-boendergaard.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim,max7219

//#include "display_max7219.h"

#include <device.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <drivers/display.h>
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
//#include <soc.h>
#include <dt-bindings/spi/spi.h>
#include <drivers/spi.h>
#include <string.h>

struct max7219_config {
	const char *spi_name;
	struct spi_config spi_config;
	uint16_t height;
	uint16_t width;
};

struct max7219_data {
	const struct max7219_config *config;
	const struct device *spi_dev;
};

static int display_blanking_on()
{
    return 0;
}

static int display_blanking_off()
{
    return 0;
}

static int display_write(const struct device *dev,
			 uint16_t length,
			 const void *buf)
{
	struct max7219_data *data = (struct max7219_data *)dev->data;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
    
	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;

    for (int i = 0; i < length; i++)
    {
		ret = spi_write(data->spi_dev, &data->config->spi_config, &tx_bufs);
		if (ret < 0) {
			return ret;
		}
    }
}


static const struct display_driver_api max7219_api = {
	.blanking_on = display_blanking_on,
	.blanking_off = display_blanking_off,
	.write = display_write,
};
static int max7219_pm_control()
{
    return 0;
}

#define MAX7219_INIT(inst)							\
	static struct max7219_data max7219_data_ ## inst;			\
										\
	const static struct max7219_config max7219_config_ ## inst = {		\
		.spi_name = DT_INST_BUS_LABEL(inst),				\
		.spi_config.slave = DT_INST_REG_ADDR(inst),			\
		.spi_config.frequency = UTIL_AND(				\
			DT_HAS_PROP(inst, spi_max_frequency),			\
			DT_INST_PROP(inst, spi_max_frequency)),			\
		.spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8),	\
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
	DEVICE_DT_INST_DEFINE(inst, max7219_init, max7219_pm_control,		\
			      &max7219_data_ ## inst, &max7219_config_ ## inst,	\
			      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY,	\
			      &max7219_api);

DT_INST_FOREACH_STATUS_OKAY(MAX7219_INIT)
