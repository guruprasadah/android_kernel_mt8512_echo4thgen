/*
 *  drivers/power/rt9428_battery.c
 *  Driver of Richtek RT9428 Fuelgauge IC
 *
 *  Copyright (C) 2014 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *  modified by Jeff Chang <jeff_chang@richtek.com>
 *  modified by ShuFan Lee <shufan_lee@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/delay.h>

/* Interrupt is from linux kernel or not */
/* #define CONFIG_RT9428_KERNEL_INTR */

#if !defined(CONFIG_RT9428_KERNEL_INTR) && defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
#include <adsp_ipi.h>
#include <linux/power/mtk_power_ipi_msg.h>
#endif /* !CONFIG_RT9428_KERNEL_INTR && CONFIG_MTK_HIFI4DSP_SUPPORT */

#include "rt9428.h"

struct rt9428_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct rt9428_platform_data *pdata;
	struct power_supply *fg_psy;
	struct power_supply_desc fg_psy_desc;
	struct delayed_work dwork;
	struct mutex io_lock;
	struct mutex var_lock;
	int alert_irq;
	int last_capacity;	/* unit 0.1% */
	int capacity;		/* unit 0.1% */
	int last_vcell;
	int vcell;
	int soc_offset;
	int btemp;
	unsigned char suspend:1;
	unsigned char online:1;
	unsigned char reg_addr;
	unsigned int reg_data;
};

static unsigned char rt9428_init_regval;
static char *rtdef_fg_name = "rt-fuelgauge";
static char *rt_fg_supply_list[] = {
	"none",
};

static enum power_supply_property rt_fg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY,
};

static inline int rt9428_read_device(struct i2c_client *i2c,
				     int reg, int bytes, void *dest)
{
	int ret;

	if (bytes > 1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

/* default read one byte */
static int rt9428_reg_read(struct i2c_client *i2c, int reg)
{
	struct rt9428_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int rt9428_reg_read_word(struct i2c_client *i2c, int reg)
{
	struct rt9428_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&chip->io_lock);
	if (ret < 0)
		dev_err(chip->dev, "read reg 0x%x io fail\n", reg);
	return (ret < 0) ? ret : swab16(ret);
}

static int rt9428_reg_write_word(struct i2c_client *i2c, int reg,
				 unsigned int data)
{
	struct rt9428_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	RT_DBG("I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
	       (unsigned int)i2c, (unsigned int)reg, (unsigned int)data);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, swab16(data));
	mutex_unlock(&chip->io_lock);
	return ret;
}

static void rt9428_update_info(struct rt9428_chip *chip);

static void rt9428_execute_qs_command(struct rt9428_chip *chip)
{
	/* set QS */
	rt9428_reg_write_word(chip->i2c, RT9428_REG_CTRLH, 0x4000);
	rt9428_update_info(chip);
}

static ssize_t rt_fg_show_attrs(struct device *, struct device_attribute *,
				char *);
static ssize_t rt_fg_store_attrs(struct device *, struct device_attribute *,
				 const char *, size_t count);

#define RT_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},		\
	.show = rt_fg_show_attrs,			\
	.store = rt_fg_store_attrs,			\
}

static struct device_attribute rt_fuelgauge_attrs[] = {
	RT_FG_ATTR(reg),
	RT_FG_ATTR(data),
	RT_FG_ATTR(regs),
	RT_FG_ATTR(qsense),
	RT_FG_ATTR(temp),
};

enum {
	FG_REG = 0,
	FG_DATA,
	FG_REGS,
	FG_QSENSE,
	FG_TEMP,
};

