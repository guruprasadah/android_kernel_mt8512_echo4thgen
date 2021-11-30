/*
 * Copyright (C) 2019 MediaTek Inc.
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

#if defined(__KERNEL__)
#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/bug.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/sync_write.h>
#include "mtk_dcm.h"
#else
#include <sync_write.h>
#include <common.h>
#endif

static void __iomem *topckctrl_base;
static void __iomem *mcucfg_base;
static void __iomem *emi_reg_base;
static void __iomem *emi_chn_reg_base;
static void __iomem *infracfg_base;
static phys_addr_t mcucfg_phys_base;

#define MCUCFG_NODE		"mediatek,mt8512-mcucfg"
#define INFRACFG_NODE		"mediatek,mt8512-infrasys"
#define TOPCKGEN_NODE		"mediatek,mt8512-topckgen"
#define EMI_REG_NODE		"mediatek,mt8512-emi"

#undef INFRACFG_BASE
#undef MCUCFG_BASE
#undef TOPCKCTRL_BASE
#undef EMI_REG_BASE
#define INFRACFG_BASE		(infracfg_base)	/*0x10001000 */
#define MCUCFG_BASE		(mcucfg_base)/*0x10200000 */
#define TOPCKCTRL_BASE		(topckctrl_base)/*0x10000000 */
#define EMI_REG_BASE		(emi_reg_base)/*0x10219000 */
#define EMI_CHN_REG_BASE	(emi_chn_reg_base)/*0x1022F000*/

/*  MCUCFG */
#define MP0_RGU_DCM_CONFIG		(MCUCFG_BASE + 0x0088)
#define EMI_WFIFO			(MCUCFG_BASE + 0x0B5C)
#define ACLKEN_DIV			(MCUCFG_BASE + 0x0640)
#define L2C_SRAM_CTRL			(MCUCFG_BASE + 0x0648)
#define CCI_STATUS			(MCUCFG_BASE + 0x0660)
#define MCUSYS_BUS_FABRIC_DCM_CTRL	(MCUCFG_BASE + 0x0668)
#define MCU_MISC_DCM_CTRL		(MCUCFG_BASE + 0x066C)
#define MP_CCI_ADB400_DCM_CONFIG	(MCUCFG_BASE + 0x0740)
#define GIC_SYNC_DCM			(MCUCFG_BASE + 0x0758)
#define BUS_PLL_DIVIDER_CFG		(MCUCFG_BASE + 0x7c0)

/* EMI */
#define EMI_CONH	(EMI_REG_BASE + 0x68)	/* 0x10219068 */
#define EMI_CONM	(EMI_REG_BASE + 0x60)	/* 0x10219060 */
#define EMI_CONB	(EMI_CHN_REG_BASE + 0x8)/* 0x1022F008 */

/* INFRA */
#define INFRABUS_DCMCTL	(INFRACFG_BASE + 0x0070)/* 0x10001070 */
#define PERIBUS_DCMCTL	(INFRACFG_BASE + 0x0074)/* 0x10001074 */
#define P2P_RX_CLK_ON	(INFRACFG_BASE + 0x00A0)

#define USING_KernelLOG
#define TAG				"[Power/dcm] "

#define dcm_err(fmt, args...)		pr_info(TAG fmt, ##args)
#define dcm_warn(fmt, args...)		pr_info(TAG fmt, ##args)
#define dcm_info(fmt, args...)		pr_info(TAG fmt, ##args)
#define dcm_ver(fmt, args...)		pr_debug(TAG fmt, ##args)
#define dcm_dbg(fmt, args...)		pr_debug(TAG fmt, ##args)

/** macro **/
#define and(v, a) ((v) & (a))
#define or(v, o) ((v) | (o))
#define aor(v, a, o) (((v) & (a)) | (o))

#define reg_read(addr)		__raw_readl(addr)
#define reg_write(addr, val)	mt_reg_sync_writel(val, addr)

#define MCUSYS_SMC_WRITE(addr, val)	mt_reg_sync_writel(val, addr)

#define DCM_OFF (0)
#define DCM_ON	(1)

/** global **/
static DEFINE_MUTEX(dcm_lock);
static unsigned int dcm_initiated;

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
typedef int (*DCM_FUNC) (int);

