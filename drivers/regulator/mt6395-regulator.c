/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/pm.h>
#include <mt-plat/mtk_chip.h>

#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
#include "mt6395-regulator.h"
#endif

/*
 * For development
 * Remove version after driver is verified and ready to upstream
 */
#define MT6395_DRV_VERSION	"1.0.4"

#define MT6395_PRODUCT_ID				0x00
#define MT6395_MANUFACTURE_ID				0x01
#define MT6395_REVISION_NUMBER				0x02
#define MT6395_PG_INFO					0x03
#define MT6395_UV_INFO					0x04
#define MT6395_OV_INFO					0x05
#define MT6395_CH1_VID					0x06
#define MT6395_CH1_SLEEP_VID				0x07
#define MT6395_INTERNAL_ENABLE				0x08
#define MT6395_BUCK_MODE_CTRL				0x09
#define MT6395_PROTECTION_MODE				0x0A
#define MT6395_WDOG_RST_CTRL				0x0B
#define MT6395_SLEEP_MODE_CTRL				0x0C
#define MT6395_SEQUENCE_CONFIG				0x0D
#define MT6395_CH1_TIME_CONFIG				0x0E
#define MT6395_CH2_TIME_CONFIG				0x0F
#define MT6395_CH3_TIME_CONFIG				0x10
#define MT6395_CH4_TIME_CONFIG				0x11
#define MT6395_CH5_TIME_CONFIG				0x12
#define MT6395_EXEN1_TIME_CONFIG			0x13
#define MT6395_RST_DLY_TIME				0x1C
#define MT6395_INT_ENABLE				0x1D
#define MT6395_INT_STATUS				0x1E
#define MT6395_DISCHG_CTRL				0x1F
#define MT6395_CH2_VOLT_OFFSET				0x20
#define MT6395_SLEW_RATE_CTRL				0x45

#define MT6395_CH1_ENABLE_MASK				BIT(1)
#define MT6395_CH2_ENABLE_MASK				BIT(2)
#define MT6395_CH3_ENABLE_MASK				BIT(3)
#define MT6395_CH4_ENABLE_MASK				BIT(4)
#define MT6395_CH5_ENABLE_MASK				BIT(5)

#define MT6395_CH1_MODE_MASK				BIT(1)
#define MT6395_CH2_MODE_MASK				BIT(2)
#define MT6395_CH3_MODE_MASK				BIT(3)
#define MT6395_CH4_MODE_MASK				BIT(4)

#define MT6395_SLEEP_MODE_MASK				BIT(0)
#define MT6395_SLEEP_CTRL_MASK				BIT(7)

#define MT6395_SWOFF_SEQ_MASK				BIT(4)

#define MT6395_INT_STATUS_MASK				0x61

#define MT6395_CH1_SLEW_RATE_MASK			0xC0
#define MT6395_CH1_SLEW_RATE_SHFT			6

enum mt6395_regulator {
	MT6395_RG_BUCK1 = 0,
	MT6395_RG_BUCK2,
	MT6395_RG_BUCK3,
	MT6395_RG_BUCK4,
	MT6395_RG_BUCK_MAX,
	MT6395_RG_LDO1 = MT6395_RG_BUCK_MAX,
	MT6395_RG_MAX,
};

#define mt6395_buck1_id			MT6395_RG_BUCK1
#define mt6395_buck1_min_uV		600000
#define mt6395_buck1_uV_step		6250
#define mt6395_buck1_n_voltages		127
#define mt6395_buck1_vsel_reg		MT6395_CH1_VID
#define mt6395_buck1_vsel_mask		0x7F
#define mt6395_buck1_enable_reg		MT6395_INTERNAL_ENABLE
#define mt6395_buck1_enable_mask	MT6395_CH1_ENABLE_MASK
#define mt6395_buck1_linear_min_sel	0

#define mt6395_buck2_id			MT6395_RG_BUCK2
#define mt6395_buck2_fixed_uV		1100000
#define mt6395_buck2_enable_reg		MT6395_INTERNAL_ENABLE
#define mt6395_buck2_enable_mask	MT6395_CH2_ENABLE_MASK