static ssize_t rt_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int i = 0;
	int j = 0;

	switch (offset) {
	case FG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x\n",
			       chip->reg_addr);
		break;
	case FG_DATA:
		chip->reg_data =
		    rt9428_reg_read_word(chip->i2c, chip->reg_addr);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x\n",
			       chip->reg_data);
		dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n", __func__,
			chip->reg_addr, chip->reg_data);
		break;
	case FG_REGS:
		for (j = RT9428_REG_RANGE1_START; j <= RT9428_REG_RANGE1_STOP;
		     j++)
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "reg%02x 0x%02x\n", j,
				       rt9428_reg_read(chip->i2c, j));

		for (j = RT9428_REG_RANGE2_START; j <= RT9428_REG_RANGE2_STOP;
		     j++)
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "reg%02x 0x%02x\n", j,
				       rt9428_reg_read(chip->i2c, j));
		break;
	case FG_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chip->btemp);
		break;
	case FG_QSENSE:
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t rt_fg_store_attrs(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev->parent);
	const ptrdiff_t offset = attr - rt_fuelgauge_attrs;
	int ret = 0;
	int x = 0;
	long int val = 0;

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			if (x < RT9428_REG_MAX &&
			    ((x >= RT9428_REG_RANGE1_START
			      && x <= RT9428_REG_RANGE1_STOP)
			     || (x >= RT9428_REG_RANGE2_START
				 && x <= RT9428_REG_RANGE2_STOP))) {
				chip->reg_addr = x;
				ret = count;
			} else
				ret = -EINVAL;
		} else
			ret = -EINVAL;
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			rt9428_reg_write_word(chip->i2c, chip->reg_addr, x);
			chip->reg_data = x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chip->reg_addr, chip->reg_data);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case FG_QSENSE:
		if (sscanf(buf, "%x\n", &x) == 1 && x == 1) {
			rt9428_execute_qs_command(chip);
			ret = count;
		} else
			ret = -EINVAL;
		break;
	case FG_TEMP:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0) {
			dev_err(dev, "get paremters fail\n");
			ret = -EINVAL;
		}
		chip->btemp = val;
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rt_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(rt_fuelgauge_attrs); i++) {
		rc = device_create_file(dev, &rt_fuelgauge_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &rt_fuelgauge_attrs[i]);
create_attrs_succeed:
	return rc;
}

static int rt9428_fg_get_offset(struct rt9428_chip *chip, int volt, int temp);
static int rt_fg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct rt9428_chip *chip = power_supply_get_drvdata(psy);
	int rc = 0;
	int regval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_VBATH);
		if (regval < 0)
			rc = -EIO;
		else {
			if (!chip->last_vcell)
				chip->last_vcell = chip->vcell =
				    ((regval >> 4) * 5) >> 2;
			val->intval = (regval >> 4) * 1250;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		/* now casually return a constant value */
		val->intval = chip->pdata->battery_type;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_SOCH);

		if (regval < 0)
			rc = -EIO;
		else {
			chip->capacity = (regval >> 8) * 10;
			/* report FG_SOC for debugging. */

			chip->soc_offset = rt9428_fg_get_offset(chip,
								chip->capacity,
								chip->btemp);
			pr_info("%s FG_SOC = %d, TEMP = %d, FG_SOC_OFFS = %d\n",
				__func__, chip->capacity, chip->btemp,
				chip->soc_offset);

			chip->capacity += chip->soc_offset;

			if (chip->capacity > 0)
				chip->capacity = DIV_ROUND_UP(chip->capacity,
							      10);
			else
				chip->capacity = 0;

			if (chip->capacity > 100)
				chip->capacity = 100;
			val->intval = chip->capacity;
			/* report SYS_SOC for debugging. */
			pr_err("%s : SYS_SOC = %d\n", __func__, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->pdata->full_design;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int rt_fg_set_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

/* new function for integrating Normal & Extra Gain ; 2017-02-15 */
static void rt9428_fg_apply_vgcomp(struct rt9428_chip *chip);
static struct vg_comp_data rt9428_fg_get_vgcomp(struct rt9428_chip *chip,
						int volt, int temp);
static int rt9428_fg_get_offset(struct rt9428_chip *chip, int volt, int temp);
static int rt9428_chip_init(struct rt9428_chip *chip);

static void rt9428_update_info(struct rt9428_chip *chip)
{
	int ret, regval;
	struct power_supply *batt_psy;
	union power_supply_propval batt_val;

	/* Check RI */
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_STATUS);
	if (regval & 0x0100) {
		regval &= (~0x0100);
		rt9428_reg_write_word(chip->i2c, RT9428_REG_STATUS, regval);
		rt9428_chip_init(chip);
		if (chip->pdata->irq_mode) {
			regval = rt9428_reg_read_word(chip->i2c,
						      RT9428_REG_CFG0);
			regval |= RT9428_IRQMODE_MASK;
			rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0,
					      regval);
		}
	}

	/* always clr ALRT */
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
	regval &= (~RT9428_SOCALRT_MASK);
	rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0, regval);
	/* get battery voltage store to chip->vcell */
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_VBATH);
	if (regval < 0) {
		dev_err(chip->dev, "read vcell fail\n");
		return;
	}
	chip->vcell = ((regval >> 4) * 5) >> 2;
	/* get battery temp from battery power supply */
	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		ret = power_supply_get_property(batt_psy,
						POWER_SUPPLY_PROP_TEMP,
						&batt_val);
		if (ret >= 0)
			chip->btemp = batt_val.intval * 10;
	}
	dev_info(chip->dev, "btemp=%d\n", chip->btemp);

	/* report FG_VBAT & TEMP for debugging. */
	pr_info("%s : FG_VBAT = %d, TEMP = %d\n",
			__func__, chip->vcell, chip->btemp);

	/* call rt9428_fg_apply_vgcomp instead of following ations */
	rt9428_fg_apply_vgcomp(chip);


