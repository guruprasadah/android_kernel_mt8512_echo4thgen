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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/of.h>

#ifdef CONFIG_MTK_CHARGER_CLASS
#include <linux/power/mtk_charger_class.h>
#endif /* CONFIG_MTK_CHARGER_CLASS */

#ifdef CONFIG_CHARGER_MTK
#include <mt-plat/charger_type.h>
#include <linux/power/mtk_charger_consumer.h>
#endif /* CONFIG_CHARGER_MTK */

struct rt_battery_data {
	int status;	/* POWER_SUPPLY_PROP_STATUS */
	int present;	/* POWER_SUPPLY_PROP_PRESENT */
	int health;	/* POWER_SUPPLY_PROP_HEALTH */
	int capacity;	/* POWER_SUPPLY_PROP_CAPACITY */
	int volt_now;	/* POWER_SUPPLY_PROP_VOLTAGE_NOW */
	int volt_max;	/* POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN */
	int chg_full;	/* POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN */
	int temp;	/* POWER_SUPPLY_PROP_TEMP */
};

static struct rt_battery_data rt_batt_data_defval = {
	.status = POWER_SUPPLY_STATUS_DISCHARGING,
	.present = 1,
	.health = POWER_SUPPLY_HEALTH_GOOD,
	.capacity = 50,
	.volt_now = 4350,
	.volt_max = 4350,
	.chg_full = 2540,
	.temp = 25,
};

struct rt_battery_info {
	struct device *dev;
	struct power_supply *chg_psy;
	struct power_supply *ac_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct mutex lock;
	struct rt_battery_data rbd;
	bool chg_online;

#ifdef CONFIG_CHARGER_MTK
	enum charger_type chg_type;
	struct charger_consumer *chg_consumer;
	struct notifier_block chg_nb;
#endif /* CONFIG_CHARGER_MTK */

#ifdef CONFIG_MTK_CHARGER_CLASS
	struct charger_device *chgdev;
#endif /* CONFIG_MTK_CHARGER_CLASS */
};

static int rt_chg_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);

	mutex_lock(&rbi->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef CONFIG_CHARGER_MTK
		/* Force to 1 for all charger type but unknown */
		val->intval = (rbi->chg_type == CHARGER_UNKNOWN) ? 0 : 1;
#else
		val->intval = rbi->chg_online;
#endif /* CONFIG_CHARGER_MTK */
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
#ifdef CONFIG_CHARGER_MTK
		val->intval = rbi->chg_type;
#else
		val->intval = 0;
#endif /* CONFIG_CHARGER_MTK */
		break;
	default:
		ret = -ENODATA;
		break;
	}

	mutex_unlock(&rbi->lock);
	return ret;
}

static int rt_chg_set_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);

	mutex_lock(&rbi->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		rbi->chg_online = val->intval ? true : false;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
#ifdef CONFIG_CHARGER_MTK
		rbi->chg_type = val->intval;
#endif /* CONFIG_CHARGER_MTK */
		break;
	default:
		ret = -EINVAL;
		break;
	}

#ifdef CONFIG_CHARGER_MTK
	if (psp == POWER_SUPPLY_PROP_CHARGE_TYPE) {
		if (rbi->chg_type == STANDARD_HOST ||
		    rbi->chg_type == CHARGING_HOST)
			mt_usb_connect();
		else
			mt_usb_disconnect();

		/* Inform mtk charger */
		mtk_charger_int_handler();
	}
#endif /* CONFIG_CHARGER_MTK */

	power_supply_changed(rbi->ac_psy);
	power_supply_changed(rbi->usb_psy);

	mutex_unlock(&rbi->lock);
	return ret;
}

static int rt_ac_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);

	mutex_lock(&rbi->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef CONFIG_CHARGER_MTK
		if ((rbi->chg_type == STANDARD_HOST) ||
		    (rbi->chg_type == CHARGING_HOST) ||
		    (rbi->chg_type == CHARGER_UNKNOWN))
			val->intval = 0;
		else
			val->intval = 1;
#endif /* CONFIG_CHARGER_MTK */
		break;
	default:
		ret = -ENODATA;
		break;
	}

	mutex_unlock(&rbi->lock);
	return ret;
}

