/*
 * mt8512-codec.h  --  Mediatek 8512 ALSA SoC Codec driver
 *
 * Copyright (c) 2019 MediaTek Inc.
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

#ifndef __MT8512_CODEC_H__
#define __MT8512_CODEC_H__

#define AFE_OFFSET			(0xA0000000)
#define APMIXED_OFFSET			(0x0B000000)

#define AFE_REG(reg)			(reg | AFE_OFFSET)
#define APMIXED_REG(reg)		(reg | APMIXED_OFFSET)
#define ADC_RC_TRIM_VALUE_MAX           (64)
#define ADC_RC_CAL_BEST_VOLT            (900)


/* audio_top_sys */
#define AUDIO_TOP_CON0			AFE_REG(0x0000)
#define AFE_ADDA_DL_SRC2_CON0		AFE_REG(0x2d08)
#define AFE_ADDA_DL_SRC2_CON1		AFE_REG(0x2d0C)
#define AFE_ADDA_TOP_CON0		AFE_REG(0x2d20)
#define AFE_ADDA_UL_DL_CON0		AFE_REG(0x2d24)
#define AFE_ADDA_PREDIS_CON0		AFE_REG(0x2d40)
#define AFE_ADDA_PREDIS_CON1		AFE_REG(0x2d44)
#define AFE_NLE_CFG			AFE_REG(0x2d50)
#define AFE_NLE_PRE_BUF_CFG		AFE_REG(0x2d54)
#define AFE_NLE_PWR_DET_LCH_CFG		AFE_REG(0x2d58)
#define AFE_NLE_ZCD_LCH_CFG		AFE_REG(0x2d5c)
#define AFE_NLE_GAIN_ADJ_LCH_CFG0	AFE_REG(0x2d60)
#define AFE_NLE_GAIN_IMP_LCH_CFG0	AFE_REG(0x2d6c)
#define AFE_NLE_PWE_DET_LCH_MON		AFE_REG(0x2d78)
#define AFE_NLE_GAIN_ADJ_LCH_MON0	AFE_REG(0x2d7c)
#define AFE_NLE_LCH_MON0		AFE_REG(0x2d84)
#define AFE_NLE_PWR_DET_RCH_CFG		AFE_REG(0x2d90)
#define AFE_NLE_ZCD_RCH_CFG		AFE_REG(0x2d94)
#define AFE_NLE_GAIN_ADJ_RCH_CFG0	AFE_REG(0x2d98)
#define AFE_NLE_GAIN_IMP_RCH_CFG0	AFE_REG(0x2da4)
#define AFE_NLE_PWE_DET_RCH_MON		AFE_REG(0x2db0)
#define AFE_NLE_GAIN_ADJ_RCH_MON0	AFE_REG(0x2db4)
#define AFE_NLE_RCH_MON0		AFE_REG(0x2dbc)
#define ABB_AFE_CON0			AFE_REG(0x2e00)
#define ABB_AFE_CON1			AFE_REG(0x2e04)
#define ABB_AFE_CON2			AFE_REG(0x2e08)
#define ABB_AFE_CON3			AFE_REG(0x2e0c)
#define ABB_AFE_CON4			AFE_REG(0x2e10)
#define ABB_AFE_CON5			AFE_REG(0x2e14)
#define ABB_AFE_CON6			AFE_REG(0x2e18)
#define ABB_AFE_CON7			AFE_REG(0x2e1c)
#define ABB_AFE_CON8			AFE_REG(0x2e20)
#define ABB_AFE_CON10			AFE_REG(0x2e28)
#define ABB_AFE_CON11			AFE_REG(0x2e2c)
#define ABB_AFE_SDM_TEST		AFE_REG(0x2e4c)
#define AFE_AD_UL_DL_CON0		AFE_REG(0x2f00)
#define ABB_ULAFE_CON0			AFE_REG(0x2f4c)
#define ABB_ULAFE_CON1			AFE_REG(0x2f50)
#define AMIC_GAIN_CON0			AFE_REG(0x2e50)
#define AMIC_GAIN_CON1			AFE_REG(0x2e54)
#define AMIC_GAIN_CON2			AFE_REG(0x2e58)
#define AMIC_GAIN_CON3			AFE_REG(0x2e5c)
#define AMIC_GAIN_CUR			AFE_REG(0x2e60)