#if 0 /* replace with "rt9428_fg_apply_vgcomp" ; 2017-02-15 */
	vg_comp = rt9428_fg_get_vgcomp(chip, chip->vcell, chip->btemp);

	for (i = 0; i < VG_COMP_NR; i++) {
		if (rt9428_reg_write_word(chip->i2c,
			RT9428_REG_MFAH, (0x8100 + (i<<8))|vg_comp.data[i]) < 0)
			dev_err(chip->dev,
				"%s: failed to write VG1\n", __func__);
	}

	pr_info("%s VG Comp = <%d %d %d %d >\n", __func__, vg_comp.data[0],
		vg_comp.data[1], vg_comp.data[2], vg_comp.data[3]);
#endif

	/* read CVS */
	rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8085);
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_MFAH);
	pr_info("%s CVS = 0x%04x\n", __func__, regval);

	/* get battery soc and store to chip->capacity */
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_SOCH);
	regval = (regval >> 8) * 10;
	pr_info("%s soc = %d\n", __func__, regval);

	if (batt_psy) {
		power_supply_changed(batt_psy);
		power_supply_put(batt_psy);
	}
}

#if defined(CONFIG_RT9428_KERNEL_INTR) || defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
static int __rt9428_irq_handler(struct rt9428_chip *chip)
{
	if (chip->suspend) {
		schedule_delayed_work(&chip->dwork, msecs_to_jiffies(20));
		goto out;
	}
	rt9428_update_info(chip);
out:
	return 0;
}

static int __rt9428_intr_init(struct rt9428_chip *chip)
{
	int rc = 0;

	if (chip->pdata->irq_mode) {
		rc = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
		rc |= RT9428_IRQMODE_MASK;
		rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0, rc);
	}
	return 0;
}
#endif /* CONFIG_RT9428_KERNEL_INTR || CONFIG_MTK_HIFI4DSP_SUPPORT */

#ifdef CONFIG_RT9428_KERNEL_INTR
static irqreturn_t rt9428_irq_handler(int irqno, void *param)
{
	struct rt9428_chip *chip = (struct rt9428_chip *)param;

	__rt9428_irq_handler(chip);
	return IRQ_HANDLED;
}
#elif defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
static struct rt9428_chip *g_chip;
static void rt9428_ipi_msg_dispatcher(int _id, void *data, unsigned int len)
{
	struct rt9428_chip *chip = g_chip;
	u8 msg = *((u8 *)data);

	dev_info(chip->dev, "%s msg %d\n", __func__, msg);
	switch (msg) {
	case MTK_POWER_IPIMSG_EINT_INIT:
		__rt9428_intr_init(chip);
		break;
	case MTK_POWER_IPIMSG_EINT_EVT:
		__rt9428_irq_handler(chip);
		break;
	default:
		break;
	}
}
#endif /* CONFIG_RT9460_KERNEL_INTR */

static void rt9428_dwork_func(struct work_struct *work)
{
	struct rt9428_chip *chip =
	    (struct rt9428_chip *)container_of(work, struct rt9428_chip,
					       dwork.work);
	/* update rt9428 information */
	rt9428_update_info(chip);
}

struct submask_condition {
	int x, y;
	int order_x, order_y;
	int xNR, yNR;
	const struct data_point *mesh_src;
};

#define MINVAL(a, b) ((a <= b) ? a : b)

static inline const struct data_point *get_mesh_data(
	int i, int j,
	const struct data_point *mesh, int xNR)
{
	return mesh + j * xNR + i;
}

#define PRECISION_ENHANCE	5
static int offset_li(int xNR, int yNR,
			const struct data_point *mesh, int x, int y)
{
	long long retval = 0;
	int i, j, k;
	long long wM, wD;
	const struct data_point *cache;

	for (i = 0 ; i < xNR; ++i) {
		for (j = 0; j < yNR; ++j) {
			wM = wD = 1;
			cache = get_mesh_data(i, j, mesh, xNR);
			for (k = 0; k < xNR; ++k) {
				if (i != k) {
					wM *= (x - get_mesh_data(k,
							j, mesh, xNR)->x);
					wD *= (cache->x - get_mesh_data(k,
							j, mesh, xNR)->x);
				}
			}
			for (k = 0; k < yNR; ++k) {
				if (j != k) {
					wM *= (y - get_mesh_data(i,
							k, mesh, xNR)->y);
					wD *= (cache->y - get_mesh_data(i,
							k, mesh, xNR)->y);
				}
			}
			retval += div64_s64(((cache->z * wM)
					<< PRECISION_ENHANCE), wD);
		}
	}
	return (int)((retval + (1 << (PRECISION_ENHANCE - 1)))
						>> PRECISION_ENHANCE);
}

