/*
 * mt8512-afe-controls.c  --  Mediatek 8512 audio controls
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

#include "mt8512-afe-controls.h"
#include "mt8512-afe-common.h"
#include "mt8512-reg.h"
#include "mt8512-afe-utils.h"
#include "../common/mtk-base-afe.h"
#include <sound/soc.h>

#define ENUM_TO_STR(enum) #enum

enum mixer_control_func {
	CTRL_SINEGEN_LOOPBACK_MODE = 0,
	CTRL_SINEGEN_TIMING,
	CTRL_ETDM_IN_DATA_SRC,
	CTRL_ETDM_IN_SLAVE_MODE_SRC,
	CTRL_MULTI_IN_BCK_SEL,
	CTRL_MULTI_IN_LRCK_SEL,
	CTRL_MULTI_IN_SDATA0_SEL,
	CTRL_MULTI_IN_SDATA1_SEL,
	CTRL_MULTI_IN_SDATA2_SEL,
	CTRL_MULTI_IN_SDATA3_SEL,
	CTRL_SPDIF_IN_PORT_SEL,
};

enum sinegen_loopback_mode {
	SINEGEN_I00_I01 = 0,
	SINEGEN_I02_I03,
	SINEGEN_I04_I11,
	SINEGEN_I12_I19,
	SINEGEN_I20_I21,
	SINEGEN_I22_I29,
	SINEGEN_I30_I33,
	SINEGEN_I34_I35,
	SINEGEN_I36_I37,
	SINEGEN_I38_I39,
	SINEGEN_I40_I41,
	SINEGEN_I42_I57,
	SINEGEN_O00_O01,
	SINEGEN_O02_O03,
	SINEGEN_O04_O11,
	SINEGEN_O12_O13,
	SINEGEN_O14_O15,
	SINEGEN_O16_O17,
	SINEGEN_O20_O21,
	SINEGEN_O42_O43,
	SINEGEN_O44_O45,
	SINEGEN_O46_O47,
	SINEGEN_O48_O49,
	SINEGEN_O26_O41,
	SINEGEN_UL9,
	SINEGEN_UL2,
	SINEGEN_I60_I61 = 28,
	SINEGEN_I58_I59,
	SINEGEN_NONE,
};

enum sinegen_timing {
	SINEGEN_8K = 0,
	SINEGEN_12K,
	SINEGEN_16K,
	SINEGEN_24K,
	SINEGEN_32K,
	SINEGEN_48K,
	SINEGEN_96K,
	SINEGEN_192K,
	SINEGEN_384K,
	SINEGEN_7P35K,
	SINEGEN_11P025K,
	SINEGEN_14P7K,
	SINEGEN_22P05K,
	SINEGEN_29P4K,
	SINEGEN_44P1K,
	SINEGEN_88P2K,
	SINEGEN_176P4K,
	SINEGEN_352P8K,
	SINEGEN_DL_1X_EN,
	SINEGEN_SGEN_EN,
};

enum etdm_in_data_src {
	ETDM_IN_FROM_PAD = 0,
	ETDM_IN_FROM_ETDM_OUT2,
};

enum etdm_in_slave_mode_src {
	ETDM_IN_SLAVE_FROM_SELF = 0,
	ETDM_IN_SLAVE_FROM_ETDM_OUT2,
};

static const char *const sinegen_loopback_mode_func[] = {
	ENUM_TO_STR(SINEGEN_I00_I01),
	ENUM_TO_STR(SINEGEN_I02_I03),
	ENUM_TO_STR(SINEGEN_I04_I11),
	ENUM_TO_STR(SINEGEN_I12_I19),
	ENUM_TO_STR(SINEGEN_I20_I21),
	ENUM_TO_STR(SINEGEN_I22_I29),
	ENUM_TO_STR(SINEGEN_I30_I33),
	ENUM_TO_STR(SINEGEN_I34_I35),
	ENUM_TO_STR(SINEGEN_I36_I37),
	ENUM_TO_STR(SINEGEN_I38_I39),
	ENUM_TO_STR(SINEGEN_I40_I41),
	ENUM_TO_STR(SINEGEN_I42_I57),
	ENUM_TO_STR(SINEGEN_O00_O01),
	ENUM_TO_STR(SINEGEN_O02_O03),
	ENUM_TO_STR(SINEGEN_O04_O11),
	ENUM_TO_STR(SINEGEN_O12_O13),
	ENUM_TO_STR(SINEGEN_O14_O15),
	ENUM_TO_STR(SINEGEN_O16_O17),
	ENUM_TO_STR(SINEGEN_O20_O21),
	ENUM_TO_STR(SINEGEN_O42_O43),
	ENUM_TO_STR(SINEGEN_O44_O45),
	ENUM_TO_STR(SINEGEN_O46_O47),
	ENUM_TO_STR(SINEGEN_O48_O49),
	ENUM_TO_STR(SINEGEN_O26_O41),
	ENUM_TO_STR(SINEGEN_UL9),
	ENUM_TO_STR(SINEGEN_UL2),
	ENUM_TO_STR(SINEGEN_I60_I61),
	ENUM_TO_STR(SINEGEN_I58_I59),
	ENUM_TO_STR(SINEGEN_NONE),
};

static const char *const sinegen_timing_func[] = {
	ENUM_TO_STR(SINEGEN_8K),
	ENUM_TO_STR(SINEGEN_12K),
	ENUM_TO_STR(SINEGEN_16K),
	ENUM_TO_STR(SINEGEN_24K),
	ENUM_TO_STR(SINEGEN_32K),
	ENUM_TO_STR(SINEGEN_48K),
	ENUM_TO_STR(SINEGEN_96K),
	ENUM_TO_STR(SINEGEN_192K),
	ENUM_TO_STR(SINEGEN_384K),
	ENUM_TO_STR(SINEGEN_7P35K),
	ENUM_TO_STR(SINEGEN_11P025K),
	ENUM_TO_STR(SINEGEN_14P7K),
	ENUM_TO_STR(SINEGEN_22P05K),
	ENUM_TO_STR(SINEGEN_29P4K),
	ENUM_TO_STR(SINEGEN_44P1K),
	ENUM_TO_STR(SINEGEN_88P2K),
	ENUM_TO_STR(SINEGEN_176P4K),
	ENUM_TO_STR(SINEGEN_352P8K),
	ENUM_TO_STR(SINEGEN_DL_1X_EN),
	ENUM_TO_STR(SINEGEN_SGEN_EN),
};

static const char *const etdm_in_data_src_func[] = {
	ENUM_TO_STR(ETDM_IN_FROM_PAD),
	ENUM_TO_STR(ETDM_IN_FROM_ETDM_OUT2),
};

static const char *const etdm_in_slave_mode_src_func[] = {
	ENUM_TO_STR(ETDM_IN_SLAVE_FROM_SELF),
	ENUM_TO_STR(ETDM_IN_SLAVE_FROM_ETDM_OUT2),
};

static const char *const multi_in_bck_sel_func[] = {
	"SPLIN_BCK",
	"I2S_IN_SLAVE_BCK",
	"I2S_OUT_SLAVE_BCK",
	"TDM_IN_SLAVE_BCK",
	"TDM_OUT_SLAVE_BCK",
	"I2S_IN_MASTER_BCK",
	"I2S_OUT_MASTER_BCK",
	"TDM_IN_MASTER_BCK",
	"TDM_OUT_MASTER_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
	"NO_BCK",
};

static const char *const multi_in_lrck_sel_func[] = {
	"SPLIN_LRCK",
	"I2S_IN_SLAVE_LRCK",
	"I2S_OUT_SLAVE_LRCK",
	"TDM_IN_SLAVE_LRCK",
	"TDM_OUT_SLAVE_LRCK",
	"I2S_IN_MASTER_LRCK",
	"I2S_OUT_MASTER_LRCK",
	"TDM_IN_MASTER_LRCK",
	"TDM_OUT_MASTER_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
	"NO_LRCK",
};

static const char *const multi_in_sdata_sel_func[] = {
	"SPLIN_SDATA",
	"I2S_OUT_DATA",
	"NO_DATA",
	"I2S_IN_DATA",
};

static const char *const spdif_in_port_sel_func[] = {
	ENUM_TO_STR(SPDIF_IN_PORT_NONE),
	ENUM_TO_STR(SPDIF_IN_PORT_OPT),
	ENUM_TO_STR(SPDIF_IN_PORT_COAXIAL),
	ENUM_TO_STR(SPDIF_IN_PORT_ARC),
};

static const struct soc_enum mt8512_afe_soc_enums[] = {
	[CTRL_SINEGEN_LOOPBACK_MODE] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_loopback_mode_func),
				    sinegen_loopback_mode_func),
	[CTRL_SINEGEN_TIMING] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sinegen_timing_func),
				    sinegen_timing_func),
	[CTRL_ETDM_IN_DATA_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(etdm_in_data_src_func),
				    etdm_in_data_src_func),
	[CTRL_ETDM_IN_SLAVE_MODE_SRC] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(etdm_in_slave_mode_src_func),
				    etdm_in_slave_mode_src_func),
	[CTRL_MULTI_IN_BCK_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 0,
			ARRAY_SIZE(multi_in_bck_sel_func),
			multi_in_bck_sel_func),
	[CTRL_MULTI_IN_LRCK_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 4,
			ARRAY_SIZE(multi_in_lrck_sel_func),
			multi_in_lrck_sel_func),
	[CTRL_MULTI_IN_SDATA0_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 8,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA1_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 10,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA2_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 12,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_MULTI_IN_SDATA3_SEL] =
		SOC_ENUM_SINGLE(AFE_LOOPBACK_CFG0, 14,
			ARRAY_SIZE(multi_in_sdata_sel_func),
			multi_in_sdata_sel_func),
	[CTRL_SPDIF_IN_PORT_SEL] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spdif_in_port_sel_func),
				    spdif_in_port_sel_func),
};

struct gnt_memif_info {
	int dai_id;
	char dai_name[32];
	unsigned int pbuf_size_reg;
	unsigned int pbuf_size_shift;
	unsigned int pbuf_size_mask;
	unsigned int pbuf_size_unit;
	unsigned int pbuf_mon_reg;
	unsigned int pbuf_mon_shift;
	unsigned int pbuf_mon_mask;
	unsigned int pbuf_mon_unit;
	int cur_tolerance;
};

struct gnt_memif_info gnt_memif_data[] = {
	{
		.dai_id = MT8512_AFE_MEMIF_DLM,
		.dai_name = "DLM",
		.pbuf_size_reg = AFE_DL10_CON0,
		.pbuf_size_shift = 16,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 64,
		.pbuf_mon_reg = AFE_MEMIF_BUF_MON5,
		.pbuf_mon_shift = 16,
		.pbuf_mon_mask = 0x7f,
		.pbuf_mon_unit = 8,
		.cur_tolerance = 1,
	},
	{
		.dai_id = MT8512_AFE_MEMIF_DL3,
		.dai_name = "DL3",
		.pbuf_size_reg = AFE_DL3_CON0,
		.pbuf_size_shift = 16,
		.pbuf_size_mask = 0x3,
		.pbuf_size_unit = 32,
		.pbuf_mon_reg = AFE_MEMIF_BUF_MON1,
		.pbuf_mon_shift = 0,
		.pbuf_mon_mask = 0x3f,
		.pbuf_mon_unit = 8,
		.cur_tolerance = 1,
	},
};


static int mt8512_afe_cm_bypass_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM0_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm0;
	else if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		ucontrol->value.integer.value[0] = data->bypass_cm1;

	return 0;
}

static int mt8512_afe_cm_bypass_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_control_data *data = &afe_priv->ctrl_data;

	if (!strcmp(kcontrol->id.name, "CM0_Bypass_Switch"))
		data->bypass_cm0 = (ucontrol->value.integer.value[0]) ?
				   true : false;
	else if (!strcmp(kcontrol->id.name, "CM1_Bypass_Switch"))
		data->bypass_cm1 = (ucontrol->value.integer.value[0]) ?
				   true : false;

	return 0;
}

static int mt8512_afe_singen_enable_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & AFE_SINEGEN_CON0_EN);

	return 0;
}

static int mt8512_afe_singen_enable_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0]) {
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_TML);

		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   AFE_SINEGEN_CON0_EN);
	} else {
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
				   AFE_SINEGEN_CON0_EN,
				   0x0);

		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_TML);
	}

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_sinegen_loopback_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int mode;
	unsigned int val;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &val);

	mt8512_afe_disable_main_clk(afe);

	mode = (val & AFE_SINEGEN_CON0_MODE_MASK) >> 27;

	if (mode >= SINEGEN_NONE)
		mode = SINEGEN_NONE;

	ucontrol->value.integer.value[0] = mode;

	return 0;
}

static int mt8512_afe_sinegen_loopback_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int mode;
	unsigned int val;

	mode = ucontrol->value.integer.value[0];

	val = (mode << 27) & AFE_SINEGEN_CON0_MODE_MASK;

	mt8512_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON0,
			   AFE_SINEGEN_CON0_MODE_MASK, val);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_sinegen_timing_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int timing;
	unsigned int val;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	val = (val & AFE_SINEGEN_CON1_TIMING_CH1_MASK) >> 16;

	switch (val) {
	case AFE_SINEGEN_CON1_TIMING_8K:
		timing = SINEGEN_8K;
		break;
	case AFE_SINEGEN_CON1_TIMING_12K:
		timing = SINEGEN_12K;
		break;
	case AFE_SINEGEN_CON1_TIMING_16K:
		timing = SINEGEN_16K;
		break;
	case AFE_SINEGEN_CON1_TIMING_24K:
		timing = SINEGEN_24K;
		break;
	case AFE_SINEGEN_CON1_TIMING_32K:
		timing = SINEGEN_32K;
		break;
	case AFE_SINEGEN_CON1_TIMING_48K:
		timing = SINEGEN_48K;
		break;
	case AFE_SINEGEN_CON1_TIMING_96K:
		timing = SINEGEN_96K;
		break;
	case AFE_SINEGEN_CON1_TIMING_192K:
		timing = SINEGEN_192K;
		break;
	case AFE_SINEGEN_CON1_TIMING_384K:
		timing = SINEGEN_384K;
		break;
	case AFE_SINEGEN_CON1_TIMING_7P35K:
		timing = SINEGEN_7P35K;
		break;
	case AFE_SINEGEN_CON1_TIMING_11P025K:
		timing = SINEGEN_11P025K;
		break;
	case AFE_SINEGEN_CON1_TIMING_14P7K:
		timing = SINEGEN_14P7K;
		break;
	case AFE_SINEGEN_CON1_TIMING_22P05K:
		timing = SINEGEN_22P05K;
		break;
	case AFE_SINEGEN_CON1_TIMING_29P4K:
		timing = SINEGEN_29P4K;
		break;
	case AFE_SINEGEN_CON1_TIMING_44P1K:
		timing = SINEGEN_44P1K;
		break;
	case AFE_SINEGEN_CON1_TIMING_88P2K:
		timing = SINEGEN_88P2K;
		break;
	case AFE_SINEGEN_CON1_TIMING_176P4K:
		timing = SINEGEN_176P4K;
		break;
	case AFE_SINEGEN_CON1_TIMING_352P8K:
		timing = SINEGEN_352P8K;
		break;
	case AFE_SINEGEN_CON1_TIMING_DL_1X_EN:
		timing = SINEGEN_DL_1X_EN;
		break;
	case AFE_SINEGEN_CON1_TIMING_SGEN_EN:
		timing = SINEGEN_SGEN_EN;
		break;
	default:
		timing = SINEGEN_8K;
		break;
	}

	ucontrol->value.integer.value[0] = timing;

	return 0;
}

static int mt8512_afe_sinegen_timing_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int timing;
	unsigned int val;

	timing = ucontrol->value.integer.value[0];

	switch (timing) {
	case SINEGEN_8K:
		val = AFE_SINEGEN_CON1_TIMING_8K;
		break;
	case SINEGEN_12K:
		val = AFE_SINEGEN_CON1_TIMING_12K;
		break;
	case SINEGEN_16K:
		val = AFE_SINEGEN_CON1_TIMING_16K;
		break;
	case SINEGEN_24K:
		val = AFE_SINEGEN_CON1_TIMING_24K;
		break;
	case SINEGEN_32K:
		val = AFE_SINEGEN_CON1_TIMING_32K;
		break;
	case SINEGEN_48K:
		val = AFE_SINEGEN_CON1_TIMING_48K;
		break;
	case SINEGEN_96K:
		val = AFE_SINEGEN_CON1_TIMING_96K;
		break;
	case SINEGEN_192K:
		val = AFE_SINEGEN_CON1_TIMING_192K;
		break;
	case SINEGEN_384K:
		val = AFE_SINEGEN_CON1_TIMING_384K;
		break;
	case SINEGEN_7P35K:
		val = AFE_SINEGEN_CON1_TIMING_7P35K;
		break;
	case SINEGEN_11P025K:
		val = AFE_SINEGEN_CON1_TIMING_11P025K;
		break;
	case SINEGEN_14P7K:
		val = AFE_SINEGEN_CON1_TIMING_14P7K;
		break;
	case SINEGEN_22P05K:
		val = AFE_SINEGEN_CON1_TIMING_22P05K;
		break;
	case SINEGEN_29P4K:
		val = AFE_SINEGEN_CON1_TIMING_29P4K;
		break;
	case SINEGEN_44P1K:
		val = AFE_SINEGEN_CON1_TIMING_44P1K;
		break;
	case SINEGEN_88P2K:
		val = AFE_SINEGEN_CON1_TIMING_88P2K;
		break;
	case SINEGEN_176P4K:
		val = AFE_SINEGEN_CON1_TIMING_176P4K;
		break;
	case SINEGEN_352P8K:
		val = AFE_SINEGEN_CON1_TIMING_352P8K;
		break;
	case SINEGEN_DL_1X_EN:
		val = AFE_SINEGEN_CON1_TIMING_DL_1X_EN;
		break;
	case SINEGEN_SGEN_EN:
		val = AFE_SINEGEN_CON1_TIMING_SGEN_EN;
		break;
	default:
		val = AFE_SINEGEN_CON1_TIMING_8K;
		break;
	}

	mt8512_afe_enable_main_clk(afe);

	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
			   AFE_SINEGEN_CON1_TIMING_CH1_MASK |
			   AFE_SINEGEN_CON1_TIMING_CH2_MASK,
			   AFE_SINEGEN_CON1_TIMING_CH1(val) |
			   AFE_SINEGEN_CON1_TIMING_CH2(val));

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_ul8_sinegen_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ASYS_TOP_CON, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & ASYS_TCON_UL8_USE_SINEGEN);

	return 0;
}

static int mt8512_afe_ul8_sinegen_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_UL8_USE_SINEGEN,
				   ASYS_TCON_UL8_USE_SINEGEN);
	else
		regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_UL8_USE_SINEGEN,
				   0x0);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_etdm_in_data_source_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = ETDM_IN_FROM_PAD;
	unsigned int val;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ETDM_COWORK_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Data_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_PAD)
			src = ETDM_IN_FROM_PAD;
		else if (val == ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT2)
			src = ETDM_IN_FROM_ETDM_OUT2;
	} else if (!strcmp(kcontrol->id.name, "ETDM_IN2_Data_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_PAD)
			src = ETDM_IN_FROM_PAD;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT2)
			src = ETDM_IN_FROM_ETDM_OUT2;
	}

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8512_afe_etdm_in_data_source_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8512_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Data_Source_Select")) {
		if (src == ETDM_IN_FROM_PAD)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_PAD |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_PAD);
		else if (src == ETDM_IN_FROM_ETDM_OUT2)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN1_DAT0_SEL_OUT2 |
				   ETDM_COWORK_CON1_TDM_IN1_DAT1_3_SEL_OUT2);
	} else if (!strcmp(kcontrol->id.name, "ETDM_IN2_Data_Source_Select")) {
		if (src == ETDM_IN_FROM_PAD)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_PAD |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_PAD);
		else if (src == ETDM_IN_FROM_ETDM_OUT2)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_MASK |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_DAT0_SEL_OUT2 |
				   ETDM_COWORK_CON1_TDM_IN2_DAT1_3_SEL_OUT2);
	}

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_etdm_in_slave_mode_src_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src = ETDM_IN_SLAVE_FROM_SELF;
	unsigned int val;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, ETDM_COWORK_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Slave_Mode_Source_Select")) {
		val = val & ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK;

		if (val == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_IN1_SLV)
			src = ETDM_IN_SLAVE_FROM_SELF;
		else if (val == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT2;
	} else if (!strcmp(kcontrol->id.name,
			   "ETDM_IN2_Slave_Mode_Source_Select")) {
		val = val & ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK;

		if (val == ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_IN2_SLV)
			src = ETDM_IN_SLAVE_FROM_SELF;
		else if (val == ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT2_MAS)
			src = ETDM_IN_SLAVE_FROM_ETDM_OUT2;
	}

	ucontrol->value.integer.value[0] = src;

	return 0;
}

static int mt8512_afe_etdm_in_slave_mode_src_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int src;

	src = ucontrol->value.integer.value[0];

	mt8512_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "ETDM_IN1_Slave_Mode_Source_Select")) {
		if (src == ETDM_IN_SLAVE_FROM_SELF)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON0,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_IN1_SLV);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_MASK,
				   ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS);
	} else if (!strcmp(kcontrol->id.name,
			   "ETDM_IN2_Slave_Mode_Source_Select")) {
		if (src == ETDM_IN_SLAVE_FROM_SELF)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_IN2_SLV);
		else if (src == ETDM_COWORK_CON0_TDM_IN1_SLV_SEL_OUT2_MAS)
			regmap_update_bits(afe->regmap, ETDM_COWORK_CON1,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_MASK,
				   ETDM_COWORK_CON1_TDM_IN2_SLV_SEL_OUT2_MAS);
	}

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_pcm_loopback_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, PCM_INTF_CON2, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & PCM_INTF_CON2_LPBK_EN);

	return 0;
}

static int mt8512_afe_pcm_loopback_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, PCM_INTF_CON2,
				   PCM_INTF_CON2_LPBK_EN,
				   PCM_INTF_CON2_LPBK_EN);
	else
		regmap_update_bits(afe->regmap, PCM_INTF_CON2,
				   PCM_INTF_CON2_LPBK_EN,
				   0x0);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_dmic_sinegen_enable_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_DMIC0_UL_SRC_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & DMIC_UL_CON1_SGEN_EN) ? 1 : 0;

	return 0;
}

static int mt8512_afe_dmic_sinegen_enable_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val_cfg = 0;
	unsigned int val_en = 0;
	unsigned int read_val = 0;
	unsigned int regs[] = {
		AFE_DMIC0_UL_SRC_CON1,
		AFE_DMIC1_UL_SRC_CON1,
		AFE_DMIC2_UL_SRC_CON1,
		AFE_DMIC3_UL_SRC_CON1,
	};
	int i;

	mt8512_afe_enable_main_clk(afe);

	val_cfg |= DMIC_UL_CON1_SGEN_CH2_AMP_DIV(6) |
		   DMIC_UL_CON1_SGEN_CH2_FREQ_DIV(1) |
		   DMIC_UL_CON1_SGEN_CH1_AMP_DIV(6) |
		   DMIC_UL_CON1_SGEN_CH1_FREQ_DIV(1);

	regmap_read(afe->regmap, AFE_DMIC0_UL_SRC_CON0, &read_val);

	switch (read_val & (0x7 << 17)) {
	case DMIC_UL_CON0_VOCIE_MODE_8K:
		val_cfg |= DMIC_UL_CON1_SGEN_CH2_SINE_MODE(0) |
			   DMIC_UL_CON1_SGEN_CH1_SINE_MODE(0);
		break;
	case DMIC_UL_CON0_VOCIE_MODE_16K:
		val_cfg |= DMIC_UL_CON1_SGEN_CH2_SINE_MODE(3) |
			   DMIC_UL_CON1_SGEN_CH1_SINE_MODE(3);
		break;
	case DMIC_UL_CON0_VOCIE_MODE_32K:
		val_cfg |= DMIC_UL_CON1_SGEN_CH2_SINE_MODE(6) |
			   DMIC_UL_CON1_SGEN_CH1_SINE_MODE(6);
		break;
	case DMIC_UL_CON0_VOCIE_MODE_48K:
		val_cfg |= DMIC_UL_CON1_SGEN_CH2_SINE_MODE(8) |
			   DMIC_UL_CON1_SGEN_CH1_SINE_MODE(8);
		break;
	default:
		val_cfg |= DMIC_UL_CON1_SGEN_CH2_SINE_MODE(0) |
			   DMIC_UL_CON1_SGEN_CH1_SINE_MODE(0);
		break;
	}

	val_en |= DMIC_UL_CON1_SGEN_EN;

	if (ucontrol->value.integer.value[0]) {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_cfg | val_en,
					   val_cfg | val_en);
	} else {
		for (i = 0; i < ARRAY_SIZE(regs); i++)
			regmap_update_bits(afe->regmap, regs[i],
					   val_en,
					   0x0);
	}

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_gasrc_in_sinegen_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & AFE_SINEGEN_CON1_GASRC_IN_SGEN);

	return 0;
}

static int mt8512_afe_gasrc_in_sinegen_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_IN_SGEN,
				   0x0);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_gasrc_out_sinegen_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val = 0;

	mt8512_afe_enable_main_clk(afe);

	regmap_read(afe->regmap, AFE_SINEGEN_CON1, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] =
		(val & AFE_SINEGEN_CON1_GASRC_OUT_SGEN);

	return 0;
}

static int mt8512_afe_gasrc_out_sinegen_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (ucontrol->value.integer.value[0])
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN);
	else
		regmap_update_bits(afe->regmap, AFE_SINEGEN_CON1,
				   AFE_SINEGEN_CON1_GASRC_OUT_SGEN,
				   0x0);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_spdif_in_port_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->spdif_in_data.port;

	return 0;
}

static int mt8512_afe_spdif_in_port_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int port = ucontrol->value.integer.value[0];

	mt8512_afe_handle_spdif_in_port_change(afe, port);

	return 0;
}

static int mt8512_afe_spdif_in_chs_data_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = 24;
	return 0;
}

static int mt8512_afe_spdif_in_chs_data_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i;

	if (spdif_in->port != SPDIF_IN_PORT_NONE) {
		mt8512_afe_enable_main_clk(afe);

		for (i = 0; i < 6; i++)
			regmap_read(afe->regmap,
				    AFE_SPDIFIN_CHSTS1 + i * 4,
				    &spdif_in->ch_status[i]);

		mt8512_afe_disable_main_clk(afe);
	} else {
		memset((void *)spdif_in->ch_status,
		       0xff, sizeof(spdif_in->ch_status));
	}

	memcpy((void *)ucontrol->value.bytes.data,
	       spdif_in->ch_status,
	       sizeof(spdif_in->ch_status));

	return 0;
}

static int mt8512_afe_spdif_in_rate_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	return 0;
}

static int mt8512_afe_spdif_in_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;

	if (spdif_in->port != SPDIF_IN_PORT_NONE)
		ucontrol->value.integer.value[0] = spdif_in->rate;
	else
		ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int mt8512_afe_spdif_in_iec958_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int mt8512_afe_spdif_in_iec958_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int i;

	if (spdif_in->port != SPDIF_IN_PORT_NONE) {
		mt8512_afe_enable_main_clk(afe);

		for (i = 0; i < 6; i++)
			regmap_read(afe->regmap,
				    AFE_SPDIFIN_CHSTS1 + i * 4,
				    &spdif_in->ch_status[i]);

		mt8512_afe_disable_main_clk(afe);
	} else {
		memset((void *)spdif_in->ch_status,
		       0xff, sizeof(spdif_in->ch_status));
	}

	memcpy((void *)ucontrol->value.iec958.status,
	       spdif_in->ch_status,
	       sizeof(spdif_in->ch_status));

	return 0;
}

static long long mt8512_afe_get_next_write_timestamp(struct snd_soc_card *card,
				    struct gnt_memif_info info, int dbg_on)
{
	struct snd_pcm_substream *substream;
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_pcm_runtime *runtime;
	struct mtk_base_afe *afe;
	struct mtk_base_afe_memif *memif;
	unsigned long flag;
	unsigned int pbuf_size = 0;
	int ret;
	static long long lastValidTimestamp = -1;
	long long timestamp = 0;
	uint64_t temp;
	int remain_size = 0;
	int real_remain_size;
	int pbuf_data_count;
	int rptr_offset = 0;
	int max_prefetch_size;
	int period = 0;
	int rate;
	int i = 0;


	list_for_each_entry(rtd, &card->rtd_list, list) {
		i++;
		if (!strcmp(rtd->cpu_dai->name, info.dai_name))
			break;
	}

	if (i == card->num_rtd) {
		pr_err("%s() can not find the target substream\n", __func__);
		return lastValidTimestamp;
	}

	substream = rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	afe = snd_soc_platform_get_drvdata(rtd->platform);
	memif = &afe->memif[rtd->cpu_dai->id];

	/* get target sub stream information */
	snd_pcm_stream_lock_irqsave(substream, flag);
	runtime = substream->runtime;
	if (runtime == NULL) {
		pr_err("%s() playback substream is not opened(%d)\n",
		    __func__, substream->hw_opened);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_from_regs;
	}

	if ((runtime->status->state == SNDRV_PCM_STATE_OPEN) ||
	    (runtime->status->state == SNDRV_PCM_STATE_DISCONNECTED) ||
	    (runtime->status->state == SNDRV_PCM_STATE_SUSPENDED)) {
		pr_err("%s() playback state(%d) is not right\n",
		    __func__, runtime->status->state);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_from_regs;
	}

	period = (int)(runtime->period_size);
	rate = (int)(runtime->rate);

	if (runtime->status->state == SNDRV_PCM_STATE_XRUN) {
		real_remain_size = 0;
		timestamp = (long long)mtk_timer_get_cnt(6);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		pr_err("%s() playback state is xrun state\n", __func__);
		goto output_cal;
	}

	if ((runtime->status->state != SNDRV_PCM_STATE_XRUN) &&
		  (!snd_pcm_running(substream))) {
		real_remain_size = (int)(runtime->buffer_size
			- snd_pcm_playback_avail(runtime)
			+ runtime->delay);
		timestamp = (long long)mtk_timer_get_cnt(6);
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_cal;
	}

	regmap_read(afe->regmap, info.pbuf_size_reg, &pbuf_size);
	pbuf_size = ((pbuf_size >> info.pbuf_size_shift) & info.pbuf_size_mask);
	pbuf_size = (info.pbuf_size_unit) << pbuf_size;
	max_prefetch_size = bytes_to_frames(runtime, pbuf_size);

	timestamp = (long long)mtk_timer_get_cnt(6);
	regmap_read(afe->regmap, info.pbuf_mon_reg, &pbuf_data_count);
	pbuf_data_count =
		((pbuf_data_count >> info.pbuf_mon_shift) & info.pbuf_mon_mask);
	pbuf_data_count = pbuf_data_count * info.pbuf_mon_unit;
	ret = snd_pcm_update_hw_ptr(substream);

	if (ret < 0) {
		real_remain_size = 0;
		snd_pcm_stream_unlock_irqrestore(substream, flag);
		goto output_cal;
	}

	remain_size = (int)(runtime->buffer_size
		- snd_pcm_playback_avail(runtime) + runtime->delay);

	rptr_offset = (int)(runtime->status->hw_ptr % runtime->period_size);
	snd_pcm_stream_unlock_irqrestore(substream, flag);

	real_remain_size = remain_size
		+ bytes_to_frames(runtime, pbuf_data_count);

	/* unormal case check */
	if ((real_remain_size >
		(remain_size + max_prefetch_size + info.cur_tolerance))
		|| (real_remain_size < (remain_size - info.cur_tolerance))) {
		if (dbg_on)
			pr_err("%s() invalid real_remain_size(%d), pbuf_data_count(%d), rptr_offset(%d), periods(%d), remain(%d)\n",
			    __func__, real_remain_size, pbuf_data_count,
			    rptr_offset, period, remain_size);
		real_remain_size = remain_size;
	}