/* apmixedsys */
#define AP_TENSE_CON00		APMIXED_REG(0x0600)
#define AUDDL_CON00		APMIXED_REG(0x0700)
#define AUDDL_CON01		APMIXED_REG(0x0704)
#define AUDDL_CON02		APMIXED_REG(0x0708)
#define AUDDL_CON03		APMIXED_REG(0x070C)
#define AUDDL_CON18		APMIXED_REG(0x0760)

#define AUDUL_CON00		APMIXED_REG(0x0710)
#define AUDUL_CON01		APMIXED_REG(0x0714)
#define AUDUL_CON04		APMIXED_REG(0x0720)
#define AUDUL_CON05		APMIXED_REG(0x0724)
#define AUDUL_CON09		APMIXED_REG(0x0734)
#define AUDUL_CON12             APMIXED_REG(0x0748)
#define AUDUL_CON19		APMIXED_REG(0x0764)
#define AUDUL_CON20		APMIXED_REG(0x0768)
#define AUDUL_CON21		APMIXED_REG(0x076C)
#define AUDUL_CON22		APMIXED_REG(0x0770)
#define AUDUL_CON23		APMIXED_REG(0x0774)
#define AUDUL_CON24		APMIXED_REG(0x0778)

/* AP_TENSE_CON00 (0x0000) */
#define AP_TENSE_CON00_BRG_FASTUP_MASK		BIT(7)
#define AP_TENSE_CON00_BRG_FASTUP		BIT(7)

/* AUDIO_TOP_CON0 (0x0000) */
#define AUD_TCON0_PDN_ADC			BIT(28)
#define AUD_TCON0_PDN_TML			BIT(27)
#define AUD_TCON0_PDN_DAC_PREDIS		BIT(26)
#define AUD_TCON0_PDN_DAC			BIT(25)
#define AUD_TCON0_PDN_APLL2			BIT(24)
#define AUD_TCON0_PDN_APLL			BIT(23)
#define AUD_TCON0_PDN_SPDIF_OUT		BIT(21)
#define AUD_TCON0_PDN_UPLINK_TML		BIT(18)
#define AUD_TCON0_SPDF_OUT_PLL_SEL_MASK	BIT(15)
#define AUD_TCON0_PDN_AFE			BIT(2)



/* AFE_ADDA_DL_SRC2_CON0 */
#define AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE_MASK	GENMASK(31, 28)
#define AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE_VAL(x)	(((x) & 0xf) << 28)
#define AFE_ADDA_DL_SRC2_CON0_DL_OUTPUT_SEL_MASK	GENMASK(25, 24)
#define AFE_ADDA_DL_SRC2_CON0_DL_OUTPUT_SEL_VAL(x)	(((x) & 0x3) << 24)
#define AFE_ADDA_DL_SRC2_CON0_MUTE_OFF_CTRL_MASK	GENMASK(12, 11)
#define AFE_ADDA_DL_SRC2_CON0_MUTE_OFF			GENMASK(12, 11)
#define AFE_ADDA_DL_SRC2_CON0_DL_VOICE_MODE_MASK	BIT(5)
#define AFE_ADDA_DL_SRC2_CON0_DL_VOICE_MODE(x)		(((x) & 0x1) << 5)
#define AFE_ADDA_DL_SRC2_CON0_GAIN_ON_MASK		BIT(1)
#define AFE_ADDA_DL_SRC2_CON0_GAIN_ON			BIT(1)
#define AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_MASK		BIT(0)
#define AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON		BIT(0)

/* AFE_ADDA_DL_SRC2_CON1 */
#define AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_MASK		GENMASK(31, 16)
#define AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_VAL(x)		(((x) & 0xffff) << 16)
#define AFE_ADDA_DL_SRC2_CON1_GAIN_MODE_MASK		BIT(0)

/* AFE_ADDA_TOP_CON0 */
#define AFE_ADDA_TOP_CON0_LOOPBACK_MODE_MASK		GENMASK(15, 12)
#define AFE_ADDA_TOP_CON0_LOOPBACK_MODE_VAL		(((x) & 0xf) << 12)

