// SPDX-License-Identifier: GPL-2.0-only
/*
 * (C) 2026 The LineageOS Project
 *
 * Common code of the camera sensor LDO PMIC drivers, see cam_ldo.h.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "cam_ldo.h"

#define CAM_LDO_VIN1_UV		1856000
#define CAM_LDO_VIN2_UV		3296000

/* Log wording of the power sequences of one port */
struct cam_ldo_msg {
	const char *up_vol_fail;
	const char *up_read_fail;
	const char *up_enable_fail;
	const char *up_ok;
	const char *down_read_fail;
	const char *down_enable_fail;
	const char *down_ok;
};

static const struct cam_ldo_msg cam_ldo_msgs[CAM_LDO_NUM_PORTS] = {
	{
		" set ldo1 failed", " read ldo1 enable failed",
		" set ldo1 enable failed", " set LDO1 success!",
		" ldo1 read enable failed", " ldo1 set enable failed",
		" disable ldo1 success!",
	}, {
		" set ldo2 failed", " ldo2 read enable failed",
		" ldo2 set enable failed", " set LDO2 success!",
		" ldo2 read enable failed", " ldo2 set enable failed",
		" disable ldo2 success!",
	}, {
		" set ldo3 failed", " read enable failed",
		" ldo3 set enable failed", " set LDO3 success!",
		" LDO3 read enable failed", " ldo3 set enable failed",
		" disable LDO3 success!",
	}, {
		" set ldo4 failed", " read enable failed",
		" set enable failed", " set LDO4 success!",
		" LDO4 read enable failed", " ldo4 set enable failed",
		" disable LDO4 success!",
	}, {
		" set LDO5 failed", " LDO5 read enable failed",
		" LDO5 set enable failed", " set LDO5 success!",
		" LDO5 read enable failed", " set enable failed",
		" disable LDO5 success!",
	}, {
		" set LDO6 failed", " LDO6 read enable failed",
		" LDO6 set enable failed", " set LDO6 success!",
		" LDO6 read enable failed", " LDO6 set enable failed",
		" disable LDO6 success!",
	}, {
		" LDO7 set avdd1 failed", " LDO7 read enable failed",
		" ldo7 set enable failed", " set LDO7 success!",
		" LDO7 read enable failed", " LDO7 set enable failed",
		" disable LDO7 success!",
	}, {
		/* LDO1P1 lives on the register pair of LDO1 */
		" set ldo1 failed", " read ldo1 enable failed",
		" set ldo1 enable failed", " set LDO1 success!",
		" ldo1 read enable failed", " ldo1 set enable failed",
		" disable ldo1 success!",
	},
};

int cam_ldo_write(struct cam_ldo_chip *chip, u8 reg, u8 val)
{
	int rc = 0;
	int cnt;

	for (cnt = 0; cnt < 2; cnt++) {
		rc = i2c_smbus_write_byte_data(chip->client, reg, val);
		if (rc >= 0)
			return 0;
		pr_err("%s_i2c_write: i2c_write cnt=%d error=%d\n",
		       chip->info->name, cnt, rc);
		msleep(2);
	}

	return rc;
}

int cam_ldo_read(struct cam_ldo_chip *chip, u8 reg, u8 *val)
{
	int rc = 0;
	int cnt;

	for (cnt = 0; cnt < 2; cnt++) {
		rc = i2c_smbus_read_byte_data(chip->client, reg);
		if (rc >= 0) {
			*val = rc;
			return 0;
		}
		pr_err("%s_i2c_read: i2c_read cnt=%d error=%d\n",
		       chip->info->name, cnt, rc);
		msleep(2);
	}

	return rc;
}

/* Read-modify-write of the LDO enable register */
static int cam_ldo_update_enable(struct cam_ldo_chip *chip, u8 set, u8 clear,
		const char *read_fail, const char *write_fail)
{
	u8 val = 0;
	int rc;

	rc = cam_ldo_read(chip, chip->info->en_reg, &val);
	if (rc < 0) {
		pr_err("%s%s\n", chip->info->name, read_fail);
		return rc;
	}

	rc = cam_ldo_write(chip, chip->info->en_reg, (val | set) & ~clear);
	if (rc < 0)
		pr_err("%s%s\n", chip->info->name, write_fail);

	return rc;
}

int cam_ldo_power_up(struct cam_ldo_chip *chip, int port)
{
	const struct cam_ldo_msg *m;
	const struct cam_ldo_port *p;
	int rc;

	if (port < 0 || port >= CAM_LDO_NUM_PORTS) {
		pr_err("%s unknown port!!!\n", chip->info->name);
		rc = -1;
		goto out;
	}

	m = &cam_ldo_msgs[port];
	p = &chip->info->port[port];

	mutex_lock(&chip->lock);
	rc = cam_ldo_write(chip, p->vreg, p->vval);
	if (rc < 0) {
		pr_err("%s%s\n", chip->info->name, m->up_vol_fail);
		goto unlock;
	}

	rc = cam_ldo_update_enable(chip, p->en_mask, 0, m->up_read_fail,
				   m->up_enable_fail);
	if (rc < 0)
		goto unlock;

	/* LDO7 is shared between several sensors */
	if (port == CAM_LDO_LDO7_PORT) {
		chip->ldo7_users++;
		pr_info("%s ldo7 power up count =%d\n", chip->info->name,
			chip->ldo7_users);
	}
	pr_info("%s%s\n", chip->info->name, m->up_ok);
unlock:
	mutex_unlock(&chip->lock);
out:
	pr_info("%s   result = %d\n", chip->info->name, rc);

	return rc;
}