output_cal:

	temp = (13000000LL) * (uint64_t)real_remain_size;
	do_div(temp, (uint64_t)rate);
	timestamp += (long long)temp;
	lastValidTimestamp = timestamp;
	return timestamp;

output_from_regs:

	timestamp = (long long)mtk_timer_get_cnt(6);
	lastValidTimestamp = timestamp;
	return timestamp;
}

static int mt8512_afe_dlm_timestamp_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct snd_soc_card *card = platform->component.card;

	ucontrol->value.integer64.value[0] =
	    mt8512_afe_get_next_write_timestamp(card, gnt_memif_data[0], 1);
	return 0;
}

static int mt8512_afe_dl3_timestamp_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct snd_soc_card *card = platform->component.card;

	ucontrol->value.integer64.value[0] =
	    mt8512_afe_get_next_write_timestamp(card, gnt_memif_data[1], 1);
	return 0;
}

static int mt8512_afe_dl2_force_interconn_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);
	unsigned int val;

	mt8512_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O04"))
		regmap_read(afe->regmap, AFE_CONN4_1, &val);
	else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O05"))
		regmap_read(afe->regmap, AFE_CONN5_1, &val);
	else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O30"))
		regmap_read(afe->regmap, AFE_CONN30_1, &val);
	else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O32"))
		regmap_read(afe->regmap, AFE_CONN32_1, &val);
	else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O33"))
		regmap_read(afe->regmap, AFE_CONN33_1, &val);

	mt8512_afe_disable_main_clk(afe);

	ucontrol->value.integer.value[0] = (val & 0x100) >> 8;

	return 0;
}