static int get_sub_mesh(struct data_point *mesh_buffer,
			struct submask_condition *condition)
{
	int i, j, x, y;

	x = condition->x;
	y = condition->y;
	for (i = 0; i < condition->xNR; ++i) {
		if (get_mesh_data(i,
			0, condition->mesh_src, condition->xNR)->x >= x)
			break;
	}
	for ( ; i >= 0 && i < condition->xNR; --i) {
		if (get_mesh_data(i,
			0, condition->mesh_src, condition->xNR)->x <= x)
			break;
	}

	for (j = 0; j < condition->yNR; ++j) {
		if (get_mesh_data(0,
			j, condition->mesh_src, condition->xNR)->y >= y)
			break;
	}
	for ( ; j >= 0 && j < condition->yNR; --j) {
		if (get_mesh_data(0,
			j, condition->mesh_src, condition->xNR)->y <= y)
			break;
	}
	i -= ((condition->order_x - 1) / 2);
	j -= ((condition->order_y - 1) / 2);

	if (i <= 0)
		i = 0;
	if (j <= 0)
		j = 0;
	if ((i + condition->order_x) > condition->xNR)
		i = condition->xNR - condition->order_x;
	if ((j + condition->order_y) > condition->yNR)
		j = condition->yNR - condition->order_y;
	for (y = 0; y < condition->order_y; ++y) {
		for (x = 0; x < condition->order_x; ++x) {
			*(mesh_buffer + y * condition->order_x + x)
				= *get_mesh_data(i + x, j + y,
					condition->mesh_src, condition->xNR);
		}
	}
	return 0;
}

static struct vg_comp_data vgcomp_li(int xNR, int yNR,
			const struct data_point *mesh, int x, int y)
{
	long long retval[] = { 0, 0, 0, 0};
	struct vg_comp_data ret;
	int i, j, k;
	long long wM, wD;
	const struct data_point *cache;

	for (i = 0 ; i < xNR; ++i) {
		for (j = 0; j < yNR; ++j) {
			wM = wD = 1;
			cache = get_mesh_data(i, j, mesh, xNR);
			for (k = 0; k < xNR; ++k) {
				if (i != k) {
					wM *= (x - get_mesh_data(k,
							j, mesh, xNR)->x);
					wD *= (cache->x - get_mesh_data(k,
							j, mesh, xNR)->x);
				}
			}
			for (k = 0; k < yNR; ++k) {
				if (j != k) {
					wM *= (y - get_mesh_data(i,
							k, mesh, xNR)->y);
					wD *= (cache->y - get_mesh_data(i,
							k, mesh, xNR)->y);
				}
			}
			for (k = 0; k < ARRAY_SIZE(retval); ++k)
				retval[k]  += div64_s64((cache->vg_comp.data[k]
						* wM) << PRECISION_ENHANCE, wD);
		}
	}
	for (k = 0; k < ARRAY_SIZE(retval); ++k) {
		ret.data[k] = (int)((retval[k] +
			(1 << (PRECISION_ENHANCE - 1))) >> PRECISION_ENHANCE);
		if (unlikely(ret.data[k] < 0))
			ret.data[k]  = 0;
		else if (unlikely(ret.data[k] > 255))
			ret.data[k] = 255;
	}
	return ret;
}

/* replaced with soc ; 20181024 */
static struct vg_comp_data rt9428_fg_get_vgcomp(struct rt9428_chip *chip,
						int soc, int temp)
{
	const int ip_x = chip->pdata->vg_comp_interpolation_order[0];
	const int ip_y = chip->pdata->vg_comp_interpolation_order[1];
	struct data_point sub_mesh[ip_x * ip_y];
	int xNR, yNR;
	const struct vg_comp_data default_vgcomp = {
		.data = { 0x32, 0x32, 0x32, 0x32, },
	};
	struct vg_comp_data retval;
	struct vg_comp_table *vgcomp_table = NULL;
	struct submask_condition condition = {
		.x = soc,  /* replaced with soc ; 20181024 */
		.y = temp,
	};

	mutex_lock(&chip->var_lock);
	vgcomp_table = &chip->pdata->vg_comp;
	xNR = vgcomp_table->socnr;  /* replaced with soc ; 20181024 */
	yNR = vgcomp_table->tempnr;
	if (xNR == 0 || yNR == 0) {
		mutex_unlock(&chip->var_lock);
		return default_vgcomp;
	}
	condition.order_x = MINVAL(ip_x, xNR);
	condition.order_y = MINVAL(ip_y, yNR);
	condition.xNR = xNR;
	condition.yNR = yNR;
	condition.mesh_src = vgcomp_table->vg_comp_data;
	get_sub_mesh(sub_mesh, &condition);
	retval = vgcomp_li(condition.order_x, condition.order_y, sub_mesh,
			   soc, temp);  /* replaced with soc ; 20181024 */
	mutex_unlock(&chip->var_lock);
	dev_dbg(chip->dev, "%d %d %d %d\n", retval.data[0],
		retval.data[1], retval.data[2], retval.data[3]);
	return retval;
}