/** 0x10001014	INFRA_TOPCKGEN_DCMDBC
 * 6	0	topckgen_dcm_dbc_cnt.
 * BUT, this field does not behave as its name.
 * only topckgen_dcm_dbc_cnt[0] is using as ARMPLL DCM mode 1 switch.
 **/

enum ENUM_ARMCORE_DCM {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
};

#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK ((0x1 << 11) | \
					(0x1 << 24) | (0x1 << 25))
#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_ON ((0x1 << 11) | \
					(0x1 << 24) | (0x1 << 25))
#define MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_OFF ((0x0 << 11) | \
					(0x0 << 24) | (0x0 << 25))

int dcm_armcore(enum ENUM_ARMCORE_DCM mode)
{
	if (mode == ARMCORE_DCM_OFF) {
		/* disable mode 1 */
		reg_write(BUS_PLL_DIVIDER_CFG,
		(reg_read(BUS_PLL_DIVIDER_CFG) &
		~MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
		MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_OFF);
	} else if (mode == ARMCORE_DCM_MODE1) {
		/* enable mode 1 */
		reg_write(BUS_PLL_DIVIDER_CFG,
		(reg_read(BUS_PLL_DIVIDER_CFG) &
		~MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_MASK) |
		MCU_MISCCFG_BUS_ARM_PLL_DIVIDER_DCM_REG0_ON);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/**************************
 * MCUSYS DCM
 **************************/
enum ENUM_MCUSYS_DCM {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
};

/*  MCUCFG DCM setting */
#define MCU_MISCCFG_ADB400_DCM_REG0_MASK ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 2) | (0x1 << 6))
#define MCU_MISCCFG_ADB400_DCM_REG0_ON ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 2) | (0x1 << 6))
#define MCU_MISCCFG_ADB400_DCM_REG0_OFF ((0x0 << 0) | \
	(0x0 << 1) | (0x0 << 2) | (0x0 << 6))

#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK ((0x1 << 8))
#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_ON ((0x1 << 8))
#define MCU_MISCCFG_BUS_CLOCK_DCM_REG0_OFF ((0x0 << 8))

#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 4) | (0x1 << 5) | (0x1 << 8) | \
	(0x1 << 9) | (0x1 << 12) | (0x1 << 16))
#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_ON ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 4) | (0x1 << 5) | (0x1 << 8) | \
	(0x1 << 9) | (0x1 << 12) | (0x1 << 16))
#define MCU_MISCCFG_BUS_FABRIC_DCM_REG0_OFF ((0x0 << 0) | \
	(0x0 << 1) | (0x0 << 4) | (0x0 << 5) | (0x0 << 8) | \
	(0x0 << 9) | (0x0 << 12) | (0x0 << 16))

#define MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK ((0x1 << 0))
#define MCU_MISCCFG_L2_SHARED_DCM_REG0_ON ((0x1 << 0))
#define MCU_MISCCFG_L2_SHARED_DCM_REG0_OFF ((0x0 << 0))

#define MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK ((0x1 << 0))
#define MCU_MISCCFG_MCU_MISC_DCM_REG0_ON ((0x1 << 0))
#define MCU_MISCCFG_MCU_MISC_DCM_REG0_OFF ((0x0 << 0))

#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK ((0x1 << 0) | \
					(0x1 << 1))
#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_ON ((0x1 << 0) | \
					(0x1 << 1))
#define MCU_MISCCFG_GIC_SYNC_DCM_REG0_OFF ((0x0 << 0) | \
					(0x0 << 1))

#define MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK ((0x1 << 0))
#define MP0_CPUCFG_MP0_RGU_DCM_REG0_ON ((0x1 << 0))
#define MP0_CPUCFG_MP0_RGU_DCM_REG0_OFF ((0x0 << 0))

