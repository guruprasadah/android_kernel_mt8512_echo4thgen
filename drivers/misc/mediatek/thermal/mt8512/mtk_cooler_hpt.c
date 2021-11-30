/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/seq_file.h>
//#include "mt_hotplug_strategy.h"
//#include "mtk_hotplug_thermal.h"

#define HPT_CORE_NUM (2)
#define tshpt_printk_enable (0)

/*=============================================================
 *Local variable definition
 *=============================================================
 */

static int previous_step = -1;

static unsigned int *hpt_cooler_state;
static struct thermal_cooling_device **hpt_cooler;
static char *hpt_cooler_name;

#define tshpt_printk(fmt, args...)   \
	do {                                    \
		if (tshpt_printk_enable) {                \
			pr_info("Power/CPU_Thermal" fmt, ##args); \
		}                                   \
	} while (0)

static int tscpu_set_hpt_state(void)
{
	int i = 0;

	for (i = 0; i < HPT_CORE_NUM; i++) {
		if (hpt_cooler_state[i] == 1) {
			if (i != previous_step) {
				tshpt_printk("%s prev step=%d, curr step=%d\n",
					__func__, previous_step, i);
				previous_step = i;
				hpt_set_cpu_num_limit(i + 1, 0);
			}
			break;
		}
	}

	/* If temp drop to our expect value,
	 * we need to restore initial cpu core setting
	 */
	if (i == HPT_CORE_NUM) {
		if (previous_step != -1) {
			tshpt_printk("Free all thermal limit, previous_step=%d\n",
				previous_step);
			previous_step = -1;
			hpt_set_cpu_num_limit(HPT_CORE_NUM, 0);
		}
	}
	return 0;
}

static int hpt_cpu_get_max_state(
	struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	*state = 1;
	return 0;
}

static int hpt_cpu_get_cur_state(
		struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	int i = 0;

	for (i = 0; i < HPT_CORE_NUM; i++) {
		if (!strcmp(cdev->type, &hpt_cooler_name[i * 20]))
			*state = hpt_cooler_state[i];
	}
	return 0;
}

static int hpt_cpu_set_cur_state(
		struct thermal_cooling_device *cdev,
		unsigned long state)
{
	int i = 0;

	for (i = 0; i < HPT_CORE_NUM; i++) {
		if (!strcmp(cdev->type, &hpt_cooler_name[i * 20])) {
			hpt_cooler_state[i] = state;
			tscpu_set_hpt_state();
			break;
		}
	}
	return 0;
}

static struct thermal_cooling_device_ops mtktscpu_cooling_hpt_ops = {
	.get_max_state = hpt_cpu_get_max_state,
	.get_cur_state = hpt_cpu_get_cur_state,
	.set_cur_state = hpt_cpu_set_cur_state,
};

/* Init local structure for hpt coolers */
static int init_cooler(void)
{
	int i;
	int num = HPT_CORE_NUM;

	hpt_cooler_state = kzalloc((num) * sizeof(unsigned int), GFP_KERNEL);
	if (hpt_cooler_state == NULL)
		return -ENOMEM;

	hpt_cooler = kzalloc(
			(num) * sizeof(struct thermal_cooling_device *),
			GFP_KERNEL);
	if (hpt_cooler == NULL)
		return -ENOMEM;

	hpt_cooler_name = kzalloc((num) * sizeof(char) * 20, GFP_KERNEL);
	if (hpt_cooler_name == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++)
		sprintf(hpt_cooler_name + (i * 20), "mtk-cl-hpt%02d", i+1);

	return 0;
}

static int __init mtk_cooler_hpt_init(void)
{
	int err = 0, i;

	tshpt_printk("%s start\n", __func__);

	err = init_cooler();
	if (err) {
		tshpt_printk("%s fail\n", __func__);
		return err;
	}
	for (i = 0; i < HPT_CORE_NUM; i++) {
		hpt_cooler[i] = mtk_thermal_cooling_device_register(
			&hpt_cooler_name[i * 20], NULL,
			&mtktscpu_cooling_hpt_ops);
	}

	tshpt_printk("%s end\n", __func__);
	return 0;
}

static void __exit mtk_cooler_hpt_exit(void)
{
	int i;

	tshpt_printk("%s\n", __func__);
	for (i = 0; i < HPT_CORE_NUM; i++) {
		if (hpt_cooler[i]) {
			mtk_thermal_cooling_device_unregister(hpt_cooler[i]);
			hpt_cooler[i] = NULL;
		}
	}
}
module_init(mtk_cooler_hpt_init);
module_exit(mtk_cooler_hpt_exit);