/* AFE_ADDA_UL_DL_CON0 */
#define AFE_ADDA_UL_DL_CON0_DL_SW_RESET_MASK            BIT(15)
#define AFE_ADDA_UL_DL_CON0_DL_LOOPBACK_MASK		BIT(3)
#define AFE_ADDA_UL_DL_CON0_DL_LOOPBACK_ON		BIT(3)
#define AFE_ADDA_UL_DL_CON0_DL_INTERCONN_BYPASS_MASK	BIT(2)
#define AFE_ADDA_UL_DL_CON0_DL_INTERCONN_BYPASS	BIT(2)
#define AFE_ADDA_UL_DL_CON0_ADDA_INTF_ON_MASK		BIT(0)
#define AFE_ADDA_UL_DL_CON0_ADDA_INTF_ON		BIT(0)

/* AFE_ADDA_PREDIS_CON0 */
#define AFE_ADDA_PREDIS_CON0_PREDIS_CH1_ON_MASK		BIT(31)
#define AFE_ADDA_PREDIS_CON0_PREDIS_CH1_ON		BIT(31)
#define AFE_ADDA_PREDIS_CON0_PREDIS_A2_CH1_MASK		GENMASK(27, 16)
#define AFE_ADDA_PREDIS_CON0_PREDIS_A2_CH1_VAL(x)	(((x) & 0xfff) << 16)
#define AFE_ADDA_PREDIS_CON0_PREDIS_A3_CH1_MASK		GENMASK(11, 0)
#define AFE_ADDA_PREDIS_CON0_PREDIS_A3_CH1_VAL(x)	((x) & 0xfff)

/* AFE_ADDA_PREDIS_CON1 */
#define AFE_ADDA_PREDIS_CON1_PREDIS_CH2_ON_MASK		BIT(31)
#define AFE_ADDA_PREDIS_CON1_PREDIS_CH2_ON			BIT(31)
#define AFE_ADDA_PREDIS_CON1_PREDIS_A2_CH2_MASK		GENMASK(27, 16)
#define AFE_ADDA_PREDIS_CON1_PREDIS_A2_CH2_VAL(x)	(((x) & 0xfff) << 16)
#define AFE_ADDA_PREDIS_CON1_PREDIS_A3_CH2_MASK		GENMASK(11, 0)
#define AFE_ADDA_PREDIS_CON1_PREDIS_A3_CH2_VAL(x)		((x) & 0xfff)

/* AFE_NLE_CFG */
#define AFE_NLE_CFG_SW_RSTB_MASK			BIT(31)
#define AFE_NLE_CFG_SW_RSTB_VAL(x)			(((x) & 0x1) << 31)
#define AFE_NLE_CFG_AFE_NLE_ON_MASK			BIT(0)
#define AFE_NLE_CFG_AFE_NLE_ON				BIT(0)

/* AFE_NLE_PRE_BUF_CFG */
#define AFE_NLE_PRE_BUF_CFG_POINT_END_MASK		GENMASK(10, 0)
#define AFE_NLE_PRE_BUF_CFG_POINT_END_VAL		((x) & 0x3ff)

/* AFE_NLE_PWR_DET_LCH_CFG
 * AFE_NLE_PWR_DET_RCH_CFG
 */
#define H2L_HOLD_TIME_MS				(26)
#define AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_MASK	GENMASK(28, 24)
#define AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_VAL(x)	(((x) & 0x1f) << 24)
#define AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_DEF_VAL	(H2L_HOLD_TIME_MS << 24)
#define AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_MASK		GENMASK(23, 0)
#define AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_VAL(x)		((x) & 0xffffff)

/* AFE_NLE_ZCD_LCH_CFG
 * AFE_NLE_ZCD_RCH_CFG
 */
#define AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_MASK		BIT(2)
#define AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_VAL(x)	(x << 2)
#define AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_MASK		GENMASK(1, 0)
#define AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_VAL(x)		((x) & 0x3)