int dcm_mcusys(enum ENUM_MCUSYS_DCM on)
{
	if (on == MCUSYS_DCM_OFF) {
		reg_write(MP_CCI_ADB400_DCM_CONFIG,
			(reg_read(MP_CCI_ADB400_DCM_CONFIG) &
			~MCU_MISCCFG_ADB400_DCM_REG0_MASK) |
			MCU_MISCCFG_ADB400_DCM_REG0_OFF);
		reg_write(CCI_STATUS,
			(reg_read(CCI_STATUS) &
			~MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_CLOCK_DCM_REG0_OFF);
		reg_write(MCUSYS_BUS_FABRIC_DCM_CTRL,
			(reg_read(MCUSYS_BUS_FABRIC_DCM_CTRL) &
			~MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_FABRIC_DCM_REG0_OFF);
		reg_write(L2C_SRAM_CTRL,
			(reg_read(L2C_SRAM_CTRL) &
			~MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK) |
			MCU_MISCCFG_L2_SHARED_DCM_REG0_OFF);
		reg_write(MCU_MISC_DCM_CTRL,
			(reg_read(MCU_MISC_DCM_CTRL) &
			~MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK) |
			MCU_MISCCFG_MCU_MISC_DCM_REG0_OFF);
		reg_write(GIC_SYNC_DCM,
			(reg_read(GIC_SYNC_DCM) &
			~MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_GIC_SYNC_DCM_REG0_OFF);
		reg_write(MP0_RGU_DCM_CONFIG,
			(reg_read(MP0_RGU_DCM_CONFIG) &
			~MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK) |
			MP0_CPUCFG_MP0_RGU_DCM_REG0_OFF);
	} else if (on == MCUSYS_DCM_ON) {
		reg_write(MP_CCI_ADB400_DCM_CONFIG,
			(reg_read(MP_CCI_ADB400_DCM_CONFIG) &
			~MCU_MISCCFG_ADB400_DCM_REG0_MASK) |
			MCU_MISCCFG_ADB400_DCM_REG0_ON);
		reg_write(CCI_STATUS,
			(reg_read(CCI_STATUS) &
			~MCU_MISCCFG_BUS_CLOCK_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_CLOCK_DCM_REG0_ON);
		reg_write(MCUSYS_BUS_FABRIC_DCM_CTRL,
			(reg_read(MCUSYS_BUS_FABRIC_DCM_CTRL) &
			~MCU_MISCCFG_BUS_FABRIC_DCM_REG0_MASK) |
			MCU_MISCCFG_BUS_FABRIC_DCM_REG0_ON);
		reg_write(L2C_SRAM_CTRL,
			(reg_read(L2C_SRAM_CTRL) &
			~MCU_MISCCFG_L2_SHARED_DCM_REG0_MASK) |
			MCU_MISCCFG_L2_SHARED_DCM_REG0_ON);
		reg_write(MCU_MISC_DCM_CTRL,
			(reg_read(MCU_MISC_DCM_CTRL) &
			~MCU_MISCCFG_MCU_MISC_DCM_REG0_MASK) |
			MCU_MISCCFG_MCU_MISC_DCM_REG0_ON);
		reg_write(GIC_SYNC_DCM,
			(reg_read(GIC_SYNC_DCM) &
			~MCU_MISCCFG_GIC_SYNC_DCM_REG0_MASK) |
			MCU_MISCCFG_GIC_SYNC_DCM_REG0_ON);
		reg_write(MP0_RGU_DCM_CONFIG,
			(reg_read(MP0_RGU_DCM_CONFIG) &
			~MP0_CPUCFG_MP0_RGU_DCM_REG0_MASK) |
			MP0_CPUCFG_MP0_RGU_DCM_REG0_ON);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

#define PERI_DCM_SFSEL_MASK	(0x1f<<10)
#define PERI_DCM_FSEL_MASK	(0x1f<<5)
#define PERI_DCM_DBC_CNT_MASK	(0x1f<<15)
#define PERI_DCM_DBC_EN_MASK	(1<<20)

#define INFRA_DCM_SFSEL_MASK	(0x1f<<10)
#define INFRA_DCM_FSEL_MASK	(0x1f<<5)
#define INFRA_DCM_DBC_CNT_MASK	(0x1f<<15)
#define INFRA_DCM_DBC_EN_MASK	(1<<20)

enum ENUM_INFRA_DCM {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
};

enum ENUM_INFRA_DCM_DBC {
	INFRA_DCM_DBC_OFF = DCM_OFF,
	INFRA_DCM_DBC_ON = DCM_ON,
};

/* cnt : 0~0x1f */
int dcm_infra_dbc(enum ENUM_INFRA_DCM_DBC on, int cnt)
{
	int value;

	cnt &= 0x1f;
	on = (on != 0) ? 1 : 0;
	value = (cnt << 15) | (on << 20);

	reg_write(INFRABUS_DCMCTL,
		aor(reg_read(INFRABUS_DCMCTL),
		~(INFRA_DCM_DBC_CNT_MASK | INFRA_DCM_DBC_EN_MASK),
		value));

	return 0;
}

/**
 * 5'b10000: /1
 * 5'b01000: /2
 * 5'b00100: /4
 * 5'b00010: /8
 * 5'b00001: /16
 * 5'b00000: /32
 */
enum ENUM_INFRA_SFSEL {
	INFRA_DCM_SFSEL_DIV_1 = 0x10,
	INFRA_DCM_SFSEL_DIV_2 = 0x08,
	INFRA_DCM_SFSEL_DIV_4 = 0x04,
	INFRA_DCM_SFSEL_DIV_8 = 0x02,
	INFRA_DCM_SFSEL_DIV_16 = 0x01,
	INFRA_DCM_SFSEL_DIV_32 = 0x00,
};

int dcm_infra_sfsel(int cnt)
{
	int value;

	cnt &= 0x1f;
	value = (cnt << 10);

	reg_write(INFRABUS_DCMCTL,
		aor(reg_read(INFRABUS_DCMCTL),
		~INFRA_DCM_SFSEL_MASK, value));

	return 0;
}

/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 **/
int dcm_infra_rate(unsigned int full, unsigned int sfel)
{
	full = 0x10 >> full;
	sfel = 0x10 >> sfel;

	reg_write(INFRABUS_DCMCTL,
		aor(reg_read(INFRABUS_DCMCTL),
		~(PERI_DCM_SFSEL_MASK | INFRA_DCM_FSEL_MASK),
		(full << 5) | (sfel << 10)));

	return 0;
}

#define INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_MASK ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 2) | (0x1 << 3) | (0x1 << 4) | \
	(0x1f << 5) | (0x1f << 10) | (0x1 << 20) | (0x1 << 21) | \
	(0x1 << 22) | (0x1 << 23) | (0x1 << 30))
#define INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_ON ((0x1 << 0) | \
	(0x1 << 1) | (0x0 << 2) | (0x0 << 3) | (0x0 << 4) | \
	(0x10 << 5) | (0x0 << 10) | (0x1 << 20) | (0x0 << 21) | \
	(0x1 << 22) | (0x1 << 23) | (0x1 << 30))
#define INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_OFF ((0x0 << 0) | \
	(0x0 << 1) | (0x0 << 2) | (0x0 << 3) | (0x0 << 4) | \
	(0x10 << 5) | (0x0 << 10) | (0x0 << 20) | (0x1 << 21) | \
	(0x1 << 22) | (0x0 << 23) | (0x0 << 30))

int dcm_infra(enum ENUM_INFRA_DCM on)
{
	if (on == INFRA_DCM_ON) {
		reg_write(INFRABUS_DCMCTL,
		(reg_read(INFRABUS_DCMCTL) &
		~INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_MASK) |
		INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_ON);
	} else if (on == INFRA_DCM_OFF) {
		reg_write(INFRABUS_DCMCTL,
		(reg_read(INFRABUS_DCMCTL) &
		~INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_MASK) |
		INFRACFG_AO_DCM_INFRABUS_GROUP_REG0_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

enum ENUM_PERI_DCM {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
};

/* cnt: 0~0x1f */
int dcm_peri_dbc(int on, int cnt)
{
	int value;

	WARN_ON(cnt > 0x1f);

	on = (on != 0) ? 1 : 0;
	value = (cnt << 15) | (on << 20);

	reg_write(PERIBUS_DCMCTL,
		aor(reg_read(PERIBUS_DCMCTL),
		~(PERI_DCM_DBC_CNT_MASK | PERI_DCM_DBC_EN_MASK),
		value));

	return 0;
}

/**
 * 5'b10000: /1
 * 5'b01000: /2
 * 5'b00100: /4
 * 5'b00010: /8
 * 5'b00001: /16
 * 5'b00000: /32
 */
enum ENUM_PERI_SFSEL {
	PERI_DCM_SFSEL_DIV_1 = 0x10,
	PERI_DCM_SFSEL_DIV_2 = 0x08,
	PERI_DCM_SFSEL_DIV_4 = 0x04,
	PERI_DCM_SFSEL_DIV_8 = 0x02,
	PERI_DCM_SFSEL_DIV_16 = 0x01,
	PERI_DCM_SFSEL_DIV_32 = 0x00,
};

int dcm_peri_sfsel(int cnt)
{
	int value;

	cnt &= 0x1f;
	value = (cnt << 10);

	reg_write(PERIBUS_DCMCTL,
		aor(reg_read(PERIBUS_DCMCTL),
		~PERI_DCM_SFSEL_MASK, value));

	return 0;
}

/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 * default: 5, 5, 5
 **/
int dcm_peri_rate(unsigned int full, unsigned int sfel)
{

	full = 0x10 >> full;
	sfel = 0x10 >> sfel;

	reg_write(PERIBUS_DCMCTL,
		aor(reg_read(PERIBUS_DCMCTL),
		~(PERI_DCM_SFSEL_MASK | PERI_DCM_FSEL_MASK),
		(full << 5) | (sfel << 10)));

	return 0;
}

#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_MASK ((0x1 << 0) | \
	(0x1 << 1) | (0x1 << 3) | (0x1 << 4) | (0x1f << 5) | \
	(0x1f << 10) | (0x1f << 15) | (0x1 << 20))
#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_MASK ((0xf << 0))
#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_ON ((0x1 << 0) | \
	(0x1 << 1) | (0x0 << 3) | (0x0 << 4) | (0x10 << 5) | \
	(0x0 << 10) | (0x1f << 15) | (0x1 << 20))
#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_ON ((0x0 << 0))
#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_OFF ((0x0 << 0) | \
	(0x0 << 1) | (0x0 << 3) | (0x0 << 4) | (0x10 << 5) | \
	(0x0 << 10) | (0x0 << 15) | (0x0 << 20))
#define INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_OFF ((0xf << 0))

int dcm_peri(enum ENUM_PERI_DCM on)
{
	if (on == PERI_DCM_ON) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_MASK) |
			INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_ON);
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_MASK) |
			INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_ON);
	} else if (on == PERI_DCM_OFF) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_MASK) |
			INFRACFG_AO_DCM_PERIBUS_GROUP_REG0_OFF);
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_MASK) |
			INFRACFG_AO_DCM_PERIBUS_GROUP_REG1_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