#define mt6395_buck3_id			MT6395_RG_BUCK3
#define mt6395_buck3_fixed_uV		1800000
#define mt6395_buck3_enable_reg		MT6395_INTERNAL_ENABLE
#define mt6395_buck3_enable_mask	MT6395_CH3_ENABLE_MASK

#define mt6395_buck4_id			MT6395_RG_BUCK4
#define mt6395_buck4_fixed_uV		3300000
#define mt6395_buck4_enable_reg		MT6395_INTERNAL_ENABLE
#define mt6395_buck4_enable_mask	MT6395_CH4_ENABLE_MASK

#define mt6395_ldo1_id			MT6395_RG_LDO1
#define mt6395_ldo1_fixed_uV		1200000
#define mt6395_ldo1_enable_reg		MT6395_INTERNAL_ENABLE
#define mt6395_ldo1_enable_mask		MT6395_CH5_ENABLE_MASK

struct mt6395_chip;

struct mt6395_regulator_drvdata {
	struct regulator_desc rdesc;
};

#ifdef CONFIG_DEBUG_FS
enum mt6395_dbg_node {
	MT6395_DBG_REGS,
	MT6395_DBG_REGADDR,
	MT6395_DBG_DATA,
	MT6395_DBG_MAX,
};

static const char * const mt6395_dentry_name[] = {
	"regs", "reg_addr", "data",
};

struct mt6395_dbg_info {
	struct mt6395_chip *chip;
	enum mt6395_dbg_node node;
};

static u8 mt6395_regaddr[] = {
	MT6395_PRODUCT_ID,
	MT6395_MANUFACTURE_ID,
	MT6395_REVISION_NUMBER,
	MT6395_PG_INFO,
	MT6395_UV_INFO,
	MT6395_OV_INFO,
	MT6395_CH1_VID,
	MT6395_CH1_SLEEP_VID,
	MT6395_INTERNAL_ENABLE,
	MT6395_BUCK_MODE_CTRL,
	MT6395_PROTECTION_MODE,
	MT6395_WDOG_RST_CTRL,
	MT6395_SLEEP_MODE_CTRL,
	MT6395_SEQUENCE_CONFIG,
	MT6395_CH1_TIME_CONFIG,
	MT6395_CH2_TIME_CONFIG,
	MT6395_CH3_TIME_CONFIG,
	MT6395_CH4_TIME_CONFIG,
	MT6395_CH5_TIME_CONFIG,
	MT6395_EXEN1_TIME_CONFIG,
	MT6395_RST_DLY_TIME,
	MT6395_INT_ENABLE,
	MT6395_INT_STATUS,
	MT6395_DISCHG_CTRL,
	MT6395_CH2_VOLT_OFFSET,
	MT6395_SLEW_RATE_CTRL,
};
#endif /* CONFIG_DEBUG_FS */

struct mt6395_chip {
	struct device *dev;
	struct regmap *regmap;
	struct i2c_client *client;
	struct gpio_desc *irq_gpio;
	int irq;
	struct mutex io_lock;

#ifdef CONFIG_DEBUG_FS
	struct mt6395_dbg_info dbg_info[MT6395_DBG_MAX];
	struct dentry *dbg_dir;
	struct dentry *dbg_entry[MT6395_DBG_MAX];
	u8 dbg_regaddr;
#endif /* CONFIG_DEBUG_FS */
};

#ifdef CONFIG_DEBUG_FS
static int mt6395_regs_show(struct mt6395_chip *chip, struct seq_file *s)
{
	int ret, i;
	u32 data;

	for (i = 0; i < ARRAY_SIZE(mt6395_regaddr); i++) {
		ret = regmap_read(chip->regmap, mt6395_regaddr[i], &data);
		if (ret < 0)
			continue;

		seq_printf(s, "reg0x%02X=0x%02X\n", mt6395_regaddr[i], data);
	}
	return 0;
}

static int mt6395_regaddr_show(struct mt6395_chip *chip, struct seq_file *s)
{
	seq_printf(s, "0x%02X\n", chip->dbg_regaddr);
	return 0;
}