/* AFE_NLE_GAIN_ADJ_LCH_CFG0
 * AFE_NLE_GAIN_ADJ_RCH_CFG0
 */
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_MASK	BIT(31)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_VAL(x)	(x << 31)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_MASK			GENMASK(26, 24)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_VAL(x)	(((x) & 0x7) << 24)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_MASK	GENMASK(22, 20)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_VAL(x) (((x) & 0x7) << 20)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_MASK	GENMASK(17, 16)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_VAL(x) (((x) & 0x3) << 16)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_MASK	GENMASK(13, 8)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_VAL(x)	(x << 8)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_MASK			GENMASK(7, 4)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_VAL(x)		(((x) & 0xf) << 4)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_MASK			GENMASK(2, 0)
#define AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_VAL(x)			((x) & 0x7)

/* AFE_NLE_GAIN_IMP_LCH_CFG0
 * AFE_NLE_GAIN_IMP_RCH_CFG0
 */
#define AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_MODE		BIT(29)
#define AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_VAL(x)		((x) << 29)
#define AFE_NLE_ANALOG_GAIN_FIX_MANUAL_MODE		BIT(28)
#define AFE_NLE_ANALOG_GAIN_FIX_MANUAL_VAL(x)		((x) << 28)
#define AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_MASK		GENMASK(21, 16)
#define AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_VAL(x)	(((x) & 0x3f) << 16)
#define AFE_NLE_DIGITAL_GAIN_MANUAL_VAL_MASK		GENMASK(13, 8)
#define AFE_NLE_DIGITAL_GAIN_MANUAL_VAL(x)		(((x) & 0x3f) << 8)
#define AFE_NLE_ANALOG_GAIN_MANUAL_VAL_MASK		GENMASK(5, 0)
#define AFE_NLE_ANALOG_GAIN_MANUAL_VAL(x)		(((x) & 0x3f))

/* ABB_AFE_CON0 */
#define ABB_AFE_CON0_DL_EN_MASK				BIT(0)
#define ABB_AFE_CON0_DL_EN				BIT(0)

/* ABB_AFE_CON1 */
#define ABB_AFE_CON1_DL_RATE_MASK			GENMASK(3, 0)
#define ABB_AFE_CON1_DL_RATE(x)				((x) & 0xf)

/* ABB_AFE_CON5 */
#define ABB_AFE_CON5_SDM_GAIN_VAL_MASK			GENMASK(5, 0)
#define ABB_AFE_CON5_SDM_GAIN_VAL(x)			((x) & 0x3f)

/* ABB_AFE_CON11 */
#define ABB_AFE_CON11_DC_CTRL				BIT(9)
#define ABB_AFE_CON11_TOP_CTRL				BIT(8)
#define ABB_AFE_CON11_DC_CTRL_STATUS			BIT(1)
#define ABB_AFE_CON11_TOP_CTRL_STATUS			BIT(0)

/* AFE_AD_UL_DL_CON0 */
#define AFE_AD_UL_DL_CON0_ADDA_AFE_ON			BIT(0)

/* ABB_ULAFE_CON0 */
#define ABB_ULAFE_CON0_AMIC_CH1_PHASE_SEL_MASK		GENMASK(29, 27)
#define ABB_ULAFE_CON0_AMIC_CH1_PHASE_SEL(x)		(((x) & 0x7) << 27)
#define ABB_ULAFE_CON0_AMIC_CH2_PHASE_SEL_MASK		GENMASK(26, 24)
#define ABB_ULAFE_CON0_AMIC_CH2_PHASE_SEL(x)		(((x) & 0x7) << 24)
#define ABB_ULAFE_CON0_UL_VOICE_MODE_MASK		GENMASK(19, 17)
#define ABB_ULAFE_CON0_UL_VOICE_MODE(x)			(((x) & 0x7) << 17)
#define ABB_ULAFE_CON0_UL_LP_MODE_MASK			GENMASK(15, 14)
#define ABB_ULAFE_CON0_UL_LP_MODE(x)			(((x) & 0x3) << 14)
#define ABB_ULAFE_CON0_UL_IIR_ON_MASK			BIT(10)
#define ABB_ULAFE_CON0_UL_IIR_ON			BIT(10)
#define ABB_ULAFE_CON0_UL_IIR_MODE_MASK			GENMASK(9, 7)
#define ABB_ULAFE_CON0_UL_IIR_MODE(x)			(((x) & 0x7) << 7)
#define ABB_ULAFE_CON0_UL_FIFO_ON_MASK			BIT(3)
#define ABB_ULAFE_CON0_UL_FIFO_ON			BIT(3)
/*1:No loopback,0:DAC loopback to ADC need fix 1, loopback can not use in 8512*/
#define ABB_ULAFE_CON0_AD_DA_LOOPBACK_DIS_MASK		BIT(1)
#define ABB_ULAFE_CON0_AD_DA_LOOPBACK_DIS		BIT(1)
#define ABB_ULAFE_CON0_UL_SRC_ON_MASK			BIT(0)
#define ABB_ULAFE_CON0_UL_SRC_ON			BIT(0)