enum ENUM_PMIC_DCM {
	PMIC_DCM_OFF = DCM_OFF,
	PMIC_DCM_ON = DCM_ON,
};

#define PMIC_SFSEL_MASK		(0x1f<<23)
#define PMIC_DCM_EN_MASK	((0x1 << 22) | (0x1f << 23))
#define PMIC_DCM_EN_ON		((0x1 << 22) | (0x0 << 23))
#define PMIC_DCM_EN_OFF		((0x0 << 22) | (0x0 << 23))

/** input argument
 * 0: 1/1
 * 1: 1/2
 * 2: 1/4
 * 3: 1/8
 * 4: 1/16
 * 5: 1/32
 **/
int dcm_pmic_rate(unsigned int sfel)
{

	sfel = 0x10 >> sfel;

	reg_write(PERIBUS_DCMCTL,
		aor(reg_read(PERIBUS_DCMCTL),
		~PMIC_SFSEL_MASK, (sfel << 23)));

	return 0;
}

int dcm_pmic(enum ENUM_PMIC_DCM on)
{
	if (on == PMIC_DCM_ON) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~PMIC_DCM_EN_MASK) |
			PMIC_DCM_EN_ON);
	} else if (on == PMIC_DCM_OFF) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~PMIC_DCM_EN_MASK) |
			PMIC_DCM_EN_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