static int mt6395_data_show(struct mt6395_chip *chip, struct seq_file *s)
{
	int ret;
	u32 data;

	ret = regmap_read(chip->regmap, chip->dbg_regaddr, &data);
	if (ret < 0)
		return ret;

	seq_printf(s, "reg0x%02X=0x%02X\n", chip->dbg_regaddr, data);
	return 0;
}

static int mt6395_dbg_show(struct seq_file *s, void *v)
{
	int ret;
	struct mt6395_dbg_info *dinfo = s->private;
	struct mt6395_chip *chip = dinfo->chip;

	switch (dinfo->node) {
	case MT6395_DBG_REGS:
		ret = mt6395_regs_show(chip, s);
		break;
	case MT6395_DBG_REGADDR:
		ret = mt6395_regaddr_show(chip, s);
		break;
	case MT6395_DBG_DATA:
		ret = mt6395_data_show(chip, s);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mt6395_dbg_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, mt6395_dbg_show, inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}

static int mt6395_dbg_extract_param(const char *buf, u8 *param)
{
	char token[5];
	long int value;

	if (buf[0] != '0' || (buf[1] != 'x' && buf[1] != 'X'))
		return -EINVAL;
	memcpy(token, buf, 5);
	if (kstrtoul(token, 16, &value) != 0)
		return -EINVAL;
	*param = value;
	return 0;
}

static bool mt6395_is_regaddr_valid(u8 regaddr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt6395_regaddr); i++) {
		if (mt6395_regaddr[i] == regaddr)
			return true;
	}
	return false;
}

static ssize_t mt6395_dbg_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	int ret;
	struct mt6395_dbg_info *dinfo = file->private_data;
	struct mt6395_chip *chip = dinfo->chip;
	char lbuf[5];
	u8 param;

	if (count != 5)
		return -EFAULT;

	ret = copy_from_user(lbuf, ubuf, count);
	if (ret)
		return -EFAULT;
	lbuf[count - 1] = '\0';

	switch (dinfo->node) {
	case MT6395_DBG_REGADDR:
		ret = mt6395_dbg_extract_param(lbuf, &param);
		if (ret < 0) {
			dev_err(chip->dev, "%s get regaddr fail\n", __func__);
			goto out;
		}
		if (!mt6395_is_regaddr_valid(param))
			return -EINVAL;
		chip->dbg_regaddr = param;
		break;
	case MT6395_DBG_DATA:
		ret = mt6395_dbg_extract_param(lbuf, &param);
		if (ret < 0) {
			dev_err(chip->dev, "%s get data fail\n", __func__);
			goto out;
		}
		ret = regmap_write(chip->regmap, chip->dbg_regaddr, param);
		if (ret < 0)
			dev_err(chip->dev, "%s write data fail\n", __func__);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	return ret < 0 ? ret : count;
}

static int mt6395_dbg_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations mt6395_dbg_ops = {
	.open = mt6395_dbg_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = mt6395_dbg_write,
	.release = mt6395_dbg_release,
};

static int mt6395_init_debugfs(struct mt6395_chip *chip)
{
	int i, len;
	char *dirname;
	struct mt6395_dbg_info *dinfo;

	len = strlen(dev_name(chip->dev));
	dirname = devm_kzalloc(chip->dev, len + 8, GFP_KERNEL);
	if (!dirname)
		return -ENOMEM;
	snprintf(dirname, len + 8, "mt6395-%s", dev_name(chip->dev));
	chip->dbg_dir = debugfs_create_dir(dirname, NULL);
	if (!chip->dbg_dir)
		return -ENOMEM;

	for (i = 0; i < MT6395_DBG_MAX; i++) {
		dinfo = &chip->dbg_info[i];
		dinfo->chip = chip;
		dinfo->node = i;
		chip->dbg_entry[i] = debugfs_create_file(mt6395_dentry_name[i],
							 S_IFREG | 0444,
							 chip->dbg_dir, dinfo,
							 &mt6395_dbg_ops);
	}
	return 0;
}

static void mt6395_uninit_debugfs(struct mt6395_chip *chip)
{
	debugfs_remove_recursive(chip->dbg_dir);
}
#else
static int mt6395_init_debugfs(struct mt6395_chip *chip)
{
	return 0;
}

