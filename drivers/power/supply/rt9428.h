/*
 *  include/linux/power/rt9428_battery.h
 *  Include header file to Richtek RT9428 fuelgauge driver
 *
 *  Copyright (C) 2014 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef __LINUX_POWER_RT9428_GAUGE_H
#define __LINUX_POWER_RT9428_GAUGE_H

#define RT9428_DEVICE_NAME "rt9428"
#define RT9428_DRV_VER	   "1.0.4_G"

enum {
	RT9428_REG_VBATH = 0x02,
	RT9428_REG_RANGE1_START = RT9428_REG_VBATH,
	RT9428_REG_VBATL,
	RT9428_REG_SOCH,
	RT9428_REG_SOCL,
	RT9428_REG_CTRLH,
	RT9428_REG_CTRLL,
	RT9428_REG_DEVID0,
	RT9428_REG_DEVID1,
	RT9428_REG_STATUS,
	RT9428_REG_CRATE,
	RT9428_REG_CFG0,
	RT9428_REG_CFG1,
	RT9428_REG_OCVH,
	RT9428_REG_OCVL,
	RT9428_REG_RANGE1_STOP = RT9428_REG_OCVL,
	RT9428_REG_MFAH = 0xFE,
	RT9428_REG_RANGE2_START = RT9428_REG_MFAH,
	RT9428_REG_MFAL,
	RT9428_REG_RANGE2_STOP = RT9428_REG_MFAL,
	RT9428_REG_MAX,
};

#define RT9428_SMOOTH_POLL	20
#define RT9428_NORMAL_POLL	30
#define RT9428_SOCALRT_MASK	0x20
#define RT9428_SOCL_SHFT	0
#define RT9428_SOCL_MASK	0x1F
#define RT9428_SOCL_MAX		32
#define RT9428_SOCL_MIN		1

#define RT9428_IRQMODE_MASK	0x40

#define VG_COMP_NR	4

#define VG_COMP_MAX 255
#define VG_COMP_DSG_START 1
#define VG_COMP_CHG_START 3

struct vg_comp_data {
	int data[VG_COMP_NR];
};

struct data_point {
	union {
		int x;
		int voltage;
		int soc;
	};
	union {
		int y;
		int temperature;
	};
	union {
		int data[VG_COMP_NR];
		struct vg_comp_data vg_comp;
		int z;
		int offset;
	};
};

struct vg_comp_table {
	int socnr;  /* replaced with soc ; 20181024 */
	int tempnr;
	struct data_point *vg_comp_data;
};

struct soc_offset_table {
	int soc_voltnr;
	int tempnr;
	struct data_point *soc_offset_data;
};

struct rt9428_platform_data {
	struct vg_comp_table vg_comp;
	struct soc_offset_table soc_offset;
	int vg_comp_interpolation_order[2];
	int offset_interpolation_order[2];
	unsigned int full_design;
	unsigned int dtsi_version[2];
	unsigned int config;
	int battery_type;
	int alert_threshold;
	int soc_comp;
	int alert_gpio;
	int low_cut_off_gain;
	int irq_mode;
};

#ifdef CONFIG_RT9428_FGDBG
#define RT_DBG(format, args...) \
	pr_info("%s:%s() line-%d: " format, RT9428_DEVICE_NAME, __func__, \
		__LINE__, ##args)
#else
#define RT_DBG(format, args...)
#endif /* CONFIG_RT9428_FGDBG */

#endif /* #ifndef __LINUX_POWER_RT9428_GAUGE_H */
