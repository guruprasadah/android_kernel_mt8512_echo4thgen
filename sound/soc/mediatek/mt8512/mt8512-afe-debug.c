/*
 * mt8512-afe-debug.c  --  Mediatek 8512 audio debugfs
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Mengge Wang <mengge.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt8512-afe-debug.h"
#include "mt8512-reg.h"
#include "mt8512-afe-utils.h"
#include "mt8512-afe-common.h"
#include "../common/mtk-base-afe.h"
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>


#ifdef CONFIG_DEBUG_FS

struct mt8512_afe_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
};

struct afe_dump_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct afe_dump_reg_attr etdm_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(AUDIO_TOP_CON5),
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(ETDM_IN1_CON0),
	DUMP_REG_ENTRY(ETDM_IN1_CON1),
	DUMP_REG_ENTRY(ETDM_IN1_CON2),
	DUMP_REG_ENTRY(ETDM_IN1_CON3),
	DUMP_REG_ENTRY(ETDM_IN1_CON4),
	DUMP_REG_ENTRY(ETDM_IN2_CON0),
	DUMP_REG_ENTRY(ETDM_IN2_CON1),
	DUMP_REG_ENTRY(ETDM_IN2_CON2),
	DUMP_REG_ENTRY(ETDM_IN2_CON3),
	DUMP_REG_ENTRY(ETDM_IN2_CON4),
	DUMP_REG_ENTRY(ETDM_OUT2_CON0),
	DUMP_REG_ENTRY(ETDM_OUT2_CON1),
	DUMP_REG_ENTRY(ETDM_OUT2_CON2),
	DUMP_REG_ENTRY(ETDM_OUT2_CON3),
	DUMP_REG_ENTRY(ETDM_OUT2_CON4),
	DUMP_REG_ENTRY(ETDM_COWORK_CON0),
	DUMP_REG_ENTRY(ETDM_COWORK_CON1),
	DUMP_REG_ENTRY(ETDM_COWORK_CON3),
};

static const struct afe_dump_reg_attr memif_dump_regs[] = {
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AUDIO_TOP_CON5),
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_DAC_CON1),
	DUMP_REG_ENTRY(AFE_DL2_BASE),
	DUMP_REG_ENTRY(AFE_DL2_CUR),
	DUMP_REG_ENTRY(AFE_DL2_END),
	DUMP_REG_ENTRY(AFE_DL2_CON0),
	DUMP_REG_ENTRY(AFE_DL3_BASE),
	DUMP_REG_ENTRY(AFE_DL3_CUR),
	DUMP_REG_ENTRY(AFE_DL3_END),
	DUMP_REG_ENTRY(AFE_DL3_CON0),
	DUMP_REG_ENTRY(AFE_DL6_BASE),
	DUMP_REG_ENTRY(AFE_DL6_CUR),
	DUMP_REG_ENTRY(AFE_DL6_END),
	DUMP_REG_ENTRY(AFE_DL6_CON0),
	DUMP_REG_ENTRY(AFE_DL10_BASE),
	DUMP_REG_ENTRY(AFE_DL10_CUR),
	DUMP_REG_ENTRY(AFE_DL10_END),
	DUMP_REG_ENTRY(AFE_DL10_CON0),
	DUMP_REG_ENTRY(AFE_UL1_BASE),
	DUMP_REG_ENTRY(AFE_UL1_CUR),
	DUMP_REG_ENTRY(AFE_UL1_END),
	DUMP_REG_ENTRY(AFE_UL1_CON0),
	DUMP_REG_ENTRY(AFE_UL2_BASE),
	DUMP_REG_ENTRY(AFE_UL2_CUR),
	DUMP_REG_ENTRY(AFE_UL2_END),
	DUMP_REG_ENTRY(AFE_UL2_CON0),
	DUMP_REG_ENTRY(AFE_UL3_BASE),
	DUMP_REG_ENTRY(AFE_UL3_CUR),
	DUMP_REG_ENTRY(AFE_UL3_END),
	DUMP_REG_ENTRY(AFE_UL3_CON0),
	DUMP_REG_ENTRY(AFE_UL4_BASE),
	DUMP_REG_ENTRY(AFE_UL4_CUR),
	DUMP_REG_ENTRY(AFE_UL4_END),
	DUMP_REG_ENTRY(AFE_UL4_CON0),
	DUMP_REG_ENTRY(AFE_UL5_BASE),
	DUMP_REG_ENTRY(AFE_UL5_CUR),
	DUMP_REG_ENTRY(AFE_UL5_END),
	DUMP_REG_ENTRY(AFE_UL5_CON0),
	DUMP_REG_ENTRY(AFE_UL8_BASE),
	DUMP_REG_ENTRY(AFE_UL8_CUR),
	DUMP_REG_ENTRY(AFE_UL8_END),
	DUMP_REG_ENTRY(AFE_UL8_CON0),
	DUMP_REG_ENTRY(AFE_UL9_BASE),
	DUMP_REG_ENTRY(AFE_UL9_CUR),
	DUMP_REG_ENTRY(AFE_UL9_END),
	DUMP_REG_ENTRY(AFE_UL9_CON0),
	DUMP_REG_ENTRY(AFE_UL10_BASE),
	DUMP_REG_ENTRY(AFE_UL10_CUR),
	DUMP_REG_ENTRY(AFE_UL10_END),
	DUMP_REG_ENTRY(AFE_UL10_CON0),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON0),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON1),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON2),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON3),
	DUMP_REG_ENTRY(AFE_I2S_UL9_REORDER),
	DUMP_REG_ENTRY(AFE_I2S_UL2_REORDER),
};

static const struct afe_dump_reg_attr irq_dump_regs[] = {
	DUMP_REG_ENTRY(ASYS_TOP_CON),
	DUMP_REG_ENTRY(AFE_IRQ1_CON),
	DUMP_REG_ENTRY(AFE_IRQ2_CON),
	DUMP_REG_ENTRY(AFE_IRQ3_CON),
	DUMP_REG_ENTRY(AFE_IRQ4_CON),
	DUMP_REG_ENTRY(AFE_IRQ5_CON),
	DUMP_REG_ENTRY(AFE_IRQ6_CON),
	DUMP_REG_ENTRY(AFE_IRQ7_CON),
	DUMP_REG_ENTRY(AFE_TSF_CON1),
	DUMP_REG_ENTRY(ASYS_IRQ1_CON),
	DUMP_REG_ENTRY(ASYS_IRQ2_CON),
	DUMP_REG_ENTRY(ASYS_IRQ3_CON),
	DUMP_REG_ENTRY(ASYS_IRQ4_CON),
	DUMP_REG_ENTRY(ASYS_IRQ5_CON),
	DUMP_REG_ENTRY(ASYS_IRQ6_CON),
	DUMP_REG_ENTRY(ASYS_IRQ7_CON),
	DUMP_REG_ENTRY(ASYS_IRQ8_CON),
	DUMP_REG_ENTRY(ASYS_IRQ9_CON),
	DUMP_REG_ENTRY(ASYS_IRQ10_CON),
	DUMP_REG_ENTRY(ASYS_IRQ11_CON),
	DUMP_REG_ENTRY(ASYS_IRQ12_CON),
	DUMP_REG_ENTRY(ASYS_IRQ13_CON),
	DUMP_REG_ENTRY(ASYS_IRQ14_CON),
	DUMP_REG_ENTRY(ASYS_IRQ15_CON),
	DUMP_REG_ENTRY(ASYS_IRQ16_CON),
	DUMP_REG_ENTRY(ASYS_IRQ_CLR),
	DUMP_REG_ENTRY(ASYS_IRQ_STATUS),
	DUMP_REG_ENTRY(AFE_IRQ_MCU_CLR),
	DUMP_REG_ENTRY(AFE_IRQ_STATUS),
	DUMP_REG_ENTRY(AFE_IRQ_MASK),
	DUMP_REG_ENTRY(ASYS_IRQ_MASK),
};

static const struct afe_dump_reg_attr conn_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_CONN0),
	DUMP_REG_ENTRY(AFE_CONN1),
	DUMP_REG_ENTRY(AFE_CONN2),
	DUMP_REG_ENTRY(AFE_CONN3),
	DUMP_REG_ENTRY(AFE_CONN4),
	DUMP_REG_ENTRY(AFE_CONN5),
	DUMP_REG_ENTRY(AFE_CONN6),
	DUMP_REG_ENTRY(AFE_CONN7),
	DUMP_REG_ENTRY(AFE_CONN8),
	DUMP_REG_ENTRY(AFE_CONN9),
	DUMP_REG_ENTRY(AFE_CONN10),
	DUMP_REG_ENTRY(AFE_CONN11),
	DUMP_REG_ENTRY(AFE_CONN12),
	DUMP_REG_ENTRY(AFE_CONN13),
	DUMP_REG_ENTRY(AFE_CONN14),
	DUMP_REG_ENTRY(AFE_CONN15),
	DUMP_REG_ENTRY(AFE_CONN16),
	DUMP_REG_ENTRY(AFE_CONN17),
	DUMP_REG_ENTRY(AFE_CONN18),
	DUMP_REG_ENTRY(AFE_CONN19),
	DUMP_REG_ENTRY(AFE_CONN20),
	DUMP_REG_ENTRY(AFE_CONN21),
	DUMP_REG_ENTRY(AFE_CONN26),
	DUMP_REG_ENTRY(AFE_CONN27),
	DUMP_REG_ENTRY(AFE_CONN28),
	DUMP_REG_ENTRY(AFE_CONN29),
	DUMP_REG_ENTRY(AFE_CONN30),
	DUMP_REG_ENTRY(AFE_CONN31),
	DUMP_REG_ENTRY(AFE_CONN32),
	DUMP_REG_ENTRY(AFE_CONN33),
	DUMP_REG_ENTRY(AFE_CONN34),
	DUMP_REG_ENTRY(AFE_CONN35),
	DUMP_REG_ENTRY(AFE_CONN36),
	DUMP_REG_ENTRY(AFE_CONN37),
	DUMP_REG_ENTRY(AFE_CONN38),
	DUMP_REG_ENTRY(AFE_CONN39),
	DUMP_REG_ENTRY(AFE_CONN40),
	DUMP_REG_ENTRY(AFE_CONN41),
	DUMP_REG_ENTRY(AFE_CONN42),
	DUMP_REG_ENTRY(AFE_CONN43),
	DUMP_REG_ENTRY(AFE_CONN44),
	DUMP_REG_ENTRY(AFE_CONN45),
	DUMP_REG_ENTRY(AFE_CONN46),
	DUMP_REG_ENTRY(AFE_CONN47),
	DUMP_REG_ENTRY(AFE_CONN48),
	DUMP_REG_ENTRY(AFE_CONN49),
	DUMP_REG_ENTRY(AFE_CONN0_1),
	DUMP_REG_ENTRY(AFE_CONN1_1),
	DUMP_REG_ENTRY(AFE_CONN2_1),
	DUMP_REG_ENTRY(AFE_CONN3_1),
	DUMP_REG_ENTRY(AFE_CONN4_1),
	DUMP_REG_ENTRY(AFE_CONN5_1),
	DUMP_REG_ENTRY(AFE_CONN6_1),
	DUMP_REG_ENTRY(AFE_CONN7_1),
	DUMP_REG_ENTRY(AFE_CONN8_1),
	DUMP_REG_ENTRY(AFE_CONN9_1),
	DUMP_REG_ENTRY(AFE_CONN10_1),
	DUMP_REG_ENTRY(AFE_CONN11_1),
	DUMP_REG_ENTRY(AFE_CONN12_1),
	DUMP_REG_ENTRY(AFE_CONN13_1),
	DUMP_REG_ENTRY(AFE_CONN14_1),
	DUMP_REG_ENTRY(AFE_CONN15_1),
	DUMP_REG_ENTRY(AFE_CONN16_1),
	DUMP_REG_ENTRY(AFE_CONN17_1),
	DUMP_REG_ENTRY(AFE_CONN18_1),
	DUMP_REG_ENTRY(AFE_CONN19_1),
	DUMP_REG_ENTRY(AFE_CONN20_1),
	DUMP_REG_ENTRY(AFE_CONN21_1),
	DUMP_REG_ENTRY(AFE_CONN26_1),
	DUMP_REG_ENTRY(AFE_CONN27_1),
	DUMP_REG_ENTRY(AFE_CONN28_1),
	DUMP_REG_ENTRY(AFE_CONN29_1),
	DUMP_REG_ENTRY(AFE_CONN30_1),
	DUMP_REG_ENTRY(AFE_CONN31_1),
	DUMP_REG_ENTRY(AFE_CONN32_1),
	DUMP_REG_ENTRY(AFE_CONN33_1),
	DUMP_REG_ENTRY(AFE_CONN34_1),
	DUMP_REG_ENTRY(AFE_CONN35_1),
	DUMP_REG_ENTRY(AFE_CONN36_1),
	DUMP_REG_ENTRY(AFE_CONN37_1),
	DUMP_REG_ENTRY(AFE_CONN38_1),
	DUMP_REG_ENTRY(AFE_CONN39_1),
	DUMP_REG_ENTRY(AFE_CONN40_1),
	DUMP_REG_ENTRY(AFE_CONN41_1),
	DUMP_REG_ENTRY(AFE_CONN42_1),
	DUMP_REG_ENTRY(AFE_CONN43_1),
	DUMP_REG_ENTRY(AFE_CONN44_1),
	DUMP_REG_ENTRY(AFE_CONN45_1),
	DUMP_REG_ENTRY(AFE_CONN46_1),
	DUMP_REG_ENTRY(AFE_CONN47_1),
	DUMP_REG_ENTRY(AFE_CONN48_1),
	DUMP_REG_ENTRY(AFE_CONN49_1),
	DUMP_REG_ENTRY(AFE_CONN_16BIT),
	DUMP_REG_ENTRY(AFE_CONN_24BIT),
	DUMP_REG_ENTRY(AFE_CONN_16BIT_1),
	DUMP_REG_ENTRY(AFE_CONN_24BIT_1),
	DUMP_REG_ENTRY(AFE_CONN_RS),
	DUMP_REG_ENTRY(AFE_CONN_RS_1),
};

static const struct afe_dump_reg_attr adda_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON3),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(ASMO_TIMING_CON0),
	DUMP_REG_ENTRY(PWR2_TOP_CON0),
	DUMP_REG_ENTRY(PWR2_TOP_CON1),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC0_IIR_COEF_02_01),
	DUMP_REG_ENTRY(AFE_DMIC0_IIR_COEF_04_03),
	DUMP_REG_ENTRY(AFE_DMIC0_IIR_COEF_06_05),
	DUMP_REG_ENTRY(AFE_DMIC0_IIR_COEF_08_07),
	DUMP_REG_ENTRY(AFE_DMIC0_IIR_COEF_10_09),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC1_IIR_COEF_02_01),
	DUMP_REG_ENTRY(AFE_DMIC1_IIR_COEF_04_03),
	DUMP_REG_ENTRY(AFE_DMIC1_IIR_COEF_06_05),
	DUMP_REG_ENTRY(AFE_DMIC1_IIR_COEF_08_07),
	DUMP_REG_ENTRY(AFE_DMIC1_IIR_COEF_10_09),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC2_IIR_COEF_02_01),
	DUMP_REG_ENTRY(AFE_DMIC2_IIR_COEF_04_03),
	DUMP_REG_ENTRY(AFE_DMIC2_IIR_COEF_06_05),
	DUMP_REG_ENTRY(AFE_DMIC2_IIR_COEF_08_07),
	DUMP_REG_ENTRY(AFE_DMIC2_IIR_COEF_10_09),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC3_IIR_COEF_02_01),
	DUMP_REG_ENTRY(AFE_DMIC3_IIR_COEF_04_03),
	DUMP_REG_ENTRY(AFE_DMIC3_IIR_COEF_06_05),
	DUMP_REG_ENTRY(AFE_DMIC3_IIR_COEF_08_07),
	DUMP_REG_ENTRY(AFE_DMIC3_IIR_COEF_10_09),
};

static const struct afe_dump_reg_attr gasrc_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(ASMO_TIMING_CON0),
	DUMP_REG_ENTRY(PWR1_ASM_CON1),
	DUMP_REG_ENTRY(GASRC_CFG0),
	DUMP_REG_ENTRY(GASRC_TIMING_CON0),
	DUMP_REG_ENTRY(GASRC_TIMING_CON1),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON14),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON13),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON14),
};

static const struct afe_dump_reg_attr spdif_dump_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(AFE_SPDIF_OUT_CON0),
	DUMP_REG_ENTRY(AFE_IEC_CFG),
	DUMP_REG_ENTRY(AFE_IEC_NSNUM),
	DUMP_REG_ENTRY(AFE_IEC_BURST_INFO),
	DUMP_REG_ENTRY(AFE_IEC_BURST_LEN),
	DUMP_REG_ENTRY(AFE_IEC_NSADR),
	DUMP_REG_ENTRY(AFE_IEC_CHL_STAT0),
	DUMP_REG_ENTRY(AFE_IEC_CHL_STAT1),
	DUMP_REG_ENTRY(AFE_IEC_CHR_STAT0),
	DUMP_REG_ENTRY(AFE_IEC_CHR_STAT1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CFG0),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CFG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS2),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS3),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS4),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS5),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CHSTS6),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG2),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG3),
	DUMP_REG_ENTRY(AFE_SPDIFIN_DEBUG4),
	DUMP_REG_ENTRY(AFE_SPDIFIN_EC),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CKLOCK_CFG),
	DUMP_REG_ENTRY(AFE_SPDIFIN_BR),
	DUMP_REG_ENTRY(AFE_SPDIFIN_BR_DBG1),
	DUMP_REG_ENTRY(AFE_SPDIFIN_CKFBDIV),
	DUMP_REG_ENTRY(AFE_SPDIFIN_INT_EXT),
	DUMP_REG_ENTRY(AFE_SPDIFIN_INT_EXT2),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO_2),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_INFO_3),
	DUMP_REG_ENTRY(SPDIFIN_FREQ_STATUS),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON0),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON1),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_MON),
};

static const struct afe_dump_reg_attr dbg_dump_regs[] = {
	DUMP_REG_ENTRY(AFE_SINEGEN_CON0),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON1),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON2),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON3),
	DUMP_REG_ENTRY(AFE_BUS_MON1),
	DUMP_REG_ENTRY(ETDM_IN1_MONITOR),
	DUMP_REG_ENTRY(ETDM_IN2_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT2_MONITOR),
	DUMP_REG_ENTRY(PCM_INTF_CON1),
	DUMP_REG_ENTRY(PCM_INTF_CON2),
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG0),
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG1),
};

static ssize_t mt8512_afe_dump_registers(char __user *user_buf,
					 size_t count,
					 loff_t *pos,
					 struct mtk_base_afe *afe,
					 const struct afe_dump_reg_attr *regs,
					 size_t regs_len)
{
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value;
	int n = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mt8512_afe_enable_main_clk(afe);

	for (i = 0; i < regs_len; i++) {
		if (regmap_read(afe->regmap, regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s = N/A\n",
				       regs[i].name);
		else
			n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
				       regs[i].name, reg_value);
	}

	mt8512_afe_disable_main_clk(afe);

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);
	kfree(buf);

	return ret;
}

static ssize_t mt8512_afe_etdm_read_file(struct file *file,
				    char __user *user_buf,
				    size_t count,
				    loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					etdm_dump_regs,
					ARRAY_SIZE(etdm_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count,
				     loff_t *pos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	const char delim[] = " ,";
	unsigned long reg, value;
	struct mtk_base_afe *afe = file->private_data;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	mt8512_afe_enable_main_clk(afe);

	regmap_write(afe->regmap, reg, value);

	mt8512_afe_disable_main_clk(afe);

	return buf_size;
}

static ssize_t mt8512_afe_memif_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					memif_dump_regs,
					ARRAY_SIZE(memif_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_irq_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					irq_dump_regs,
					ARRAY_SIZE(irq_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_conn_read_file(struct file *file,
					 char __user *user_buf,
					 size_t count,
					 loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					conn_dump_regs,
					ARRAY_SIZE(conn_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_adda_read_file(struct file *file,
					 char __user *user_buf,
					 size_t count,
					 loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					adda_dump_regs,
					ARRAY_SIZE(adda_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_gasrc_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					gasrc_dump_regs,
					ARRAY_SIZE(gasrc_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_spdif_read_file(struct file *file,
					  char __user *user_buf,
					  size_t count,
					  loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					spdif_dump_regs,
					ARRAY_SIZE(spdif_dump_regs));

	return ret;
}

static ssize_t mt8512_afe_dbg_read_file(struct file *file,
					char __user *user_buf,
					size_t count,
					loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	ssize_t ret;

	if (*pos < 0 || !count)
		return -EINVAL;

	ret = mt8512_afe_dump_registers(user_buf, count, pos, afe,
					dbg_dump_regs,
					ARRAY_SIZE(dbg_dump_regs));

	return ret;
}

static const struct file_operations mt8512_afe_etdm_fops = {
	.open = simple_open,
	.read = mt8512_afe_etdm_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_memif_fops = {
	.open = simple_open,
	.read = mt8512_afe_memif_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_irq_fops = {
	.open = simple_open,
	.read = mt8512_afe_irq_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_conn_fops = {
	.open = simple_open,
	.read = mt8512_afe_conn_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_adda_fops = {
	.open = simple_open,
	.read = mt8512_afe_adda_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_gasrc_fops = {
	.open = simple_open,
	.read = mt8512_afe_gasrc_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_spdif_fops = {
	.open = simple_open,
	.read = mt8512_afe_spdif_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const struct file_operations mt8512_afe_dbg_fops = {
	.open = simple_open,
	.read = mt8512_afe_dbg_read_file,
	.write = mt8512_afe_write_file,
	.llseek = default_llseek,
};

static const
struct mt8512_afe_debug_fs afe_debug_fs[MT8512_AFE_DEBUGFS_NUM] = {
	{"mtksocaudioetdm", &mt8512_afe_etdm_fops},
	{"mtksocaudiomemif", &mt8512_afe_memif_fops},
	{"mtksocaudioirq", &mt8512_afe_irq_fops},
	{"mtksocaudioconn", &mt8512_afe_conn_fops},
	{"mtksocaudioadda", &mt8512_afe_adda_fops},
	{"mtksocaudiogasrc", &mt8512_afe_gasrc_fops},
	{"mtksocaudiospdif", &mt8512_afe_spdif_fops},
	{"mtksocaudiodbg", &mt8512_afe_dbg_fops},
};

#endif

void mt8512_afe_init_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		afe_priv->debugfs_dentry[i] =
			debugfs_create_file(afe_debug_fs[i].fs_name,
				0644, NULL, afe, afe_debug_fs[i].fops);
		if (!afe_priv->debugfs_dentry[i])
			dev_info(afe->dev, "%s create %s debugfs failed\n",
				 __func__, afe_debug_fs[i].fs_name);
	}
#endif
}

void mt8512_afe_cleanup_debugfs(struct mtk_base_afe *afe)
{
#ifdef CONFIG_DEBUG_FS
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int i;

	if (!afe_priv)
		return;

	for (i = 0; i < MT8512_AFE_DEBUGFS_NUM; i++)
		debugfs_remove(afe_priv->debugfs_dentry[i]);
#endif
}
