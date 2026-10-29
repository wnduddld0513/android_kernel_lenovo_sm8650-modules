// SPDX-License-Identifier: GPL-2.0-only
/*
 * (C) 2026 The LineageOS Project
 *
 * ETEK ET5907 camera LDO PMIC, supplied through vin1.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>

#include "cam_ldo.h"

static const struct cam_ldo_reg_val et5907_table[] = {
	{ 0x04, 0x66 }, { 0x05, 0x66 }, { 0x06, 0xa0 }, { 0x07, 0xa0 },
	{ 0x08, 0xa0 }, { 0x09, 0xa0 }, { 0x0a, 0x41 }, { 0x05, 0x55 },
};

static const struct cam_ldo_info et5907_info = {
	.name = "et5907",
	.en_reg = 0x03,
	.id_reg = 0x00,
	.id = { 0x01, 0x01 },
	.port = {
		{ 0x04, 0x66, 0x01 },
		{ 0x05, 0x66, 0x02 },
		{ 0x06, 0xa0, 0x04 },
		{ 0x07, 0xa0, 0x08 },
		{ 0x08, 0xa0, 0x10 },
		{ 0x09, 0xa0, 0x20 },
		{ 0x0a, 0x41, 0x40 },
		{ 0x05, 0x55, 0x02 },
	},
	.table = et5907_table,
	.table_len = ARRAY_SIZE(et5907_table),
	.settle = true,
};

static struct cam_ldo_chip *et5907_chip;

int et5907_camera_power_up(int port)
{
	if (!et5907_chip)
		return -1;

	return cam_ldo_power_up(et5907_chip, port);
}
EXPORT_SYMBOL(et5907_camera_power_up);

int et5907_camera_power_down(int port)
{
	if (!et5907_chip)
		return -1;

	return cam_ldo_power_down(et5907_chip, port);
}
EXPORT_SYMBOL(et5907_camera_power_down);

int et5907_camera_power_up_all(void)
{
	if (!et5907_chip)
		return -1;

	return cam_ldo_write_table(et5907_chip);
}
EXPORT_SYMBOL(et5907_camera_power_up_all);

static int et5907_probe(struct i2c_client *client)
{
	struct cam_ldo_chip *chip;
	int rc;

	chip = cam_ldo_chip_create(client, &et5907_info);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	rc = cam_ldo_vin_enable(chip, false);
	if (rc < 0) {
		dev_err(chip->dev, "enable power failed\n");
		return rc;
	}

	rc = cam_ldo_init(chip);
	if (rc < 0) {
		dev_err(chip->dev, "et5907 init fail!\n");
		return -ENODEV;
	}

	rc = cam_ldo_sysfs_create(chip);
	if (rc < 0) {
		dev_info(chip->dev, "%s error creating sysfs attr files\n",
			 __func__);
		return rc;
	}

	et5907_chip = chip;

	return 0;
}

static void et5907_remove(struct i2c_client *client)
{
	et5907_chip = NULL;
}

static const struct i2c_device_id et5907_id_table[] = {
	{ "vendor,et5907", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, et5907_id_table);

static const struct of_device_id et5907_of_match[] = {
	{ .compatible = "vendor,et5907" },
	{ .compatible = "vendor,et5907s" },
	{ }
};
MODULE_DEVICE_TABLE(of, et5907_of_match);

static struct i2c_driver et5907_driver = {
	.driver = {
		.name = "et5907",
		.of_match_table = et5907_of_match,
	},
	.probe_new = et5907_probe,
	.remove = et5907_remove,
	.id_table = et5907_id_table,
};

int cam_et5907_driver_init(void)
{
	return i2c_add_driver(&et5907_driver);
}

void cam_et5907_driver_exit(void)
{
	i2c_del_driver(&et5907_driver);
}