// new function for integrating Normal & Extra Gain ; 2017-02-15
static void rt9428_fg_apply_vgcomp(struct rt9428_chip *chip)
{
	int last_soc = 0, i;
	int now_soc = 0;
	struct vg_comp_data vg_comp;
	int vg_ratio = 0;  // ratio is saved as 10x
	int dSOC = 0;
	int vg_comp_check = 0;

	// get last_soc & now_soc
	last_soc = chip->capacity - chip->soc_offset;
	now_soc = rt9428_reg_read_word(chip->i2c, RT9428_REG_SOCH);
	now_soc = (now_soc >> 8) * 10;
	pr_info("%s last_soc = %d ; now_soc = %d ; soc_offset = %d\n", __func__,
		last_soc, now_soc, chip->soc_offset);

	// get vg_comp by lookup table
	/* replaced with soc ; 20181024 */
	vg_comp = rt9428_fg_get_vgcomp(chip, now_soc, chip->btemp);

	for (i = 0; i < VG_COMP_NR; i++) {
		if (rt9428_reg_write_word(chip->i2c,
			RT9428_REG_MFAH, (0x8100 + (i<<8))|vg_comp.data[i]) < 0)
			dev_err(chip->dev,
				"%s: failed to write VG1\n", __func__);
	}

	pr_info("%s VG Comp = <%d %d %d %d >\n", __func__, vg_comp.data[0],
		vg_comp.data[1], vg_comp.data[2], vg_comp.data[3]);

	/* update(write) vg_comp to rt9428 after get new vg_comp */
	for (i = 0; i < VG_COMP_NR; i++) {
		/* check before writing, the maximum is VG_COMP_MAX */
		if (vg_comp.data[i] > VG_COMP_MAX) {
			if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH,
			    (0x8100 + (i << 8)) | VG_COMP_MAX) < 0)
				dev_err(chip->dev, "%s: failed to write VG%d\n",
					__func__, i + 1);
		} else {
			if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH,
			    (0x8100 + (i << 8)) | vg_comp.data[i]) < 0)
				dev_err(chip->dev, "%s: failed to write VG%d\n",
					__func__, i + 1);
		}
	}

	if ((now_soc - last_soc) > 0) /* check as charging */
		vg_comp_check = vg_comp.data[VG_COMP_CHG_START];
	else
		vg_comp_check = vg_comp.data[VG_COMP_DSG_START];

	/* check the vg_comp > VG_COMP_MAX or not */
	if (vg_comp_check > VG_COMP_MAX) {
		/* get the ratio between vg_comp & VG_COMP_MAX */
		vg_ratio = DIV_ROUND_UP((vg_comp_check * 10), VG_COMP_MAX);

		/* get the difference between last_soc & now_soc */
		dSOC = DIV_ROUND_UP(((now_soc - last_soc) * vg_ratio), 10);

		/* write back to rt9428 if dSOC increase over 1% */
		if ((dSOC > 10) || (dSOC < -10)) {
			if (rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH,
			    0x8600 | DIV_ROUND_UP(last_soc + dSOC, 10)) < 0)
				dev_err(chip->dev,
					"%s: failed to write back SOC\n",
					__func__);
		}
	}

}

static int rt9428_fg_get_offset(struct rt9428_chip *chip, int soc_val, int temp)
{
	const int ip_x = chip->pdata->offset_interpolation_order[0];
	const int ip_y = chip->pdata->offset_interpolation_order[1];
	struct data_point sub_mesh[ip_x * ip_y];
	int xNR, yNR;
	int offset;
	struct soc_offset_table *offset_table = NULL;

	struct submask_condition condition = {
		.x = soc_val,
		.y = temp,
	};

	mutex_lock(&chip->var_lock);
	offset_table = &chip->pdata->soc_offset;
	xNR = offset_table->soc_voltnr;
	yNR = offset_table->tempnr;
	if (xNR == 0 || yNR == 0) {
		mutex_unlock(&chip->var_lock);
		return 0;
	}
	condition.order_x = MINVAL(ip_x, xNR);
	condition.order_y = MINVAL(ip_y, yNR);
	condition.xNR = xNR;
	condition.yNR = yNR;
	condition.mesh_src = offset_table->soc_offset_data;
	get_sub_mesh(sub_mesh, &condition);
	offset = offset_li(condition.order_x, condition.order_y, sub_mesh,
			   soc_val, temp);
	mutex_unlock(&chip->var_lock);
	return offset;
}