enum ENUM_USB_DCM {
	USB_DCM_OFF = DCM_OFF,
	USB_DCM_ON = DCM_ON,
};

#define USB_DCM_EN_MASK	((0x1 << 21) | (0x1 << 28) | (0x1 << 31))
#define USB_DCM_EN_ON	((0x1 << 21) | (0x1 << 28) | (0x1 << 31))
#define USB_DCM_EN_OFF	((0x0 << 21) | (0x0 << 28) | (0x0 << 31))

int dcm_usb(enum ENUM_USB_DCM on)
{
	if (on == USB_DCM_ON) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~USB_DCM_EN_MASK) |
			USB_DCM_EN_ON);
	} else if (on == USB_DCM_OFF) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~USB_DCM_EN_MASK) |
			USB_DCM_EN_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

enum ENUM_AUDIO_DCM {
	AUDIO_DCM_OFF = DCM_OFF,
	AUDIO_DCM_ON = DCM_ON,
};

#define AUDIO_DCM_EN_MASK	(0x1 << 29)
#define AUDIO_DCM_EN_ON		(0x1 << 29)
#define AUDIO_DCM_EN_OFF	(0x0 << 29)

int dcm_audio(enum ENUM_AUDIO_DCM on)
{
	if (on == AUDIO_DCM_ON) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~AUDIO_DCM_EN_MASK) |
			AUDIO_DCM_EN_ON);
	} else if (on == AUDIO_DCM_OFF) {
		reg_write(PERIBUS_DCMCTL,
			(reg_read(PERIBUS_DCMCTL) &
			~AUDIO_DCM_EN_MASK) |
			AUDIO_DCM_EN_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

enum ENUM_EMI_DCM {
	EMI_DCM_OFF = DCM_OFF,
	EMI_DCM_ON = DCM_ON,
};

#define EMI_CONH_DCM_MASK	((0xff << 24))
#define EMI_CONH_DCM_ON		((0x0 << 24))
#define EMI_CONH_DCM_OFF	((0xff << 24))
#define EMI_CONM_DCM_MASK	((0xff << 24))
#define EMI_CONM_DCM_ON		((0x0 << 24))
#define EMI_CONM_DCM_OFF	((0xff << 24))
#define EMI_CONB_DCM_MASK	((0xff << 24))
#define EMI_CONB_DCM_ON		((0x0 << 24))
#define EMI_CONB_DCM_OFF	((0xff << 24))

int dcm_emi(enum ENUM_EMI_DCM on)
{
	if (on == EMI_DCM_ON) {
		reg_write(EMI_CONH,
			(reg_read(EMI_CONH) &
			~EMI_CONH_DCM_MASK) |
			EMI_CONH_DCM_ON);
		reg_write(EMI_CONM,
			(reg_read(EMI_CONM) &
			~EMI_CONM_DCM_MASK) |
			EMI_CONM_DCM_ON);
		reg_write(EMI_CONB,
			(reg_read(EMI_CONB) &
			~EMI_CONB_DCM_MASK) |
			EMI_CONB_DCM_ON);
	} else if (on == EMI_DCM_OFF) {
		reg_write(EMI_CONH,
			(reg_read(EMI_CONH) &
			~EMI_CONH_DCM_MASK) |
			EMI_CONH_DCM_OFF);
		reg_write(EMI_CONM,
			(reg_read(EMI_CONM) &
			~EMI_CONM_DCM_MASK) |
			EMI_CONM_DCM_OFF);
		reg_write(EMI_CONB,
			(reg_read(EMI_CONB) &
			~EMI_CONB_DCM_MASK) |
			EMI_CONB_DCM_OFF);
	} else {
		dcm_err("%s(): unkwon value\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/*****************************************************/

enum {
	ARMCORE_DCM_TYPE = (1U << 0),
	MCUSYS_DCM_TYPE = (1U << 1),
	INFRA_DCM_TYPE = (1U << 2),
	PERI_DCM_TYPE = (1U << 3),
	EMI_DCM_TYPE = (1U << 4),
	PMIC_DCM_TYPE = (1U << 5),
	USB_DCM_TYPE = (1U << 6),
	NR_DCM_TYPE = 7,
};

#define ALL_DCM_TYPE  (ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | \
			INFRA_DCM_TYPE | PERI_DCM_TYPE |  \
			EMI_DCM_TYPE | \
			PMIC_DCM_TYPE | USB_DCM_TYPE)

struct _dcm {
	int current_state;
	int saved_state;
	int disable_refcnt;
	int default_state;
	DCM_FUNC func;
	int typeid;
	char *name;
};

static struct _dcm dcm_array[NR_DCM_TYPE] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .current_state = ARMCORE_DCM_MODE1,
	 .default_state = ARMCORE_DCM_MODE1,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .current_state = MCUSYS_DCM_ON,
	 .default_state = MCUSYS_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .current_state = PERI_DCM_ON,
	 .default_state = PERI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .current_state = EMI_DCM_ON,
	 .default_state = EMI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PMIC_DCM_TYPE,
	 .name = "PMIC_DCM",
	 .func = (DCM_FUNC) dcm_pmic,
	 .current_state = PMIC_DCM_ON,
	 .default_state = PMIC_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = USB_DCM_TYPE,
	 .name = "USB_DCM",
	 .func = (DCM_FUNC) dcm_usb,
	 .current_state = USB_DCM_ON,
	 .default_state = USB_DCM_ON,
	 .disable_refcnt = 0,
	 },
};

/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/power/dcm_state interface:
 *     'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/

void dcm_set_default(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	/* dcm_info("[%s]type:0x%08x\n", __func__, type); */

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
		dcm->saved_state = dcm->current_state = dcm->default_state;
		dcm->disable_refcnt = 0;
		dcm->func(dcm->current_state);

		/* dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
		 * dcm->name, dcm->typeid, dcm->current_state,
		 * dcm->disable_refcnt);
		 */
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	int i;
	struct _dcm *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x, set:%d\n", __func__, type, state);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->saved_state = state;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid,
				dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}


void dcm_disable(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->current_state = DCM_OFF;
			dcm->disable_refcnt++;
			dcm->func(dcm->current_state);

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid,
				dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);

}

void dcm_restore(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0];
		type && (i < NR_DCM_TYPE); i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->disable_refcnt > 0)
				dcm->disable_refcnt--;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = dcm->saved_state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid,
				dcm->current_state, dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}