static int rt_usb_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);

	mutex_lock(&rbi->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#ifdef CONFIG_CHARGER_MTK
		if ((rbi->chg_type == STANDARD_HOST) ||
		    (rbi->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
#endif /* CONFIG_CHARGER_MTK */
		break;
	default:
		ret = -ENODATA;
		break;
	}

	mutex_unlock(&rbi->lock);
	return ret;
}

static int rt_batt_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);
	struct rt_battery_data *rbd = &rbi->rbd;
	struct power_supply *gauge_psy;

	mutex_lock(&rbi->lock);
	gauge_psy = power_supply_get_by_name("rt-fuelgauge");

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = rbd->status;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = rbd->present;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = rbd->health;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = rbd->temp;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (gauge_psy) {
			ret = power_supply_get_property(gauge_psy, psp, val);
			if (ret >= 0)
				rbd->capacity = val->intval;
		} else
			val->intval = rbd->capacity;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (gauge_psy) {
			ret = power_supply_get_property(gauge_psy, psp, val);
			if (ret >= 0)
				rbd->volt_now = val->intval;
		} else
			val->intval = rbd->volt_now;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		if (gauge_psy) {
			ret = power_supply_get_property(gauge_psy, psp, val);
			if (ret >= 0)
				rbd->volt_max = val->intval;
		} else
			val->intval = rbd->volt_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (gauge_psy) {
			ret = power_supply_get_property(gauge_psy, psp, val);
			if (ret >= 0)
				rbd->chg_full = val->intval;
		} else
			val->intval = rbd->chg_full;
		break;
	default:
		ret = -ENODATA;
		break;
	}

	if (gauge_psy)
		power_supply_put(gauge_psy);
	mutex_unlock(&rbi->lock);
	return ret;
}

static int rt_batt_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
	struct rt_battery_info *rbi = power_supply_get_drvdata(psy);
	struct rt_battery_data *rbd = &rbi->rbd;
	struct power_supply *gauge_psy;

	mutex_lock(&rbi->lock);
	gauge_psy = power_supply_get_by_name("rt-fuelgauge");
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		if (gauge_psy)
			ret = power_supply_set_property(gauge_psy, psp, val);
		rbd->temp = val->intval;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&rbi->lock);
	return ret;
}