enum comp_offset_typs {
	VG_COMP = 0,
	SOC_OFFSET,
};

static void new_vgcomp_soc_offset_datas(struct device *dev, int type,
		struct rt9428_platform_data *pdata, int size_x, int size_y)
{
	switch (type) {
	case VG_COMP:
		if (pdata->vg_comp.vg_comp_data) {
			devm_kfree(dev, pdata->vg_comp.vg_comp_data);
			pdata->vg_comp.vg_comp_data = NULL;
		}
		if (size_x != 0 && size_y != 0)
			pdata->vg_comp.vg_comp_data = devm_kzalloc(dev,
						size_x * size_y *
						sizeof(struct data_point),
								GFP_KERNEL);
		if (pdata->vg_comp.vg_comp_data) {
			/* replaced with soc ; 20181024 */
			pdata->vg_comp.socnr = size_x;
			pdata->vg_comp.tempnr = size_y;

		} else {
			/* replaced with soc ; 20181024 */
			pdata->vg_comp.socnr = 0;
			pdata->vg_comp.tempnr = 0;
		}
		break;
	case SOC_OFFSET:
		if (pdata->soc_offset.soc_offset_data) {
			devm_kfree(dev,
				pdata->soc_offset.soc_offset_data);
			pdata->soc_offset.soc_offset_data = NULL;
		}
		if (size_x != 0 && size_y != 0)
			pdata->soc_offset.soc_offset_data =
				devm_kzalloc(dev,
				size_x * size_y * sizeof(struct data_point),
				GFP_KERNEL);
		if (pdata->soc_offset.soc_offset_data) {
			pdata->soc_offset.soc_voltnr = size_x;
			pdata->soc_offset.tempnr = size_y;

		} else {
			pdata->soc_offset.soc_voltnr = 0;
			pdata->soc_offset.tempnr = 0;
		}
		break;
	default:
		WARN_ON(1);
		break;
	}
}

struct dt_offset_params {
	int data[3];
};

static int rt_parse_dt(struct device *dev, struct rt9428_platform_data *pdata)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	u32 prop_array[6];
	int j, ret = 0;
	int sizes[2];
	struct dt_offset_params *offset_params;

	pr_info("%s\n", __func__);

	ret = of_property_read_u32_array(np, "rt,dtsi_version",
						pdata->dtsi_version, 2);
	if (ret < 0)
		pdata->dtsi_version[0] =
			pdata->dtsi_version[1] = 0;

	/* VG_COMP */
	sizes[0] = sizes[1] = 0;
	ret = of_property_read_u32_array(np, "rt,vg_comp_size", sizes, 2);
	if (ret < 0)
		pr_err("%s: Can't get prop vg_comp_size (%d)\n", __func__, ret);
	new_vgcomp_soc_offset_datas(dev, VG_COMP, pdata, sizes[0], sizes[1]);
	if (pdata->vg_comp.vg_comp_data) {
		of_property_read_u32_array(np, "rt,vg_comp_data",
			(u32 *)pdata->vg_comp.vg_comp_data,
					sizes[0] * sizes[1] * 6);
	}

	ret = of_property_read_u32_array(np, "rt,vg_comp_interpolation_order",
					 pdata->vg_comp_interpolation_order, 2);
	if (ret < 0)
		pdata->vg_comp_interpolation_order[0] =
			pdata->vg_comp_interpolation_order[1] = 1;

	ret = of_property_read_u32_array(np, "rt,offset_interpolation_order",
					 pdata->offset_interpolation_order, 2);
	if (ret < 0)
		pdata->offset_interpolation_order[0] =
			pdata->offset_interpolation_order[1] = 2;

	sizes[0] = sizes[1] = 0;
	ret = of_property_read_u32_array(np, "rt,soc_offset_size", sizes, 2);
	if (ret < 0)
		pr_err("%s Cant get prop soc_offset_size(%d)\n", __func__, ret);
	new_vgcomp_soc_offset_datas(dev, SOC_OFFSET, pdata, sizes[0], sizes[1]);
	if (pdata->soc_offset.soc_offset_data) {
		offset_params = devm_kzalloc(dev,
					     sizes[0] * sizes[1] *
					     sizeof(struct dt_offset_params),
					     GFP_KERNEL);

		of_property_read_u32_array(np, "rt,soc_offset_data",
				(u32 *)offset_params, sizes[0] * sizes[1] * 3);
		for (j = 0; j < sizes[0] * sizes[1];  ++j) {
			pdata->soc_offset.
				soc_offset_data[j].x = offset_params[j].data[0];
			pdata->soc_offset.
				soc_offset_data[j].y = offset_params[j].data[1];
			pdata->soc_offset.
				soc_offset_data[j].offset =
						offset_params[j].data[2];
		}
	}

	ret = of_property_read_u32(np, "rt,battery_type",
					 &pdata->battery_type);
	if (ret < 0) {
		dev_info(dev, "use default battery_type 4350mV\n");
		pdata->battery_type = 4350;
	}

	ret = of_property_read_u32(np, "rt,irq_mode", &pdata->irq_mode);
	if (ret < 0) {
		dev_info(dev, "use irq mode\n");
		pdata->irq_mode = 1;
	}
	pr_info("%s irq_mode = %d\n", __func__, pdata->irq_mode);

	if (of_property_read_u32(np,
		"rt,alert_threshold", &prop_array[0]) < 0) {
		dev_warn(dev,
			 "no alert_threshold value, using default value\n");
		pdata->alert_threshold = 1;
	} else {
		if (prop_array[0] >= RT9428_SOCL_MIN
		    && prop_array[0] <= RT9428_SOCL_MAX) {
			pdata->alert_threshold = prop_array[0];
			rt9428_init_regval &= (~RT9428_SOCL_MASK);
			rt9428_init_regval |=
			    ((~prop_array[0] + 1) & RT9428_SOCL_MASK);
		} else
			dev_err(dev,
				"alert threshold value is 1, due to out of range (1~32)\n");
	}

	pdata->alert_gpio = of_get_named_gpio(np, "rt,alert_gpio", 0);
	if (of_property_read_u32(np, "rt,full_design", &prop_array[0]) < 0) {
		dev_warn(dev, "no full_design value, using default value\n");
		pdata->full_design = 2540;
	} else
		pdata->full_design = prop_array[0];