static int mt6395_uninit_debugfs(struct mt6395_chip *chip)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static const u8 mt6395_buck_mode_mask[MT6395_RG_BUCK_MAX] = {
	MT6395_CH1_MODE_MASK, MT6395_CH2_MODE_MASK, MT6395_CH3_MODE_MASK,
	MT6395_CH4_MODE_MASK,
};

static bool mt6395_volatile_reg(struct device *dev, unsigned int reg)
{
	if (reg == MT6395_INT_STATUS)
		return true;
	return false;
}

static const struct regmap_config mt6395_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x45,
	.volatile_reg = mt6395_volatile_reg,
};

static inline bool mt6395_is_buck(int rid)
{
	if (rid >= MT6395_RG_BUCK1 && rid <= MT6395_RG_BUCK4)
		return true;
	return false;
}

static int mt6395_regulator_get_status(struct regulator_dev *rdev)
{
	int ret;
	u32 regval;
	struct mt6395_regulator_drvdata *drvdata = rdev_get_drvdata(rdev);
	struct regulator_desc *rdesc = &drvdata->rdesc;

	ret = regmap_read(rdev->regmap, rdesc->enable_reg, &regval);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s get enable status fail(%d)\n",
			__func__, ret);
		return ret;
	}

	return (regval & rdesc->enable_mask) ? REGULATOR_STATUS_ON :
					       REGULATOR_STATUS_OFF;
}

static inline int mt6395_set_pwm_mode(struct regulator_dev *rdev, int rid,
				      bool pwm)
{
	u8 mask = mt6395_buck_mode_mask[rid];
	u8 val = pwm ? mask : 0;

	return regmap_update_bits(rdev->regmap, MT6395_BUCK_MODE_CTRL, mask,
				  val);
}

static inline int mt6395_set_standby_mode(struct regulator_dev *rdev,
					  bool standby)
{
	u8 val = standby ? MT6395_SLEEP_MODE_MASK : 0;

	return regmap_update_bits(rdev->regmap, MT6395_SLEEP_MODE_CTRL,
				  MT6395_SLEEP_MODE_MASK, val);
}

static int mt6395_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	int ret;
	struct mt6395_regulator_drvdata *drvdata = rdev_get_drvdata(rdev);
	struct regulator_desc *rdesc = &drvdata->rdesc;
	int rid = rdesc->id;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		if (!mt6395_is_buck(rid))
			return -EINVAL;
		ret = mt6395_set_pwm_mode(rdev, rid, true);
		break;
	case REGULATOR_MODE_NORMAL:
		if (mt6395_is_buck(rid)) {
			ret = mt6395_set_pwm_mode(rdev, rid, false);
			if (ret < 0)
				return ret;
		}
		ret = mt6395_set_standby_mode(rdev, false);
		break;
	case REGULATOR_MODE_STANDBY:
		ret = mt6395_set_standby_mode(rdev, true);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Even though the return type is unsigned int,
 * regulator framework returns negative value when it fails
 */
static unsigned int mt6395_regulator_get_mode(struct regulator_dev *rdev)
{
	int ret;
	struct mt6395_regulator_drvdata *drvdata = rdev_get_drvdata(rdev);
	struct regulator_desc *rdesc = &drvdata->rdesc;
	int rid = rdesc->id;
	u32 regval;

	ret = regmap_read(rdev->regmap, MT6395_SLEEP_MODE_CTRL, &regval);
	if (ret < 0)
		return ret;

	if (regval & MT6395_SLEEP_MODE_MASK)
		return REGULATOR_MODE_STANDBY;
	if (!mt6395_is_buck(rid))
		return REGULATOR_MODE_NORMAL;
	ret = regmap_read(rdev->regmap, MT6395_BUCK_MODE_CTRL, &regval);
	if (ret < 0)
		return ret;
	if (regval & mt6395_buck_mode_mask[rid])
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static const u8 mt6395_ramp_delay_regval[] = {
	0x03, 0x02, 0x01, 0x00
};

static int mt6395_regulator_set_ramp_delay(struct regulator_dev *rdev,
					   int ramp_delay)
{
	u8 regval;

	if (ramp_delay < 0)
		return -EINVAL;
	if (ramp_delay < 5000)
		ramp_delay = 5000;
	else if (ramp_delay > 20000)
		ramp_delay = 20000;

	/*
	 * (ramp delay + (step - 1000) - step) / step
	 * (ramp delay - 1000) / step
	 */
	regval = (ramp_delay - 1000) / 5000;

	/* map ramp delay to register value */
	regval = mt6395_ramp_delay_regval[regval];

	return regmap_update_bits(rdev->regmap, MT6395_SLEW_RATE_CTRL,
				  MT6395_CH1_SLEW_RATE_MASK,
				  regval << MT6395_CH1_SLEW_RATE_SHFT);
}

/* Modifiable regulator */
static const struct regulator_ops mt6395_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_mode = mt6395_regulator_set_mode,
	.get_mode = mt6395_regulator_get_mode,
	.get_status = mt6395_regulator_get_status,
	.set_ramp_delay = mt6395_regulator_set_ramp_delay,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
};