void dcm_dump_state(int type)
{
	int i;
	struct _dcm *dcm;

	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm_info("[%-16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid,
				dcm->current_state, dcm->disable_refcnt);
		}
	}
}

#define REG_DUMP(addr) { dcm_info("%-30s(0x%p): 0x%08X\n", \
			#addr, addr, reg_read(addr)); }

void dcm_dump_regs(void)
{
	if (dcm_initiated == 0) {
		dcm_err("DCM driver is not initialized\n");
		return;
	}

	dcm_info("\n******** dcm dump register *********\n");
	/* MCUSYS */
	REG_DUMP(MP0_RGU_DCM_CONFIG);
	REG_DUMP(EMI_WFIFO);
	REG_DUMP(ACLKEN_DIV);
	REG_DUMP(L2C_SRAM_CTRL);
	REG_DUMP(CCI_STATUS);
	REG_DUMP(MCUSYS_BUS_FABRIC_DCM_CTRL);
	REG_DUMP(MCU_MISC_DCM_CTRL);
	REG_DUMP(MP_CCI_ADB400_DCM_CONFIG);
	REG_DUMP(GIC_SYNC_DCM);
	REG_DUMP(BUS_PLL_DIVIDER_CFG);

	/*INFRA,PERI,EMI,PMIC,USB,AUDIO */
	REG_DUMP(INFRABUS_DCMCTL);
	REG_DUMP(PERIBUS_DCMCTL);
	REG_DUMP(P2P_RX_CLK_ON);
	REG_DUMP(EMI_CONH);
	REG_DUMP(EMI_CONM);
	REG_DUMP(EMI_CONB);
}

