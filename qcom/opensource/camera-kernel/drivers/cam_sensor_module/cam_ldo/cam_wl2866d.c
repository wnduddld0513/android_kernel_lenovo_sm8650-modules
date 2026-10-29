// SPDX-License-Identifier: GPL-2.0-only
/*
 * (C) 2026 The LineageOS Project
 *
 * Willsemi WL2866D camera LDO PMIC, supplied through vin1/vin2.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>

#include "cam_ldo.h"

static const struct cam_ldo_reg_val wl2866d_table[] = {
	{ 0x03, 0x5a }, { 0x04, 0x5a }, { 0x05, 0xa2 }, { 0x06, 0xa2 },
	{ 0x07, 0xa2 }, { 0x08, 0xa2 }, { 0x09, 0x2c }, { 0x04, 0x4d },
};

static const struct cam_ldo_info wl2866d_info = {
	.name = "wl2866d",
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
	.table = wl2866d_table,
	.table_len = ARRAY_SIZE(wl2866d_table),
	.settle = true,
};

static struct cam_ldo_chip *wl2866d_chip;

int wl2866d_camera_power_up(int port)
{
	if (!wl2866d_chip)
		return -1;

	return cam_ldo_power_up(wl2866d_chip, port);
}
EXPORT_SYMBOL(wl2866d_camera_power_up);

int wl2866d_camera_power_down(int port)
{
	if (!wl2866d_chip)
		return -1;

	return cam_ldo_power_down(wl2866d_chip, port);
}
EXPORT_SYMBOL(wl2866d_camera_power_down);

int wl2866d_camera_power_up_all(void)
{
	if (!wl2866d_chip)
		return -1;

	return cam_ldo_write_table(wl2866d_chip);
}
EXPORT_SYMBOL(wl2866d_camera_power_up_all);

int wl2866d_camera_power_up_eeprom(void)
{
	return wl2866d_camera_power_up_all();
}
EXPORT_SYMBOL(wl2866d_camera_power_up_eeprom);

int wl2866d_camera_power_down_eeprom(void)
{
	if (!wl2866d_chip)
		return -1;

	return cam_ldo_disable_all(wl2866d_chip);
}
EXPORT_SYMBOL(wl2866d_camera_power_down_eeprom);

static int wl2866d_probe(struct i2c_client *client)
{
	struct cam_ldo_chip *chip;
	int rc;

	chip = cam_ldo_chip_create(client, &wl2866d_info);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	rc = cam_ldo_vin_enable(chip, true);
	if (rc < 0) {
		dev_err(chip->dev, "enable power failed\n");
		return rc;
	}

	rc = cam_ldo_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "wl2866d init fail!\n");
		return -ENODEV;
	}

	rc = cam_ldo_sysfs_create(chip);
	if (rc < 0) {
		dev_info(chip->dev, "%s error creating sysfs attr files\n",
			 __func__);
		return rc;
	}

	wl2866d_chip = chip;

	return 0;
}

static void wl2866d_remove(struct i2c_client *client)
{
	wl2866d_chip = NULL;
}

static const struct i2c_device_id wl2866d_id_table[] = {
	{ "vendor,wl2868c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wl2866d_id_table);

static const struct of_device_id wl2866d_of_match[] = {
	{ .compatible = "vendor,wl2868c" },
	{ .compatible = "lenovo,wl2868c" },
	{ }
};
MODULE_DEVICE_TABLE(of, wl2866d_of_match);

static struct i2c_driver wl2866d_driver = {
	.driver = {
		.name = "wl2866d",
		.of_match_table = wl2866d_of_match,
	},
	.probe_new = wl2866d_probe,
	.remove = wl2866d_remove,
	.id_table = wl2866d_id_table,
};

int cam_wl2866d_driver_init(void)
{
	return i2c_add_driver(&wl2866d_driver);
}

void cam_wl2866d_driver_exit(void)
{
	i2c_del_driver(&wl2866d_driver);
}