/* ABB_ULAFE_CON1 */
#define ABB_ULAFE_CON1_UL_GAIN_BYPASS_MASK		BIT(28)
#define ABB_ULAFE_CON1_UL_GAIN_NO_BYPASS		BIT(28)
#define ABB_ULAFE_CON1_UL_SINEGEN_OUTPUT_MASK		BIT(27)
#define ABB_ULAFE_CON1_UL_SINEGEN_OUTPUT		BIT(27)
#define ABB_ULAFE_CON1_UL_SINEGEN_MUTE_MASK		BIT(26)
#define ABB_ULAFE_CON1_UL_SINEGEN_MUTE			BIT(26)
#define ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH2_MASK	GENMASK(23, 21)
#define ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH2_VAL(x)	(((x) & 0x7) << 21)
#define ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH2_MASK	GENMASK(20, 16)
#define ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH2_VAL(x)	(((x) & 0x1f) << 16)
#define ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH1_MASK	GENMASK(11, 9)
#define ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH1_VAL(x)	(((x) & 0x7) << 9)
#define ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH1_MASK	GENMASK(8, 4)
#define ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH1_VAL(x)	(((x) & 0x1f) << 4)

/* AMIC_GAIN_CON0 */
#define AMIC_GAIN_CON0_SAMPLE_PER_STEP_MASK		GENMASK(15, 8)
#define AMIC_GAIN_CON0_SAMPLE_PER_STEP_VAL(x)		(((x) & 0xff) << 8)
#define AMIC_GAIN_CON0_GAIN_EN_MASK			BIT(0)
#define AMIC_GAIN_CON0_GAIN_ON				BIT(0)

/* AMIC_GAIN_CON1 */
#define AMIC_GAIN_CON1_TARGET_MASK			GENMASK(19, 0)
#define AMIC_GAIN_CON1_TARGET_VAL(x)			((x) & 0xfffff)

/* AMIC_GAIN_CON2 */
#define AMIC_GAIN_CON2_DOWN_STEP_MASK			GENMASK(19, 0)
#define AMIC_GAIN_CON2_DOWN_STEP_VAL(x)		((x) & 0xfffff)

/* AMIC_GAIN_CON3 */
#define AMIC_GAIN_CON3_UP_STEP_MASK			GENMASK(19, 0)
#define AMIC_GAIN_CON3_UP_STEP_VAL(x)			((x) & 0xfffff)

/* AMIC_GAIN_CUR */
#define AMIC_GAIN_CUR_CUR_GAIN_MASK			GENMASK(19, 0)
#define AMIC_GAIN_CUR_CURT_GAIN_VAL(x)			(((x) & 0xfffff) << 8)

/* AUDDL_CON00 */
#define AUDDL_CON00_LDO1P8_ADAC_EN_MASK			BIT(18)
#define AUDDL_CON00_LDO_VOLSEL_ADAC_MASK		GENMASK(17, 15)
#define AUDDL_CON00_LDO_VOLSEL_ADAC_VAL(x)		(((x) & 0x7) << 15)

#define AUDDL_CON00_LDO_LAT_EN_MASK			BIT(13)
#define AUDDL_CON00_CLK26MHZ_EN_MASK			BIT(12)
#define AUDDL_CON00_CLK26MHZ_DIV_EN_MASK		BIT(11)
#define AUDDL_CON00_LDO_LAT_IQSEL_MASK			GENMASK(8, 7)
#define AUDDL_CON00_LDO_LAT_IQSEL_VAL(x)		(((x) & 0x3) << 7)
#define AUDDL_CON00_GLBIAS_ADAC_EN_MASK			BIT(6)
#define AUDDL_CON00_VMID_PWDB_MASK			BIT(5)
#define AUDDL_CON00_VMID_FASTUP_EN_MASK			BIT(4)

