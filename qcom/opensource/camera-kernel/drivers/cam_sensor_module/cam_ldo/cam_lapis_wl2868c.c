// SPDX-License-Identifier: GPL-2.0-only
/*
 * (C) 2026 The LineageOS Project
 *
 * Willsemi WL2868C camera LDO PMIC as wired on the Lenovo TB520FU (lapis):
 * I2C address 0x2f of the qupv3_se6 bus, enable GPIO "en-gpio".
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "cam_ldo.h"

static const struct cam_ldo_reg_val lapis_wl2868c_table[] = {
	{ 0x03, 0x5a }, { 0x04, 0x5a }, { 0x05, 0xa2 }, { 0x06, 0xa2 },
	{ 0x07, 0xa2 }, { 0x08, 0xa2 }, { 0x09, 0x2c }, { 0x11, 0x80 },
	{ 0x13, 0x80 }, { 0x15, 0x80 }, { 0x17, 0x80 }, { 0x19, 0x80 },
	{ 0x1b, 0x80 }, { 0x1d, 0x80 }, { 0x0e, 0x80 },
};

static const struct cam_ldo_info lapis_wl2868c_info = {
	.name = "lapis_wl2868c",
	.en_reg = 0x0e,
	.id_reg = 0x00,
	.id = { 0x82, 0x33 },
	.port = {
		{ 0x03, 0x5a, 0x01 },
		{ 0x04, 0x5a, 0x02 },
		{ 0x05, 0xa2, 0x04 },
		{ 0x06, 0xa2, 0x08 },
		{ 0x07, 0xa2, 0x10 },
		{ 0x08, 0xa2, 0x20 },
		{ 0x09, 0x2c, 0x40 },
		{ 0x04, 0x4d, 0x02 },
	},
	.table = lapis_wl2868c_table,
	.table_len = ARRAY_SIZE(lapis_wl2868c_table),
	.ignore_init_err = true,
};

static struct cam_ldo_chip *lapis_wl2868c_chip;

int lapis_wl2868c_camera_power_up(int port)
{
	if (!lapis_wl2868c_chip)
		return -1;

	return cam_ldo_power_up(lapis_wl2868c_chip, port);
}
EXPORT_SYMBOL(lapis_wl2868c_camera_power_up);

int lapis_wl2868c_camera_power_down(int port)
{
	if (!lapis_wl2868c_chip)
		return -1;

	return cam_ldo_power_down(lapis_wl2868c_chip, port);
}
EXPORT_SYMBOL(lapis_wl2868c_camera_power_down);

int lapis_wl2868c_camera_power_up_all(void)
{
	if (!lapis_wl2868c_chip)
		return -1;

	return cam_ldo_write_table(lapis_wl2868c_chip);
}
EXPORT_SYMBOL(lapis_wl2868c_camera_power_up_all);

int lapis_wl2868c_camera_power_up_eeprom(void)
{
	return lapis_wl2868c_camera_power_up_all();
}
EXPORT_SYMBOL(lapis_wl2868c_camera_power_up_eeprom);

int lapis_wl2868c_camera_power_down_eeprom(void)
{
	if (!lapis_wl2868c_chip)
		return -1;

	return cam_ldo_disable_all(lapis_wl2868c_chip);
}
EXPORT_SYMBOL(lapis_wl2868c_camera_power_down_eeprom);

static int lapis_wl2868c_probe(struct i2c_client *client)
{
	struct cam_ldo_chip *chip;
	int rc;

	chip = cam_ldo_chip_create(client, &lapis_wl2868c_info);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	chip->en_gpio = of_get_named_gpio(client->dev.of_node, "en-gpio", 0);
	if (!gpio_is_valid(chip->en_gpio)) {
		dev_err(chip->dev, "%s: en gpio not specified\n", __func__);
		return -EINVAL;
	}

	rc = devm_gpio_request(chip->dev, chip->en_gpio, "lapis_wl2868c_en");
	if (rc < 0) {
		dev_err(chip->dev, "enable gpio request failed\n");
		return rc;
	}

	/* enable the PMIC, pulse the enable line to restart it */
	gpiod_direction_output_raw(gpio_to_desc(chip->en_gpio), 1);
	gpiod_set_raw_value_cansleep(gpio_to_desc(chip->en_gpio), 0);
	msleep(2);
	gpiod_set_raw_value_cansleep(gpio_to_desc(chip->en_gpio), 1);
	msleep(3);

	rc = cam_ldo_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "lapis_wl2868c init fail!\n");
		return -ENODEV;
	}

	rc = cam_ldo_sysfs_create(chip);
	if (rc < 0) {
		dev_info(chip->dev, "%s error creating sysfs attr files\n",
			 __func__);
		return rc;
	}

	lapis_wl2868c_chip = chip;

	return 0;
}

static void lapis_wl2868c_remove(struct i2c_client *client)
{
	lapis_wl2868c_chip = NULL;
}

static const struct i2c_device_id lapis_wl2868c_id_table[] = {
	{ "qcom,ic_wl2868c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lapis_wl2868c_id_table);

static const struct of_device_id lapis_wl2868c_of_match[] = {
	{ .compatible = "qcom,ic_wl2868c" },
	{ }
};
MODULE_DEVICE_TABLE(of, lapis_wl2868c_of_match);

static struct i2c_driver lapis_wl2868c_driver = {
	.driver = {
		.name = "lapis_wl2868c",
		.of_match_table = lapis_wl2868c_of_match,
	},
	.probe_new = lapis_wl2868c_probe,
	.remove = lapis_wl2868c_remove,
	.id_table = lapis_wl2868c_id_table,
};

int cam_lapis_wl2868c_driver_init(void)
{
	return i2c_add_driver(&lapis_wl2868c_driver);
}

void cam_lapis_wl2868c_driver_exit(void)
{
	i2c_del_driver(&lapis_wl2868c_driver);
}