/* Fixed regulator */
static const struct regulator_ops mt6395_fixed_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6395_regulator_set_mode,
	.get_mode = mt6395_regulator_get_mode,
	.get_status = mt6395_regulator_get_status,
};

#define MT6395_REGULATOR_LINEAR(_name, _match) \
{ \
	.rdesc = { \
		.name = #_name, \
		.of_match = #_match, \
		.ops = &mt6395_regulator_ops, \
		.type = REGULATOR_VOLTAGE, \
		.id = _name##_id, \
		.owner = THIS_MODULE, \
		.n_voltages = _name##_n_voltages, \
		.linear_min_sel = _name##_linear_min_sel, \
		.min_uV = _name##_min_uV, \
		.uV_step = _name##_uV_step, \
		.vsel_reg = _name##_vsel_reg, \
		.vsel_mask = _name##_vsel_mask, \
		.enable_reg = _name##_enable_reg, \
		.enable_mask = _name##_enable_mask, \
		.enable_val = _name##_enable_mask, \
		.disable_val = 0, \
	}, \
}

#define MT6395_REGULATOR_FIXED(_name, _match) \
{ \
	.rdesc = { \
		.name = #_name, \
		.of_match = #_match, \
		.ops = &mt6395_fixed_regulator_ops, \
		.type = REGULATOR_VOLTAGE, \
		.id = _name##_id, \
		.owner = THIS_MODULE, \
		.n_voltages = 1, \
		.fixed_uV = _name##_fixed_uV, \
		.enable_reg = _name##_enable_reg, \
		.enable_mask = _name##_enable_mask, \
		.enable_val = _name##_enable_mask, \
		.disable_val = 0, \
	}, \
}

static struct mt6395_regulator_drvdata
mt6395_regulator_drvdata_tbl[MT6395_RG_MAX] = {
	MT6395_REGULATOR_LINEAR(mt6395_buck1, mt6395_buck1),
	MT6395_REGULATOR_FIXED(mt6395_buck2, mt6395_buck2),
	MT6395_REGULATOR_FIXED(mt6395_buck3, mt6395_buck3),
	MT6395_REGULATOR_FIXED(mt6395_buck4, mt6395_buck4),
	MT6395_REGULATOR_FIXED(mt6395_ldo1, mt6395_ldo1),
};

static int mt6395_register_regulator(struct mt6395_chip *chip)
{
	int ret, i;
	struct device_node *np = chip->dev->of_node;
	struct device_node *chd_np;
	struct regulator_desc *rdesc;
	struct regulator_config rcfg = {};
	struct regulator_init_data *init_data;
	struct regulator_dev *rdev;
	struct mt6395_regulator_drvdata *drvdata;
	struct regulation_constraints *constraints;

	for (i = 0; i < MT6395_RG_MAX; i++) {
		drvdata = &mt6395_regulator_drvdata_tbl[i];
		rdesc = &drvdata->rdesc;
		chd_np = of_get_child_by_name(np, rdesc->name);
		if (!chd_np)
			continue;
		init_data = of_get_regulator_init_data(chip->dev, chd_np,
						       rdesc);
		if (!init_data)
			continue;
		rcfg.init_data = init_data;
		rcfg.dev = chip->dev;
		rcfg.regmap = chip->regmap;
		rcfg.driver_data = drvdata;
		rcfg.of_node = chd_np;

		rdev = devm_regulator_register(chip->dev, rdesc, &rcfg);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(chip->dev, "%s register %s fail(%d)\n",
				__func__, rdesc->name, ret);
			return ret;
		}

		constraints = rdev->constraints;
		constraints->valid_modes_mask |= REGULATOR_MODE_NORMAL |
						 REGULATOR_MODE_STANDBY |
						 REGULATOR_MODE_FAST;
		constraints->valid_ops_mask |= REGULATOR_CHANGE_MODE;
	}
	return 0;
}