static int mt8512_afe_dl2_force_interconn_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	mt8512_afe_enable_main_clk(afe);

	if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O04")) {
		if (ucontrol->value.integer.value[0])
			regmap_update_bits(afe->regmap,
				AFE_CONN4_1,
				0x100,
				0x100);
		else
			regmap_update_bits(afe->regmap,
				AFE_CONN4_1,
				0x100,
				0x0);
	} else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O05")) {
		if (ucontrol->value.integer.value[0])
			regmap_update_bits(afe->regmap,
				AFE_CONN5_1,
				0x100,
				0x100);
		else
			regmap_update_bits(afe->regmap,
				AFE_CONN5_1,
				0x100,
				0x0);
	} else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O30")) {
		if (ucontrol->value.integer.value[0])
			regmap_update_bits(afe->regmap,
				AFE_CONN30_1,
				0x100,
				0x100);
		else
			regmap_update_bits(afe->regmap,
				AFE_CONN30_1,
				0x100,
				0x0);
	} else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O32")) {
		if (ucontrol->value.integer.value[0])
			regmap_update_bits(afe->regmap,
				AFE_CONN32_1,
				0x100,
				0x100);
		else
			regmap_update_bits(afe->regmap,
				AFE_CONN32_1,
				0x100,
				0x0);
	} else if (!strcmp(kcontrol->id.name, "Force_Connect_I40_O33")) {
		if (ucontrol->value.integer.value[0])
			regmap_update_bits(afe->regmap,
				AFE_CONN33_1,
				0x100,
				0x100);
		else
			regmap_update_bits(afe->regmap,
				AFE_CONN33_1,
				0x100,
				0x0);
	}

	mt8512_afe_disable_main_clk(afe);

	return 0;
}