int cam_ldo_power_down(struct cam_ldo_chip *chip, int port)
{
	const struct cam_ldo_msg *m;
	int rc;

	if (port < 0 || port >= CAM_LDO_NUM_PORTS) {
		pr_err("%s unknown camera!!!\n", chip->info->name);
		rc = -1;
		goto out;
	}

	m = &cam_ldo_msgs[port];

	mutex_lock(&chip->lock);
	if (port == CAM_LDO_LDO7_PORT && chip->ldo7_users >= 2) {
		/* another sensor still uses the shared rail */
		chip->ldo7_users--;
		pr_info("%s ldo7 power down count = %d\n", chip->info->name,
			chip->ldo7_users);
		rc = 0;
		goto unlock;
	}

	rc = cam_ldo_update_enable(chip, 0, chip->info->port[port].en_mask,
				   m->down_read_fail, m->down_enable_fail);
	if (rc < 0)
		goto unlock;

	if (port == CAM_LDO_LDO7_PORT) {
		chip->ldo7_users--;
		pr_info("%s ldo7 power down count = %d\n", chip->info->name,
			chip->ldo7_users);
	}
	pr_info("%s%s\n", chip->info->name, m->down_ok);
unlock:
	mutex_unlock(&chip->lock);
out:
	pr_info("%s result = %d\n", chip->info->name, rc);

	return rc;
}

int cam_ldo_write_table(struct cam_ldo_chip *chip)
{
	int i, rc;

	for (i = 0; i < chip->info->table_len; i++) {
		rc = cam_ldo_write(chip, chip->info->table[i].reg,
				   chip->info->table[i].val);
		if (rc < 0) {
			pr_err("%s init voltage failed\n", chip->info->name);
			return rc;
		}
	}

	return 0;
}

int cam_ldo_disable_all(struct cam_ldo_chip *chip)
{
	return cam_ldo_write(chip, chip->info->en_reg, 0);
}

static void cam_ldo_print_reg(struct cam_ldo_chip *chip)
{
	const struct cam_ldo_info *info = chip->info;
	u8 val = 0;
	int i;

	for (i = 0; i < CAM_LDO_NUM_PORTS; i++) {
		cam_ldo_read(chip, info->port[i].vreg, &val);
		pr_err("%s_print_reg:%s info is reg 0x%x, value 0x%x\n",
		       info->name, info->name, info->port[i].vreg, val);
	}
	cam_ldo_read(chip, info->en_reg, &val);
	pr_err("%s_print_reg:%s info is reg 0x%x, value 0x%x\n", info->name,
	       info->name, info->en_reg, val);
}

struct cam_ldo_chip *cam_ldo_chip_create(struct i2c_client *client,
		const struct cam_ldo_info *info)
{
	struct cam_ldo_chip *chip;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return ERR_PTR(-ENOMEM);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed\n");
		return ERR_PTR(-EIO);
	}

	chip->dev = &client->dev;
	chip->client = client;
	chip->info = info;
	chip->en_gpio = -1;
	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	return chip;
}

static void cam_ldo_regulator_disable(void *data)
{
	regulator_disable(data);
}

/*
 * Supply the PMIC input rails, vin1 at 1.856 V and optionally vin2 at 3.296 V.
 * The rails stay enabled while the driver is bound and are released by devm.
 */
int cam_ldo_vin_enable(struct cam_ldo_chip *chip, bool vin2)
{
	int rc;

	chip->vin1 = devm_regulator_get(chip->dev, "vin1");
	if (IS_ERR(chip->vin1)) {
		rc = PTR_ERR(chip->vin1);
		dev_err(chip->dev, "%s_vreg_init: can't get VIN1,%d\n",
			chip->info->name, rc);
		dev_err(chip->dev, "get vreg failed\n");
		return rc;
	}

	if (vin2) {
		chip->vin2 = devm_regulator_get(chip->dev, "vin2");
		if (IS_ERR(chip->vin2)) {
			rc = PTR_ERR(chip->vin2);
			dev_err(chip->dev, "%s_vreg_init: can't get VIN2,%d\n",
				chip->info->name, rc);
			dev_err(chip->dev, "get vreg failed\n");
			return rc;
		}
	}

	rc = regulator_set_voltage(chip->vin1, CAM_LDO_VIN1_UV,
				   CAM_LDO_VIN1_UV);
	if (rc) {
		dev_err(chip->dev, "Unable to set voltage for vin1:%d\n", rc);
		return rc;
	}
	rc = regulator_enable(chip->vin1);
	if (rc) {
		dev_err(chip->dev, "Unable to enable vin1:%d\n", rc);
		return rc;
	}
	rc = devm_add_action_or_reset(chip->dev, cam_ldo_regulator_disable,
				      chip->vin1);
	if (rc)
		return rc;

	if (vin2) {
		rc = regulator_set_voltage(chip->vin2, CAM_LDO_VIN2_UV,
					   CAM_LDO_VIN2_UV);
		if (rc) {
			dev_err(chip->dev,
				"Unable to set voltage for vin2:%d\n", rc);
			return rc;
		}
		rc = regulator_enable(chip->vin2);
		if (rc) {
			dev_err(chip->dev, "Unable to enable vin2:%d\n", rc);
			return rc;
		}
		rc = devm_add_action_or_reset(chip->dev,
					      cam_ldo_regulator_disable,
					      chip->vin2);
		if (rc)
			return rc;
	}

	return 0;
}