static enum power_supply_property rt_charger_prop[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static enum power_supply_property rt_battery_prop[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TEMP,
};

static char *charger_supplied_to[] = {
	"battery",
};

static const struct power_supply_desc rt_chg_desc = {
	.name			= "charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= rt_charger_prop,
	.num_properties		= ARRAY_SIZE(rt_charger_prop),
	.get_property		= rt_chg_get_property,
	.set_property		= rt_chg_set_property,
};

static const struct power_supply_desc rt_ac_desc = {
	.name			= "ac",
	.type			= POWER_SUPPLY_TYPE_MAINS,
	.properties		= rt_charger_prop,
	.num_properties		= ARRAY_SIZE(rt_charger_prop),
	.get_property		= rt_ac_get_property,
};

static const struct power_supply_desc rt_usb_desc = {
	.name			= "usb",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= rt_charger_prop,
	.num_properties		= ARRAY_SIZE(rt_charger_prop),
	.get_property		= rt_usb_get_property,
};

static const struct power_supply_desc rt_batt_desc = {
	.name			= "battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= rt_battery_prop,
	.num_properties		= ARRAY_SIZE(rt_battery_prop),
	.get_property		= rt_batt_get_property,
	.set_property		= rt_batt_set_property,
};

#ifdef CONFIG_CHARGER_MTK
static int rt_chg_notifier_callback(struct notifier_block *nb,
				    unsigned long event, void *v)
{
	struct rt_battery_info *rbi = container_of(nb, struct rt_battery_info,
						   chg_nb);
	struct rt_battery_data *rbd = &rbi->rbd;

	dev_info(rbi->dev, "%s %ld\n", __func__, event);

	switch (event) {
	case CHGMGR_NOTIFY_NORMAL:
	case CHGMGR_NOTIFY_START_CHARGING:
		rbd->status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case CHGMGR_NOTIFY_STOP_CHARGING:
		rbd->status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case CHGMGR_NOTIFY_ERROR:
		rbd->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}
	return 0;
}
#endif /* CONFIG_CHARGER_MTK */

static int rt_battery_probe(struct platform_device *pdev)
{
	int ret;
	struct rt_battery_info *rbi = NULL;
	struct power_supply_config psy_cfg = {};

	dev_info(&pdev->dev, "%s\n", __func__);
	rbi = devm_kzalloc(&pdev->dev, sizeof(*rbi), GFP_KERNEL);
	if (!rbi)
		return -ENOMEM;
	rbi->dev = &pdev->dev;
	mutex_init(&rbi->lock);
	memcpy(&rbi->rbd, &rt_batt_data_defval, sizeof(struct rt_battery_data));

	psy_cfg.of_node = rbi->dev->of_node;
	psy_cfg.drv_data = rbi;

	/* battery */
	rbi->batt_psy = devm_power_supply_register(rbi->dev, &rt_batt_desc,
						   &psy_cfg);
	if (IS_ERR(rbi->batt_psy)) {
		ret = PTR_ERR(rbi->batt_psy);
		goto err;
	}

	/* charger */
	rbi->chg_psy = devm_power_supply_register(rbi->dev, &rt_chg_desc,
						  &psy_cfg);
	if (IS_ERR(rbi->chg_psy)) {
		ret = PTR_ERR(rbi->chg_psy);
		goto err;
	}

	psy_cfg.supplied_to = charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(charger_supplied_to);

	/* ac */
	rbi->ac_psy = devm_power_supply_register(rbi->dev, &rt_ac_desc,
						 &psy_cfg);
	if (IS_ERR(rbi->ac_psy)) {
		ret = PTR_ERR(rbi->ac_psy);
		goto err;
	}

	/* usb */
	rbi->usb_psy = devm_power_supply_register(rbi->dev, &rt_usb_desc,
						  &psy_cfg);
	if (IS_ERR(rbi->usb_psy)) {
		ret = PTR_ERR(rbi->usb_psy);
		goto err;
	}

#ifdef CONFIG_MTK_CHARGER_CLASS
	rbi->chgdev = get_charger_by_name("primary_chg");
	if (!rbi->chgdev) {
		dev_err(rbi->dev, "%s get charger dev fail\n", __func__);
		ret = -ENODEV;
		goto err;
	}
#endif /* CONFIG_MTK_CHARGER_CLASS */

#ifdef CONFIG_CHARGER_MTK
	rbi->chg_consumer = mtk_charger_manager_get_consumer(rbi->dev);
	if (!rbi->chg_consumer) {
		dev_err(rbi->dev, "%s get charger consumer fail\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	rbi->chg_nb.notifier_call = rt_chg_notifier_callback;
	ret = mtk_charger_manager_register_notifier(rbi->chg_consumer,
						    &rbi->chg_nb);
	if (ret < 0) {
		dev_err(rbi->dev, "%s reg chgmgr notifier fail\n", __func__);
		goto err;
	}
#endif /* CONFIG_CHARGER_MTK */
	dev_info(rbi->dev, "%s successfully\n", __func__);
	return 0;
err:
	mutex_destroy(&rbi->lock);
	return ret;
}

static int rt_battery_remove(struct platform_device *pdev)
{
	struct rt_battery_info *rbi = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);
	if (rbi)
		mutex_destroy(&rbi->lock);
	return 0;
}

static void rt_battery_shutdown(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);
}

static int rt_battery_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int rt_battery_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt_battery_pm_ops, rt_battery_suspend,
			 rt_battery_resume);

static const struct platform_device_id rt_battery_id_table[] = {
	{ "rt_battery", },
	{},
};
MODULE_DEVICE_TABLE(platform, rt_battery_id_table);

#ifdef CONFIG_OF
static const struct of_device_id rt_battery_of_match_table[] = {
	{ .compatible = "richtek,battery" },
	{},
};
MODULE_DEVICE_TABLE(of, rt_battery_of_match_table);
#endif /* CONFIG_OF */

static struct platform_driver rt_battery_driver = {
	.probe	= rt_battery_probe,
	.remove = rt_battery_remove,
	.shutdown = rt_battery_shutdown,
	.driver = {
		.name = "rt_battery",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt_battery_of_match_table),
		.pm = &rt_battery_pm_ops,
	},
	.id_table = rt_battery_id_table,
};

static int __init rt_battery_init(void)
{
	platform_driver_register(&rt_battery_driver);
	return 0;
}
device_initcall_sync(rt_battery_init);

static void __exit rt_battery_exit(void)
{
	platform_driver_unregister(&rt_battery_driver);
}
module_exit(rt_battery_exit);
