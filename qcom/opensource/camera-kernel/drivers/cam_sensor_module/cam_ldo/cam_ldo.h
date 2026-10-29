/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * (C) 2026 The LineageOS Project
 *
 * Camera sensor LDO PMICs of the Lenovo TB520FU (lapis).
 *
 * Re-implementation of the camera LDO power control found in the stock
 * camera.ko (Lenovo ZUI 18.5.10.037): one table-driven core and three thin
 * I2C drivers (Willsemi WL2868C as wired on lapis, Willsemi WL2866D and ETEK
 * ET5907). A "port" is the index of the LDO in the sequence type
 * SENSOR_LDO1 .. SENSOR_LDO1P1 of the sensor power settings.
 */

#ifndef _CAM_LDO_H_
#define _CAM_LDO_H_

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>

#define CAM_LDO_NUM_PORTS	8
#define CAM_LDO_LDO7_PORT	6
#define CAM_LDO_NUM_ATTRS	9

/**
 * struct cam_ldo_port - registers controlling one sequence port
 * @vreg:    output voltage register
 * @vval:    voltage code written on power up
 * @en_mask: bit of the enable register
 */
struct cam_ldo_port {
	u8 vreg;
	u8 vval;
	u8 en_mask;
};

struct cam_ldo_reg_val {
	u8 reg;
	u8 val;
};

/**
 * struct cam_ldo_info - description of one LDO PMIC
 * @name:            driver/log name
 * @en_reg:          LDO enable register
 * @id_reg:          chip id register
 * @id:              accepted chip ids
 * @port:            sequence port to register mapping
 * @table:           register writes done by the "power up all" entry points
 * @table_len:       number of entries of @table
 * @settle:          wait and dump the registers after the initialisation
 * @ignore_init_err: carry on when the initial register writes fail
 */
struct cam_ldo_info {
	const char *name;
	u8 en_reg;
	u8 id_reg;
	u8 id[2];
	struct cam_ldo_port port[CAM_LDO_NUM_PORTS];
	const struct cam_ldo_reg_val *table;
	int table_len;
	bool settle;
	bool ignore_init_err;
};

struct cam_ldo_attr {
	struct device_attribute attr;
	u8 reg;
};

struct cam_ldo_chip {
	struct device *dev;
	struct i2c_client *client;
	const struct cam_ldo_info *info;
	struct regulator *vin1;
	struct regulator *vin2;
	int en_gpio;
	struct mutex lock;
	int ldo7_users;
	struct cam_ldo_attr attrs[CAM_LDO_NUM_ATTRS];
	struct attribute *attr_list[CAM_LDO_NUM_ATTRS + 1];
	struct attribute_group attr_group;
};

struct cam_ldo_chip *cam_ldo_chip_create(struct i2c_client *client,
		const struct cam_ldo_info *info);
int cam_ldo_vin_enable(struct cam_ldo_chip *chip, bool vin2);
int cam_ldo_init(struct cam_ldo_chip *chip);
int cam_ldo_sysfs_create(struct cam_ldo_chip *chip);

int cam_ldo_write(struct cam_ldo_chip *chip, u8 reg, u8 val);
int cam_ldo_read(struct cam_ldo_chip *chip, u8 reg, u8 *val);
int cam_ldo_power_up(struct cam_ldo_chip *chip, int port);
int cam_ldo_power_down(struct cam_ldo_chip *chip, int port);
int cam_ldo_write_table(struct cam_ldo_chip *chip);
int cam_ldo_disable_all(struct cam_ldo_chip *chip);

/* Entry points of the individual chips, exported for other camera modules */
int lapis_wl2868c_camera_power_up(int port);
int lapis_wl2868c_camera_power_down(int port);
int lapis_wl2868c_camera_power_up_all(void);
int lapis_wl2868c_camera_power_up_eeprom(void);
int lapis_wl2868c_camera_power_down_eeprom(void);
int wl2866d_camera_power_up(int port);
int wl2866d_camera_power_down(int port);
int wl2866d_camera_power_up_all(void);
int wl2866d_camera_power_up_eeprom(void);
int wl2866d_camera_power_down_eeprom(void);
int et5907_camera_power_up(int port);
int et5907_camera_power_down(int port);
int et5907_camera_power_up_all(void);

/* Whichever of the LDO PMICs is present, see cam_ldo_core.c */
int cam_ldo_camera_power_up(int port);
int cam_ldo_camera_power_down(int port);

int cam_lapis_wl2868c_driver_init(void);
void cam_lapis_wl2868c_driver_exit(void);
int cam_wl2866d_driver_init(void);
void cam_wl2866d_driver_exit(void);
int cam_et5907_driver_init(void);
void cam_et5907_driver_exit(void);

#endif /* _CAM_LDO_H_ */