#define SND_SOC_CTL_RO(xname, xhandler_info, xhandler_get) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.access = SNDRV_CTL_ELEM_ACCESS_READ | \
		  SNDRV_CTL_ELEM_ACCESS_VOLATILE, \
	.info = xhandler_info, .get = xhandler_get }

static const struct snd_kcontrol_new mt8512_afe_controls[] = {
	SOC_SINGLE_BOOL_EXT("CM0_Bypass_Switch",
			    0,
			    mt8512_afe_cm_bypass_get,
			    mt8512_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("CM1_Bypass_Switch",
			    0,
			    mt8512_afe_cm_bypass_get,
			    mt8512_afe_cm_bypass_put),
	SOC_SINGLE_BOOL_EXT("SineGen_Enable_Switch",
			    0,
			    mt8512_afe_singen_enable_get,
			    mt8512_afe_singen_enable_put),
	SOC_ENUM_EXT("SineGen_Loopback_Mode_Select",
		     mt8512_afe_soc_enums[CTRL_SINEGEN_LOOPBACK_MODE],
		     mt8512_afe_sinegen_loopback_mode_get,
		     mt8512_afe_sinegen_loopback_mode_put),
	SOC_ENUM_EXT("SineGen_Timing_Select",
		     mt8512_afe_soc_enums[CTRL_SINEGEN_TIMING],
		     mt8512_afe_sinegen_timing_get,
		     mt8512_afe_sinegen_timing_put),
	SOC_SINGLE_RANGE("SineGen_Amp_Div_Ch1",
			 AFE_SINEGEN_CON0, 5, 0, 7, 0),
	SOC_SINGLE_RANGE("SineGen_Amp_Div_Ch2",
			 AFE_SINEGEN_CON0, 17, 0, 7, 0),
	SOC_SINGLE_RANGE("SineGen_Freq_Div_Ch1",
			 AFE_SINEGEN_CON0, 0, 0, 31, 0),
	SOC_SINGLE_RANGE("SineGen_Freq_Div_Ch2",
			 AFE_SINEGEN_CON0, 12, 0, 31, 0),
	SOC_SINGLE_BOOL_EXT("UL8_SineGen_Select",
			    0,
			    mt8512_afe_ul8_sinegen_get,
			    mt8512_afe_ul8_sinegen_put),
	SOC_ENUM_EXT("ETDM_IN1_Data_Source_Select",
		     mt8512_afe_soc_enums[CTRL_ETDM_IN_DATA_SRC],
		     mt8512_afe_etdm_in_data_source_get,
		     mt8512_afe_etdm_in_data_source_put),
	SOC_ENUM_EXT("ETDM_IN2_Data_Source_Select",
		     mt8512_afe_soc_enums[CTRL_ETDM_IN_DATA_SRC],
		     mt8512_afe_etdm_in_data_source_get,
		     mt8512_afe_etdm_in_data_source_put),
	SOC_ENUM_EXT("ETDM_IN1_Slave_Mode_Source_Select",
		     mt8512_afe_soc_enums[CTRL_ETDM_IN_SLAVE_MODE_SRC],
		     mt8512_afe_etdm_in_slave_mode_src_get,
		     mt8512_afe_etdm_in_slave_mode_src_put),
	SOC_ENUM_EXT("ETDM_IN2_Slave_Mode_Source_Select",
		     mt8512_afe_soc_enums[CTRL_ETDM_IN_SLAVE_MODE_SRC],
		     mt8512_afe_etdm_in_slave_mode_src_get,
		     mt8512_afe_etdm_in_slave_mode_src_put),
	SOC_SINGLE_BOOL_EXT("PCM_Tx2Rx_Loopback_Switch",
			    0,
			    mt8512_afe_pcm_loopback_get,
			    mt8512_afe_pcm_loopback_put),
	SOC_SINGLE_BOOL_EXT("DMIC_SineGen_Enable_Switch",
		     0,
		     mt8512_afe_dmic_sinegen_enable_get,
		     mt8512_afe_dmic_sinegen_enable_put),
	SOC_SINGLE_BOOL_EXT("GASRC_In_SineGen_Select",
			    0,
			    mt8512_afe_gasrc_in_sinegen_get,
			    mt8512_afe_gasrc_in_sinegen_put),
	SOC_SINGLE_BOOL_EXT("GASRC_Out_SineGen_Select",
			    0,
			    mt8512_afe_gasrc_out_sinegen_get,
			    mt8512_afe_gasrc_out_sinegen_put),
	SOC_ENUM("Multi_In_Bck_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_BCK_SEL]),
	SOC_ENUM("Multi_In_Lrck_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_LRCK_SEL]),
	SOC_ENUM("Multi_In_Sdata0_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_SDATA0_SEL]),
	SOC_ENUM("Multi_In_Sdata1_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_SDATA1_SEL]),
	SOC_ENUM("Multi_In_Sdata2_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_SDATA2_SEL]),
	SOC_ENUM("Multi_In_Sdata3_Select",
		 mt8512_afe_soc_enums[CTRL_MULTI_IN_SDATA3_SEL]),
	SOC_ENUM_EXT("Spdif_In_Port_Select",
		     mt8512_afe_soc_enums[CTRL_SPDIF_IN_PORT_SEL],
		     mt8512_afe_spdif_in_port_get,
		     mt8512_afe_spdif_in_port_put),
	SND_SOC_CTL_RO("Spdif_In_Channel_Status_Data",
		       mt8512_afe_spdif_in_chs_data_info,
		       mt8512_afe_spdif_in_chs_data_get),
	SND_SOC_CTL_RO(SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		       mt8512_afe_spdif_in_iec958_info,
		       mt8512_afe_spdif_in_iec958_get),
	SND_SOC_CTL_RO("Spdif_In_Rate",
		       mt8512_afe_spdif_in_rate_info,
		       mt8512_afe_spdif_in_rate_get),
	SOC_SINGLE("Spdif_Out_to_Spdif_In_Loopback_Switch",
		   AFE_SPDIFIN_CFG1, 14, 1, 0),
	SND_SOC_BYTES_EXT("DLM Timestamp",
			    8,
			    mt8512_afe_dlm_timestamp_get,
			    NULL),
	SND_SOC_BYTES_EXT("DL3 Timestamp",
			    8,
			    mt8512_afe_dl3_timestamp_get,
			    NULL),
	SOC_SINGLE_BOOL_EXT("Force_Connect_I40_O04",
			    0,
			    mt8512_afe_dl2_force_interconn_get,
			    mt8512_afe_dl2_force_interconn_put),
	SOC_SINGLE_BOOL_EXT("Force_Connect_I40_O05",
			    0,
			    mt8512_afe_dl2_force_interconn_get,
			    mt8512_afe_dl2_force_interconn_put),
	SOC_SINGLE_BOOL_EXT("Force_Connect_I40_O30",
			    0,
			    mt8512_afe_dl2_force_interconn_get,
			    mt8512_afe_dl2_force_interconn_put),
	SOC_SINGLE_BOOL_EXT("Force_Connect_I40_O32",
			    0,
			    mt8512_afe_dl2_force_interconn_get,
			    mt8512_afe_dl2_force_interconn_put),
	SOC_SINGLE_BOOL_EXT("Force_Connect_I40_O33",
			    0,
			    mt8512_afe_dl2_force_interconn_get,
			    mt8512_afe_dl2_force_interconn_put),
};

int mt8512_afe_add_controls(struct snd_soc_platform *platform)
{
	return snd_soc_add_platform_controls(platform, mt8512_afe_controls,
					     ARRAY_SIZE(mt8512_afe_controls));
}