#define AUDDL_CON00_V2I_PWDB_MASK			BIT(3)

/* AUDDL_CON01 */
#define AUDDL_CON01_IDACL_PWDB_MASK			BIT(17)
#define AUDDL_CON01_IDACR_PWDB_MASK			BIT(16)
#define AUDDL_CON01_I2VL_PWDB_MASK			BIT(15)
#define AUDDL_CON01_I2VR_PWDB_MASK			BIT(13)

/* AUDUL_CON00 */
#define AUDUL_CON00_AUDULL_PGA_CMFB_EN_MASK		BIT(27)
#define AUDUL_CON00_AUDULL_INT_CHP_EN_MASK		BIT(16)
#define AUDUL_CON00_AUDULL_PGA_GAIN_MASK		GENMASK(8, 4)
#define AUDUL_CON00_AUDULL_PGA_GAIN_VAL(x)		(((x) & 0x1f) << 4)

/* AUDUL_CON01 */
#define AUDUL_CON01_AUDULL_RC_TRIM_SEL_MASK		BIT(25)
#define AUDUL_CON01_AUDULL_RC_TRIM_SEL			BIT(25)
#define AUDUL_CON01_AUDULL_RC_TRIM_MASK		GENMASK(5, 0)
#define AUDUL_CON01_AUDULL_RC_TRIM_VAL(x)		(((x) & 0x3f) << 0)

/* AUDUL_CON04 */
#define AUDUL_CON04_AUDULR_PGA_CMFB_EN_MASK		BIT(31)
#define AUDUL_CON04_AUDULR_INT_CHP_EN_MASK		BIT(16)
#define AUDUL_CON04_AUDULR_PGA_CHP_EN_MASK		BIT(15)
#define AUDUL_CON04_AUDULR_PGA_GAIN_MASK		GENMASK(8, 4)
#define AUDUL_CON04_AUDULR_PGA_GAIN_VAL(x)		(((x) & 0x1f) << 4)

/* AUDUL_CON05 */
#define AUDUL_CON05_AUDULR_RC_TRIM_SEL_MASK		BIT(25)
#define AUDUL_CON05_AUDULR_RC_TRIM_SEL			BIT(25)
#define AUDUL_CON05_AUDULR_RC_TRIM_MASK		GENMASK(6, 1)
#define AUDUL_CON05_AUDULR_RC_TRIM_VAL(x)		(((x) & 0x3f) << 1)

/* AUDUL_CON09 */
#define AUDUL_CON09_ADUUL_REV_MASK			GENMASK(2, 1)
#define AUDUL_CON05_AUDUL_REV_VAL(x)			(((x) & 0x3) << 1)

#define AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK		BIT(0)

/* AUDUL_CON12*/
#define AUDUL_CON11_AD_RC_CALI_EN_SIM                   BIT(1)
#define AUDUL_CON11_AD_RC_CALI_MONITOR_EN               BIT(0)

/* AUDDL_CON18 */
#define AUDDL_CON18_DEPOP_CLK_EN_MASK			BIT(31)
#define AUDDL_CON18_DP_RAMP_SEL_MASK                    GENMASK(27, 26)
#define AUDDL_CON18_DP_RAMP_SEL_VAL(x)	               (((x) & 0x3) << 26)

#define AUDDL_CON18_DEPOP_RAMPGEN_START_MASK		BIT(22)
#define AUDDL_CON18_DEPOP_RAMPGEN_EN_MASK		BIT(21)
#define AUDDL_CON18_DEPOP_RAMPGEN_CAP_RESET_MASK	BIT(19)
#define AUDDL_CON18_ENVO_MASK				BIT(18)
#define AUDDL_CON18_ENDP_MASK				BIT(17)
#define AUDDL_CON18_DP_S1_MASK				BIT(16)
#define AUDDL_CON18_DP_S0_MASK				BIT(15)
#define AUDDL_CON18_VCMB_SEL_MASK			BIT(14)
#define AUDDL_CON18_VCMBUF_EN_MASK			BIT(13)