#endif /* #ifdef CONFIG_OF */
	return 0;
}

static int rt9428_intr_init(struct rt9428_chip *chip)
{
	int rc = 0;

#ifdef CONFIG_RT9428_KERNEL_INTR
	if (gpio_is_valid(chip->pdata->alert_gpio)) {
		rc = gpio_request_one(chip->pdata->alert_gpio, GPIOF_IN,
				      "rt9428_fg_intr");
		if (rc < 0) {
			dev_err(chip->dev, "gpio request error\n");
			goto err_intr;
		}
		chip->alert_irq = gpio_to_irq(chip->pdata->alert_gpio);
		if (chip->alert_irq < 0) {
			dev_err(chip->dev, "irq value is not valid\n");
			gpio_free(chip->pdata->alert_gpio);
			rc = -EINVAL;
			goto err_intr;
		}
		rc = devm_request_threaded_irq(chip->dev, chip->alert_irq, NULL,
					       rt9428_irq_handler,
					       IRQF_TRIGGER_FALLING |
					       IRQF_ONESHOT, "rt9428_fg_irq",
					       chip);
		if (rc < 0) {
			dev_err(chip->dev, "irq register failed\n");
			gpio_free(chip->pdata->alert_gpio);
			chip->alert_irq = -1;
			rc = -EINVAL;
			goto err_intr;
		}
		enable_irq_wake(chip->alert_irq);
	} else
		rc = -EINVAL;
	__rt9428_intr_init(chip);
err_intr:
#elif defined(CONFIG_MTK_HIFI4DSP_SUPPORT)
	adsp_ipi_registration(ADSP_IPI_GAUGE, rt9428_ipi_msg_dispatcher,
			      "rt9428_irq");
#endif /* CONFIG_RT9428_KERNEL_INTR */

	return rc;
}

static int rt9428_intr_deinit(struct rt9428_chip *chip)
{
#ifdef CONFIG_RT9428_KERNEL_INTR
	if (chip->alert_irq >= 0) {
		devm_free_irq(chip->dev, chip->alert_irq, chip);
		gpio_free(chip->pdata->alert_gpio);
	}
#endif /* CONFIG_RT9428_KERNEL_INTR */
	return 0;
}

static int rt9428_chip_init(struct rt9428_chip *chip)
{
	int regval = 0;

	if (chip->pdata->battery_type == 4350)
		rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8501);
	else
		rt9428_reg_write_word(chip->i2c, RT9428_REG_MFAH, 0x8500);

	/* Set SOC Low threhold, clear ALERT&Sleep */
	regval = rt9428_reg_read_word(chip->i2c, RT9428_REG_CFG0);
	regval &= 0xff40;
	regval |= rt9428_init_regval;
	rt9428_reg_write_word(chip->i2c, RT9428_REG_CFG0, regval);
	chip->online = 1;
	return 0;
}