/* Match the chip id, then write the default voltages */
int cam_ldo_init(struct cam_ldo_chip *chip)
{
	const struct cam_ldo_info *info = chip->info;
	u8 id = 0;
	int rc;

	rc = cam_ldo_read(chip, info->id_reg, &id);
	pr_info("%s_get_id:%s id is 0x%x\n", info->name, info->name, id);
	if (rc < 0 || (id != info->id[0] && id != info->id[1])) {
		pr_err("%s read id failed\n", info->name);
		return -1;
	}

	rc = cam_ldo_write_table(chip);
	if (rc < 0)
		pr_err("%s init failed\n", info->name);

	if (info->settle) {
		msleep(10);
		cam_ldo_print_reg(chip);
	}

	return 0;
}

static ssize_t cam_ldo_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_ldo_attr *a = container_of(attr, struct cam_ldo_attr, attr);
	struct cam_ldo_chip *chip = dev_get_drvdata(dev);
	u8 val = 0;

	cam_ldo_read(chip, a->reg, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t cam_ldo_attr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_ldo_attr *a = container_of(attr, struct cam_ldo_attr, attr);
	struct cam_ldo_chip *chip = dev_get_drvdata(dev);
	u8 val;
	int rc;

	rc = kstrtou8(buf, 0, &val);
	if (rc)
		return rc;

	rc = cam_ldo_write(chip, a->reg, val);
	if (rc < 0) {
		if (a->err)
			pr_err("%s%s\n", chip->info->name, a->err);
		else
			pr_err("%s set enable failed\n", chip->info->name);
		return rc;
	}

	if (!a->err)
		pr_info("%s set enable ret = %d\n", chip->info->name, rc);

	return count;
}

int cam_ldo_sysfs_create(struct cam_ldo_chip *chip)
{
	static const char * const names[CAM_LDO_NUM_ATTRS] = {
		"out_ldo1", "out_ldo1p1", "out_ldo2", "out_ldo3", "out_ldo4",
		"out_ldo5", "out_ldo6", "out_ldo7", "vol_enable",
	};
	/* sequence port behind each attribute, -1 for the enable register */
	static const int ports[CAM_LDO_NUM_ATTRS] = {
		0, 7, 1, 2, 3, 4, 5, 6, -1,
	};
	static const char * const errs[CAM_LDO_NUM_ATTRS] = {
		" open ldo1 failed", " open ldo1 failed", " open ldo2 failed",
		" open ldo3 failed", " open ldo4 failed", " open ldo5 failed",
		" open ldo6 failed", " open ldo7 failed", NULL,
	};
	int i;

	for (i = 0; i < CAM_LDO_NUM_ATTRS; i++) {
		struct cam_ldo_attr *a = &chip->attrs[i];

		a->reg = ports[i] < 0 ? chip->info->en_reg :
					chip->info->port[ports[i]].vreg;
		a->err = errs[i];
		sysfs_attr_init(&a->attr.attr);
		a->attr.attr.name = names[i];
		a->attr.attr.mode = 0664;
		a->attr.show = cam_ldo_attr_show;
		a->attr.store = cam_ldo_attr_store;
		chip->attr_list[i] = &a->attr.attr;
	}
	chip->attr_list[CAM_LDO_NUM_ATTRS] = NULL;
	chip->attr_group.attrs = chip->attr_list;

	/* removed together with the device binding */
	return devm_device_add_group(chip->dev, &chip->attr_group);
}

/*
 * The sensor power sequence names an LDO port only; try the PMICs one after
 * the other, the one that probed handles it.
 */
int cam_ldo_camera_power_up(int port)
{
	int rc;

	rc = wl2866d_camera_power_up(port);
	if (rc >= 0)
		return rc;
	rc = et5907_camera_power_up(port);
	if (rc >= 0)
		return rc;

	return lapis_wl2868c_camera_power_up(port);
}

int cam_ldo_camera_power_down(int port)
{
	int rc;

	rc = wl2866d_camera_power_down(port);
	if (rc >= 0)
		return rc;
	rc = et5907_camera_power_down(port);
	if (rc >= 0)
		return rc;

	return lapis_wl2868c_camera_power_down(port);
}