#define AUDDL_CON18_RELATCH_EN_MASK			BIT(5)
#define AUDDL_CON18_CK_6P5M_FIFO_EN_MASK		BIT(2)
#define AUDDL_CON18_DEPOP_VMID_RSEL_MASK		GENMASK(1, 0)
#define AUDDL_CON18_DEPOP_VMID_RSEL_VAL(x)		((x) & 0x3)

/* AUDUL_CON19 */
#define AUDUL_CON19_LDO_LAT_VOSEL_MASK			GENMASK(6, 5)
#define AUDUL_CON19_LDO_LAT_VOSEL_VAL(x)		(((x) & 0x3) << 5)
#define AUDUL_CON19_DP_PL_SEL_MASK			BIT(4)
#define AUDUL_CON19_DP_PL_EN_MASK			BIT(3)
#define AUDUL_CON19_DEPOP_CLK_SEL_MASK			GENMASK(2, 0)
#define AUDUL_CON19_DEPOP_CLK_SEL_VAL(x)		((x) & 0x7)

/* AUDUL_CON20 */
#define AUDUL_CON20_GLBIAS_EN_AUDUL_MASK		BIT(21)
#define AUDUL_CON20_AUD_PWDB_MICBIAS_MASK		BIT(20)
#define AUDUL_CON20_AUD_MICBIAS_VREF_MASK		GENMASK(18, 17)
#define AUDUL_CON20_AUD_MICBIAS_VREF_VAL(x)		(((x) & 0x3) << 17)
#define AUDUL_CON20_LDO_PWDB_MASK			BIT(13)
#define AUDUL_CON20_LDO18_VOSEL_MASK			GENMASK(12, 10)
#define AUDUL_CON20_LDO18_VOSEL_VAL(x)			(((x) & 0x7) << 10)
#define AUDUL_CON20_AUDULR_LDO08_VOSEL_MASK		GENMASK(7, 6)
#define AUDUL_CON20_AUDULR_LDO08_VOSEL_VAL(x)		(((x) & 0x3) << 6)
#define AUDUL_CON20_AUDULL_LDO08_VOSEL_MASK		GENMASK(5, 4)
#define AUDUL_CON20_AUDULL_LDO08_VOSEL_VAL(x)		(((x) & 0x3) << 4)
#define AUDUL_CON20_AUDULR_LDO08_PWDB_MASK		BIT(3)
#define AUDUL_CON20_AUDULL_LDO08_PWDB_MASK		BIT(2)
#define AUDUL_CON20_CLK_SEL_MASK			BIT(1)
#define AUDUL_CON20_CLK_EN_MASK				BIT(0)

/* AUDUL_CON21 */
#define AUDUL_CON21_VREF_EN_MASK			BIT(30)
#define AUDUL_CON21_VREF_CHP_EN_MASK			BIT(29)
#define AUDUL_CON21_VCM_EN_MASK				BIT(26)
#define AUDUL_CON21_VCM_CHP_EN_MASK			BIT(25)
#define AUDUL_CON21_RC_CHARGIN_EN                       BIT(24)
#define AUDUL_CON21_C_CALB_PATH_SEL                     BIT(23)
#define AUDUL_CON21_C_CALB_EN                           BIT(22)