static int rt9428_psy_register(struct rt9428_chip *chip)
{
	struct power_supply_config psy_cfg = {};

	chip->fg_psy_desc.name = rtdef_fg_name;
	chip->fg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->fg_psy_desc.properties = rt_fg_props;
	chip->fg_psy_desc.num_properties = ARRAY_SIZE(rt_fg_props);
	chip->fg_psy_desc.get_property = rt_fg_get_property;
	chip->fg_psy_desc.set_property = rt_fg_set_property;
	psy_cfg.supplied_to = rt_fg_supply_list;
	psy_cfg.of_node = chip->dev->of_node;
	psy_cfg.drv_data = chip;
	chip->fg_psy = devm_power_supply_register(chip->dev, &chip->fg_psy_desc,
						  &psy_cfg);
	if (IS_ERR(chip->fg_psy))
		return PTR_ERR(chip->fg_psy);
	return 0;
}

static bool rt9428_is_hw_exist(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, RT9428_REG_DEVID1);
	if (ret < 0)
		return false;
	return true;
}

static int rt9428_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	struct rt9428_chip *chip;
	struct rt9428_platform_data *pdata = client->dev.platform_data;
	bool use_dt = client->dev.of_node;

	pr_info("%s\n", __func__);
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "%s : i2c_check_functionality fail\n",
			__func__);
		return -EIO;
	}
	if (!rt9428_is_hw_exist(client)) {
		dev_err(&client->dev, "%s No rt9428 found\n", __func__);
		return -ENODEV;
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	if (use_dt) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err_init;
		}
		rt_parse_dt(&client->dev, pdata);
		chip->pdata = pdata;
	} else {
		dev_err(&client->dev, "No Device Node\n");
		ret = -ENODEV;
		goto err_init;
	}

	chip->i2c = client;
	chip->dev = &client->dev;
	chip->alert_irq = -1;	/* set default irq number = -1; */
	chip->btemp = 250;
	mutex_init(&chip->var_lock);
	mutex_init(&chip->io_lock);
	INIT_DELAYED_WORK(&chip->dwork, rt9428_dwork_func);
	i2c_set_clientdata(client, chip);
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
	g_chip = chip;
#endif /* CONFIG_MTK_HIFI4DSP_SUPPORT */

	/* register power_supply for rt9428 fg */
	ret = rt9428_psy_register(chip);
	if (ret < 0)
		goto err_init1;

	rt9428_chip_init(chip);
	rt9428_intr_init(chip);

	/* queue update work immediately */
	schedule_delayed_work(&chip->dwork, msecs_to_jiffies(100));

	rt_fg_create_attrs(chip->dev);
	dev_info(&client->dev, "driver successfully loaded\n");
	return 0;
err_init1:
	mutex_destroy(&chip->io_lock);
	mutex_destroy(&chip->var_lock);
err_init:
	devm_kfree(&client->dev, chip);
	return ret;
}

static int rt9428_i2c_remove(struct i2c_client *client)
{
	struct rt9428_chip *chip = i2c_get_clientdata(client);

	rt9428_intr_deinit(chip);
	return 0;
}

static int rt9428_suspend(struct device *dev)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	cancel_delayed_work_sync(&chip->dwork);
	chip->suspend = 1;

	return 0;
}

static int rt9428_resume(struct device *dev)
{
	struct rt9428_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	chip->suspend = 0;

	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9428_pm_ops, rt9428_suspend, rt9428_resume);

static const struct i2c_device_id rt_i2c_id[] = {
	{RT9428_DEVICE_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt_i2c_id);

static const struct of_device_id rt_match_table[] = {
	{.compatible = "rt,rt9428",},
	{},
};

static struct i2c_driver rt9428_i2c_driver = {
	.driver = {
		.name = RT9428_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = &rt9428_pm_ops,
	},
	.probe = rt9428_i2c_probe,
	.remove = rt9428_i2c_remove,
	.id_table = rt_i2c_id,
};

static int __init rt9428_init(void)
{
	return i2c_add_driver(&rt9428_i2c_driver);
}
subsys_initcall_sync(rt9428_init);

static void __exit rt9428_exit(void)
{
	i2c_del_driver(&rt9428_i2c_driver);
}
module_exit(rt9428_exit);

MODULE_AUTHOR("cy_huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT9428 Fuelgauge Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT9428_DRV_VER);