#if defined(CONFIG_PM)
static ssize_t dcm_state_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int len = 0;
	int i;
	struct _dcm *dcm;

	/* dcm_dump_state(ALL_DCM_TYPE); */
	len = snprintf(buf, PAGE_SIZE, "\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		len += snprintf(buf+len, PAGE_SIZE-len,
				"[%-16s 0x%08x] current state:%d (%d)\n",
				dcm->name, dcm->typeid,
				dcm->current_state, dcm->disable_refcnt);
	}

	len += snprintf(buf+len, PAGE_SIZE-len,
			"\n********** dcm_state help *********\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"echo set [mask] [mode] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"echo disable [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"echo restore [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"echo dump [mask] > /sys/power/dcm_state\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mask] is hexl bit mask of dcm;\n");
	len += snprintf(buf+len, PAGE_SIZE-len,
			"***** [mode] is type of DCM to set and retained\n");

	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t n)
{
	char cmd[16];
	unsigned int mask;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= ALL_DCM_TYPE;

		if (!strcmp(cmd, "restore")) {
			/* dcm_dump_regs(); */
			dcm_restore(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "disable")) {
			/* dcm_dump_regs(); */
			dcm_disable(mask);
			/* dcm_dump_regs(); */
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_state(mask);
			dcm_dump_regs();
		} else if (!strcmp(cmd, "set")) {
			int mode;

			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= ALL_DCM_TYPE;

				dcm_set_state(mask, mode);
			}
		} else {
			dcm_info("Do not support your command: %s\n", cmd);
			return -EINVAL;
		}
		return n;
	}

	dcm_info("SORRY, do not support your command.\n");

	return -EINVAL;
}