/* AUDUL_CON22 */
#define AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_MASK	GENMASK(30, 29)
#define AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_VAL(x)	(((x) & 0x3) << 29)
#define AUDUL_CON22_AUDULL_SARADC_RESET_MASK		BIT(28)
#define AUDUL_CON22_AUDULL_SARADC_EN_MASK		BIT(27)
#define AUDUL_CON22_AUDULL_SARADC_CTRL_SEL_MASK		GENMASK(26, 19)
#define AUDUL_CON22_AUDULL_SARADC_CTRL_SEL_VAL(x)	(((x) & 0xff) << 19)
#define AUDUL_CON22_AUDULL_SARADC_DEC_DLY_SEL_MASK	GENMASK(18, 17)
#define AUDUL_CON22_AUDULL_SARADC_DEC_DLY_SEL_VAL(x)	(((x) & 0x3) << 17)
#define AUDUL_CON22_AUDULL_PGA_PWDB_MASK		BIT(16)
#define AUDUL_CON22_AUDULL_PGA_CHP_EN_MASK		BIT(11)
#define AUDUL_CON22_AUDULL_PGA_OUTPUT_EN_MASK		BIT(10)
#define AUDUL_CON22_AUDULL_PDN_INT1OP_MASK		BIT(9)
#define AUDUL_CON22_AUDULL_INT2_RESET_MASK		BIT(5)
#define AUDUL_CON22_AUDULL_INT1_RESET_MASK		BIT(4)
#define AUDUL_CON22_VREF_CURRENT_ADJUST_MASK		GENMASK(2, 0)
#define AUDUL_CON22_VREF_CURRENT_ADJUST_VAL(x)		((x) & 0x7)


/* AUDUL_CON23 */
#define AUDUL_CON23_AUDULR_PGA_OUTPUT_EN_MASK		BIT(27)
#define AUDUL_CON23_AUDULR_PDN_INT1OP_MASK		BIT(26)
#define AUDUL_CON23_AUDULR_INT2_RESET_MASK		BIT(22)
#define AUDUL_CON23_AUDULR_INT1_RESET_MASK		BIT(21)
#define AUDUL_CON23_AUDULL_VCM09_SEL_MASK		GENMASK(16, 14)
#define AUDUL_CON23_AUDULL_VCM09_SELL_VAL(x)		(((x) & 0x7) << 14)
#define AUDUL_CON23_AUDULL_VCM08_SEL_MASK		GENMASK(13, 11)
#define AUDUL_CON23_AUDULL_VCM08_SELL_VAL(x)		(((x) & 0x7) << 11)
#define AUDUL_CON23_AUDULL_SARADC_VREF_ISEL_MASK	GENMASK(10, 8)
#define AUDUL_CON23_AUDULL_SARADC_VREF_ISEL_VAL(x)	(((x) & 0x7) << 8)
#define AUDUL_CON23_AUDULL_SARADC_SEL_MASK		GENMASK(7, 0)
#define AUDUL_CON23_AUDULL_SARADC_SEL_VAL(x)		(((x) & 0xff) << 17)

/* AUDUL_CON24 */
#define AUDUL_CON24_AUDULR_VCM09_SEL_MASK		GENMASK(31, 29)
#define AUDUL_CON24_AUDULR_VCM09_SEL_VAL(x)		(((x) & 0x7) << 29)
#define AUDUL_CON24_AUDULR_VCM08_SEL_MASK		GENMASK(28, 26)
#define AUDUL_CON24_AUDULR_VCM08_SEL_VAL(x)		(((x) & 0x7) << 26)
#define AUDUL_CON24_AUDULR_SARADC_VREF_ISEL_MASK	GENMASK(25, 23)
#define AUDUL_CON24_AUDULR_SARADC_VREF_ISEL_VAL(x)	(((x) & 0x7) << 23)
#define AUDUL_CON24_AUDULR_SARADC_SEL_MASK		GENMASK(22, 15)
#define AUDUL_CON24_AUDULR_SARADC_SEL_VAL(x)		(((x) & 0xff) << 15)
#define AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_MASK	GENMASK(14, 13)
#define AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_VAL(x)	(((x) & 0x7) << 13)
#define AUDUL_CON24_AUDULR_SARADC_RESET_MASK		BIT(12)
#define AUDUL_CON24_AUDULR_SARADC_EN_MASK		BIT(11)
#define AUDUL_CON24_AUDULR_SARADC_CTRL_SEL_MASK		GENMASK(10, 3)
#define AUDUL_CON24_AUDULR_SARADC_CTRL_SEL_VAL(x)	(((x) & 0xff) << 3)
#define AUDUL_CON24_AUDULR_SARADC_DEC_DLY_SEL_MASK	GENMASK(2, 1)
#define AUDUL_CON24_AUDULR_SARADC_DEC_DLY_SEL_VAL(x)	(((x) & 0xff) << 3)
#define AUDUL_CON24_AUDULR_PGA_PWDB_MASK		BIT(0)


#endif