static int mt6395_hotdie_irq_handler(struct mt6395_chip *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
	return 0;
}

static int mt6395_pwrkey_release_irq_handler(struct mt6395_chip *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
	kpd_pwrkey_pmic_handler(0);
#endif
	return 0;
}

static int mt6395_pwrkey_press_irq_handler(struct mt6395_chip *chip)
{
	dev_err(chip->dev, "%s\n", __func__);
#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
	kpd_pwrkey_pmic_handler(1);
#endif
	return 0;
}

struct irq_mapping_tbl {
	const char *name;
	int (*hdlr)(struct mt6395_chip *chip);
	int num;
};

#define MT6395_IRQ_MAPPING(_name, _num) \
	{.name = #_name, .hdlr = mt6395_##_name##_irq_handler, .num = _num}

static const struct irq_mapping_tbl mt6395_irq_mapping_tbl[] = {
	MT6395_IRQ_MAPPING(hotdie, 0),
	MT6395_IRQ_MAPPING(pwrkey_press, 6),
	MT6395_IRQ_MAPPING(pwrkey_release, 5),
};

static int __mt6395_irq_handler(struct mt6395_chip *chip)
{
	int i, ret;
	u32 event, mask;

	ret = regmap_read(chip->regmap, MT6395_INT_STATUS, &event);
	if (ret < 0)
		goto err;
	ret = regmap_read(chip->regmap, MT6395_INT_ENABLE, &mask);
	if (ret < 0)
		goto err;
	ret = regmap_write_bits(chip->regmap, MT6395_INT_STATUS,
				MT6395_INT_STATUS_MASK, event);
	if (ret < 0)
		goto err;
	event &= mask;
	for (i = 0; i < ARRAY_SIZE(mt6395_irq_mapping_tbl); i++) {
		if (event & BIT(mt6395_irq_mapping_tbl[i].num))
			mt6395_irq_mapping_tbl[i].hdlr(chip);
	}
err:
	return ret;
}

static irqreturn_t mt6395_irq_handler(int irq, void *data)
{
	int ret;
	struct mt6395_chip *chip = (struct mt6395_chip *)data;

	dev_info(chip->dev, "%s\n", __func__);
	do {
		ret = __mt6395_irq_handler(chip);
		if (ret < 0)
			dev_err(chip->dev, "%s handle irq fail(%d)\n",
				__func__, ret);
		ret = gpiod_get_value(chip->irq_gpio);
		if (ret < 0)
			dev_err(chip->dev, "%s get irq gpio status fail(%d)\n",
				__func__, ret);
	} while (ret == 0);

	return IRQ_HANDLED;
}

static int mt6395_register_irq(struct mt6395_chip *chip)
{
	int ret, len;
	char *name;

	/* Mask & clear all events */
	regmap_update_bits(chip->regmap, MT6395_INT_ENABLE,
			   MT6395_INT_STATUS_MASK, 0x00);
	regmap_write_bits(chip->regmap, MT6395_INT_STATUS,
			  MT6395_INT_STATUS_MASK, MT6395_INT_STATUS_MASK);

	chip->irq_gpio = devm_gpiod_get(chip->dev, "mt6395,irq", GPIOD_IN);
	if (IS_ERR(chip->irq_gpio))
		return PTR_ERR(chip->irq_gpio);
	chip->irq = gpiod_to_irq(chip->irq_gpio);

	len = strlen(dev_name(chip->dev));
	name = devm_kzalloc(chip->dev, len + 5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	snprintf(name, len + 5, "%s_irq", dev_name(chip->dev));
	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					mt6395_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					name, chip);
	if (ret < 0) {
		dev_err(chip->dev, "%s request thread irq fail\n", __func__);
		return ret;
	}
	device_init_wakeup(chip->dev, true);
	return 0;
}

static int mt6395_init_irq(struct mt6395_chip *chip)
{
	u8 mask = MT6395_INT_STATUS_MASK;

	return regmap_update_bits(chip->regmap, MT6395_INT_ENABLE,
				  MT6395_INT_STATUS_MASK, mask);
}

static struct regmap *mt6395_regmap;

void mt6395_power_off(void)
{
	pr_notice("[%s]power off system\n", __func__);

	if (mt6395_regmap)
		regmap_update_bits(mt6395_regmap, MT6395_SEQUENCE_CONFIG,
			   MT6395_SWOFF_SEQ_MASK, MT6395_SWOFF_SEQ_MASK);
	while(1) {
		mdelay(1000);
		pr_notice("[%s] power off\n", __func__);
	}
}

static int mt6395_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
#if defined(CONFIG_MACH_MT8518)
	int val;
#endif
	struct mt6395_chip *chip;

	dev_info(&client->dev, "%s %s\n", __func__, MT6395_DRV_VERSION);

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->regmap = devm_regmap_init_i2c(client, &mt6395_regmap_config);
	if (IS_ERR(chip->regmap))
		return PTR_ERR(chip->regmap);

	mutex_init(&chip->io_lock);
	chip->client = client;
	chip->dev = &client->dev;
	i2c_set_clientdata(client, chip);

#if defined(CONFIG_MACH_MT8518)
	/*E1 IC should change vsel register if suspend mode controlled by HW*/
	ret = regmap_read(chip->regmap, MT6395_SLEEP_MODE_CTRL, &val);
	if (ret < 0) {
		dev_info(chip->dev, "%s read buck ctrl fail\n", __func__);
		goto err;
	}
	if ((mt_get_chip_hw_ver() == 0xCA00) && (val & MT6395_SLEEP_CTRL_MASK))
		mt6395_regulator_drvdata_tbl[0].rdesc.vsel_reg =
							MT6395_CH1_SLEEP_VID;
#endif

	ret = mt6395_register_regulator(chip);
	if (ret < 0)
		goto err;

	ret = mt6395_register_irq(chip);
	if (ret < 0)
		goto err;

	ret = mt6395_init_debugfs(chip);
	if (ret < 0)
		dev_err(chip->dev, "%s init debugfs fail\n", __func__);

	ret = mt6395_init_irq(chip);
	if (ret < 0)
		goto err;

	mt6395_regmap = chip->regmap;
	if (!pm_power_off)
		pm_power_off = mt6395_power_off;
	else
		dev_warn(chip->dev, "pm_power_off callback already assingned\n");

	dev_info(chip->dev, "%s successfully\n", __func__);
	return 0;
err:
	return ret;
}

static int mt6395_remove(struct i2c_client *client)
{
	struct mt6395_chip *chip = i2c_get_clientdata(client);

	if (chip)
		mt6395_uninit_debugfs(chip);
	return 0;
}

static int mt6395_suspend(struct device *dev)
{
	struct mt6395_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);

	return 0;
}

static int mt6395_resume(struct device *dev)
{
	struct mt6395_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6395_pm_ops, mt6395_suspend, mt6395_resume);

static const struct of_device_id mt6395_match_table[] = {
	{ .compatible = "mediatek,mt6395-regulator", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6395_match_table);

static const struct i2c_device_id mt6395_dev_id[] = {
	{"mt6395-regulator", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6395_dev_id);

static struct i2c_driver mt6395_i2c_driver = {
	.driver = {
		.name = "mt6395-regulator",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6395_match_table),
		.pm = &mt6395_pm_ops,
	},
	.probe = mt6395_i2c_probe,
	.remove = mt6395_remove,
	.id_table = mt6395_dev_id,
};
module_i2c_driver(mt6395_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("Regulator driver for MT6395");