static struct kobj_attribute dcm_state_attr = {
	.attr = {
		 .name = "dcm_state",
		 .mode = 0644,
		 },
	.show = dcm_state_show,
	.store = dcm_state_store,
};


#endif /*#if defined (CONFIG_PM) */


#if defined(CONFIG_OF)
static int mt_dcm_dts_map(void)
{
	struct device_node *node;
	struct resource r;

	/* topckgen */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_NODE);
	if (!node) {
		dcm_info("info:cannot find topckgen node " TOPCKGEN_NODE);
		WARN_ON(1);
		goto fail;
	}
	topckctrl_base = of_iomap(node, 0);
	if (!topckctrl_base) {
		dcm_info("info:cannot iomap topckgen " TOPCKGEN_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		dcm_info("info:cannot find mcucfg node " MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}
	if (of_address_to_resource(node, 0, &r)) {
		dcm_info("info:cannot get phys addr" MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}
	mcucfg_phys_base = r.start;

	mcucfg_base = of_iomap(node, 0);
	if (!mcucfg_base) {
		dcm_info("info: cannot iomap mcucfg " MCUCFG_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* emi_reg */
	node = of_find_compatible_node(NULL, NULL, EMI_REG_NODE);
	if (!node) {
		dcm_info("info: cannot find emi node " EMI_REG_NODE);
		WARN_ON(1);
		goto fail;
	}
	emi_reg_base = of_iomap(node, 0);
	if (!emi_reg_base) {
		dcm_info("info: cannot iomap emi " EMI_REG_NODE);
		WARN_ON(1);
		goto fail;
	}
	emi_chn_reg_base = of_iomap(node, 2);
	if (!emi_chn_reg_base) {
		dcm_info("info: cannot iomap emi " EMI_REG_NODE);
		WARN_ON(1);
		goto fail;
	}

	/* infracfg */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_NODE);
	if (!node) {
		dcm_info("info: cannot find infracfg node " INFRACFG_NODE);
		WARN_ON(1);
		goto fail;
	}
	infracfg_base = of_iomap(node, 0);
	if (!infracfg_base) {
		dcm_info("info: cannot iomap infracfg " INFRACFG_NODE);
		WARN_ON(1);
		goto fail;
	}

	dcm_initiated = 1;

	return 0;
fail:
	return -1;
}
#else
static int mt_dcm_dts_map(void)
{
	return 0;
}
#endif


int mt_dcm_init(void)
{
	int ret, err = 0;

	ret = mt_dcm_dts_map();
	if (ret != 0) {
		dcm_err("check dcm dts map\n");
		return 0;
	}

#ifndef DCM_DEFAULT_ALL_OFF
	/* enable all dcm */
	dcm_set_default(ALL_DCM_TYPE);
#else
	dcm_set_state(ALL_DCM_TYPE, DCM_OFF);
#endif

	err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);
	if (err)
		dcm_err("[%s]: fail to create sysfs\n", __func__);

	return 0;
}
late_initcall(mt_dcm_init);


/**** public APIs *****/
void mt_dcm_disable(void)
{
	dcm_disable(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_disable);

void mt_dcm_restore(void)
{
	dcm_restore(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_restore);

