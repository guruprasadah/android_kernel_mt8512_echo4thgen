/*
 * mt8512-afe-utils.c  --  Mediatek 8512 audio utility
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

#include "mt8512-afe-utils.h"
#include "mt8512-afe-common.h"
#include "mt8512-reg.h"
#include "../common/mtk-base-afe.h"
#include <linux/device.h>
#include <linux/pm_qos.h>
static struct pm_qos_request qos_request = { {0} };

#ifdef COMMON_CLOCK_FRAMEWORK_API
static const char *aud_clks[MT8512_CLK_NUM] = {
	[MT8512_CLK_TOP_AUD_26M] = "top_aud_26m",
	[MT8512_CLK_TOP_AUD_BUS] = "top_aud_intbus",
	[MT8512_CLK_FA1SYS] = "fa1sys",
	[MT8512_CLK_FA2SYS] = "fa2sys",
	[MT8512_CLK_FAPLL1] = "hapll1",
	[MT8512_CLK_FAPLL2] = "hapll2",
	[MT8512_CLK_AUD1] = "aud1",
	[MT8512_CLK_AUD2] = "aud2",
	[MT8512_CLK_FASM_L] = "fasm_l",
	[MT8512_CLK_FASM_M] = "fasm_m",
	[MT8512_CLK_FASM_H] = "fasm_h",
	[MT8512_CLK_SPDIF_IN] = "spdif_in",
	[MT8512_CLK_I2SIN_M_SEL] =  "i2si1_m_sel",
	[MT8512_CLK_TDMIN_M_SEL] =  "tdmin_m_sel",
	[MT8512_CLK_I2SOUT_M_SEL] =  "i2so1_m_sel",
	[MT8512_CLK_APLL12_DIV0] = "apll12_div0",
	[MT8512_CLK_APLL12_DIV1] = "apll12_div1",
	[MT8512_CLK_APLL12_DIV2] = "apll12_div2",
	[MT8512_CLK_I2SIN_MCK] =  "i2si1_mck",
	[MT8512_CLK_TDMIN_MCK] =  "tdmin_mck",
	[MT8512_CLK_I2SOUT_MCK] =  "i2so1_mck",
	[MT8512_CLK_CLK26M] =  "top_clk26m_clk",
	[MT8512_CLK_AUD_SYSPLL1_D4] = "syspll1_d4",
	[MT8512_CLK_AUDIO_CG] = "audio_cg",
	[MT8512_CLK_AUD_26M_CG] = "aud_26m_cg",
};
#endif

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

int mt8512_afe_init_audio_clk(struct mtk_base_afe *afe)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		afe_priv->clocks[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clocks[i])) {
			dev_info(afe->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i]);
			return PTR_ERR(afe_priv->clocks[i]);
		}
	}
#endif
	return 0;
}

int mt8512_afe_enable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk) {
		ret = clk_prepare_enable(clk);
		if (ret) {
			dev_info(afe->dev, "Failed to enable clk\n");
			return ret;
		}
	} else {
		dev_info(afe->dev, "null clk\n");
	}
#endif
	return 0;
}

void mt8512_afe_disable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	if (clk)
		clk_disable_unprepare(clk);
	else
		dev_info(afe->dev, "null clk\n");
#endif
}

int mt8512_afe_set_clk_rate(struct mtk_base_afe *afe, struct clk *clk,
			    unsigned int rate)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk) {
		ret = clk_set_rate(clk, rate);
		if (ret) {
			dev_info(afe->dev, "Failed to set rate\n");
			return ret;
		}
	}
#endif
	return 0;
}

int mt8512_afe_set_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
			      struct clk *parent)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (clk && parent) {
		ret = clk_set_parent(clk, parent);
		if (ret) {
			dev_info(afe->dev, "Failed to set parent\n");
			return ret;
		}
	}
#endif
	return 0;
}

static unsigned int get_top_cg_reg(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8512_TOP_CG_AFE:
	case MT8512_TOP_CG_APLL:
	case MT8512_TOP_CG_APLL2:
	case MT8512_TOP_CG_DAC:
	case MT8512_TOP_CG_DAC_PREDIS:
	case MT8512_TOP_CG_ADC:
	case MT8512_TOP_CG_TML:
	case MT8512_TOP_CG_UPLINK_TML:
	case MT8512_TOP_CG_APLL_TUNER:
	case MT8512_TOP_CG_APLL2_TUNER:
	case MT8512_TOP_CG_SPIDFIN_TUNER_APLL:
		return AUDIO_TOP_CON0;
	case MT8512_TOP_CG_A1SYS_HOPPING:
		return AUDIO_TOP_CON1;
	case MT8512_TOP_CG_I2S_IN:
	case MT8512_TOP_CG_TDM_IN:
	case MT8512_TOP_CG_I2S_OUT:
	case MT8512_TOP_CG_ASRC11:
	case MT8512_TOP_CG_ASRC12:
	case MT8512_TOP_CG_A1SYS:
	case MT8512_TOP_CG_A2SYS:
	case MT8512_TOP_CG_AFE_CONN:
	case MT8512_TOP_CG_PCMIF:
	case MT8512_TOP_CG_GASRC0:
	case MT8512_TOP_CG_GASRC1:
	case MT8512_TOP_CG_GASRC2:
	case MT8512_TOP_CG_GASRC3:
	case MT8512_TOP_CG_MULTI_IN:
	case MT8512_TOP_CG_INTDIR:
	case MT8512_TOP_CG_DL_ASRC:
		return AUDIO_TOP_CON4;
	case MT8512_TOP_CG_DMIC0:
	case MT8512_TOP_CG_DMIC1:
	case MT8512_TOP_CG_DMIC2:
	case MT8512_TOP_CG_DMIC3:
		return PWR2_TOP_CON0;
	case MT8512_TOP_CG_A1SYS_TIMING:
	case MT8512_TOP_CG_A2SYS_TIMING:
	case MT8512_TOP_CG_26M_TIMING:
	case MT8512_TOP_CG_LP_MODE:
		return ASYS_TOP_CON;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_mask(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8512_TOP_CG_AFE:
		return AUD_TCON0_PDN_AFE;
	case MT8512_TOP_CG_APLL:
		return AUD_TCON0_PDN_APLL;
	case MT8512_TOP_CG_APLL2:
		return AUD_TCON0_PDN_APLL2;
	case MT8512_TOP_CG_DAC:
		return AUD_TCON0_PDN_DAC;
	case MT8512_TOP_CG_DAC_PREDIS:
		return AUD_TCON0_PDN_DAC_PREDIS;
	case MT8512_TOP_CG_ADC:
		return AUD_TCON0_PDN_ADC;
	case MT8512_TOP_CG_TML:
		return AUD_TCON0_PDN_TML;
	case MT8512_TOP_CG_UPLINK_TML:
		return AUD_TCON0_PDN_UPLINK_TML;
	case MT8512_TOP_CG_APLL_TUNER:
		return AUD_TCON0_PDN_APLL_TUNER;
	case MT8512_TOP_CG_APLL2_TUNER:
		return AUD_TCON0_PDN_APLL2_TUNER;
	case MT8512_TOP_CG_SPIDFIN_TUNER_APLL:
		return AUD_TCON0_PDN_SPDIFIN_TUNER_APLL_CK;
	case MT8512_TOP_CG_A1SYS_HOPPING:
		return AUD_TCON1_PDN_A1SYS_HOPPING_CK;
	case MT8512_TOP_CG_I2S_IN:
		return AUD_TCON4_PDN_I2S_IN;
	case MT8512_TOP_CG_TDM_IN:
		return AUD_TCON4_PDN_TDM_IN;
	case MT8512_TOP_CG_I2S_OUT:
		return AUD_TCON4_PDN_I2S_OUT;
	case MT8512_TOP_CG_ASRC11:
		return AUD_TCON4_PDN_ASRC11;
	case MT8512_TOP_CG_ASRC12:
		return AUD_TCON4_PDN_ASRC12;
	case MT8512_TOP_CG_A1SYS:
		return AUD_TCON4_PDN_A1SYS;
	case MT8512_TOP_CG_A2SYS:
		return AUD_TCON4_PDN_A2SYS;
	case MT8512_TOP_CG_DL_ASRC:
		return AUD_TCON4_PDN_DL_ASRC;
	case MT8512_TOP_CG_MULTI_IN:
		return AUD_TCON4_PDN_MULTI_IN;
	case MT8512_TOP_CG_INTDIR:
		return AUD_TCON4_PDN_INTDIR;
	case MT8512_TOP_CG_AFE_CONN:
		return AUD_TCON4_PDN_AFE_CONN;
	case MT8512_TOP_CG_PCMIF:
		return AUD_TCON4_PDN_PCMIF;
	case MT8512_TOP_CG_GASRC0:
		return AUD_TCON4_PDN_GASRC0;
	case MT8512_TOP_CG_GASRC1:
		return AUD_TCON4_PDN_GASRC1;
	case MT8512_TOP_CG_GASRC2:
		return AUD_TCON4_PDN_GASRC2;
	case MT8512_TOP_CG_GASRC3:
		return AUD_TCON4_PDN_GASRC3;
	case MT8512_TOP_CG_DMIC0:
		return PWR2_TOP_CON_PDN_DMIC0;
	case MT8512_TOP_CG_DMIC1:
		return PWR2_TOP_CON_PDN_DMIC1;
	case MT8512_TOP_CG_DMIC2:
		return PWR2_TOP_CON_PDN_DMIC2;
	case MT8512_TOP_CG_DMIC3:
		return PWR2_TOP_CON_PDN_DMIC3;
	case MT8512_TOP_CG_A1SYS_TIMING:
		return ASYS_TCON_A1SYS_TIMING_ON;
	case MT8512_TOP_CG_A2SYS_TIMING:
		return ASYS_TCON_A2SYS_TIMING_ON;
	case MT8512_TOP_CG_26M_TIMING:
		return ASYS_TCON_LP_26M_ENGEN_ON;
	case MT8512_TOP_CG_LP_MODE:
		return ASYS_TCON_LP_MODE_ON;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_on_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8512_TOP_CG_A1SYS_TIMING:
	case MT8512_TOP_CG_A2SYS_TIMING:
	case MT8512_TOP_CG_26M_TIMING:
	case MT8512_TOP_CG_LP_MODE:
		return get_top_cg_mask(cg_type);
	default:
		return 0;
	}
}

static unsigned int get_top_cg_off_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8512_TOP_CG_A1SYS_TIMING:
	case MT8512_TOP_CG_A2SYS_TIMING:
	case MT8512_TOP_CG_26M_TIMING:
	case MT8512_TOP_CG_LP_MODE:
		return 0;
	default:
		return get_top_cg_mask(cg_type);
	}
}

int mt8512_afe_enable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_on_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]++;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8512_afe_disable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_off_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]--;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 0)
		need_update = true;
	else if (afe_priv->top_cg_ref_cnt[cg_type] < 0)
		afe_priv->top_cg_ref_cnt[cg_type] = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8512_afe_is_top_cg_on(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	return afe_priv->top_cg_ref_cnt[cg_type];
}

int mt8512_afe_enable_main_clk(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s\n", __func__);

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FA1SYS]);

	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A2SYS);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS_TIMING);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A2SYS_TIMING);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_26M_TIMING);

	mt8512_afe_enable_afe_on(afe);

	return 0;
}

int mt8512_afe_disable_main_clk(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s\n", __func__);

	mt8512_afe_disable_afe_on(afe);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_26M_TIMING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A2SYS_TIMING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS_TIMING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A2SYS);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS);

	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_FA1SYS]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);

	return 0;
}

int mt8512_afe_enable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FA1SYS]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	return 0;
}

int mt8512_afe_disable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_FA1SYS]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	return 0;
}

int mt8512_afe_enable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt++;
	if (afe_priv->afe_on_ref_cnt == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	return 0;
}

int mt8512_afe_disable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt--;
	if (afe_priv->afe_on_ref_cnt == 0)
		need_update = true;
	else if (afe_priv->afe_on_ref_cnt < 0)
		afe_priv->afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	return 0;
}

static const struct mtk_base_irq_data *mt8512_get_irq_data(
	struct mtk_base_afe *afe, unsigned int irq_id) {
	int i;

	for (i = 0; i < afe->irqs_size; i++) {
		if (irq_id == afe->irqs->irq_data[i].id)
			return &afe->irqs->irq_data[i];
	}

	return NULL;
}

int mt8512_afe_enable_irq(struct mtk_base_afe *afe, unsigned int irq_id)
{
	const struct mtk_base_irq_data *data;

	data = mt8512_get_irq_data(afe, irq_id);
	if (!data)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
			   data->irq_en_reg,
			   1 << data->irq_en_shift,
			   1 << data->irq_en_shift);

	return 0;
}

int mt8512_afe_disable_irq(struct mtk_base_afe *afe, unsigned int irq_id)
{
	const struct mtk_base_irq_data *data;

	data = mt8512_get_irq_data(afe, irq_id);
	if (!data)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
			   data->irq_en_reg,
			   1 << data->irq_en_shift,
			   0 << data->irq_en_shift);

	return 0;
}

static int mt8512_afe_enable_spdif_in(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int port_sel;
	unsigned int mux_sel;
	unsigned int port = afe_priv->spdif_in_data.port;

	switch (port) {
	case SPDIF_IN_PORT_OPT:
	case SPDIF_IN_PORT_COAXIAL:
	case SPDIF_IN_PORT_ARC:
		mux_sel = afe_priv->spdif_in_data.ports_mux[port];
		break;
	case SPDIF_IN_PORT_NONE:
	default:
		dev_notice(afe->dev, "%s unexpected port %u\n",
			   __func__, port);
		return -EINVAL;
	}

	switch (mux_sel) {
	case SPDIF_IN_MUX_0:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_OPTICAL;
		break;
	case SPDIF_IN_MUX_1:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_COAXIAL;
		break;
	case SPDIF_IN_MUX_2:
		port_sel = AFE_SPDIFIN_INT_EXT_SEL_ARC;
		break;
	default:
		dev_notice(afe->dev, "%s unexpected mux_sel %u\n",
			   __func__, mux_sel);
		return -EINVAL;
	}

	mt8512_afe_enable_main_clk(afe);

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_SPDIF_IN]);

	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_INTDIR);

	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO, 0x00877986);
	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO_2, 0x006596e8);
	regmap_write(afe->regmap, SPDIFIN_FREQ_INFO_3, 0x000005a5);
	regmap_write(afe->regmap, AFE_SPDIFIN_BR, 0x00039000);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_INT_EXT2,
			   SPDIFIN_594MODE_MASK,
			   SPDIFIN_594MODE_EN);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG1,
			   AFE_SPDIFIN_CFG1_SET_MASK,
			   AFE_SPDIFIN_CFG1_INT_BITS |
			   AFE_SPDIFIN_CFG1_SEL_BCK_SPDIFIN |
			   AFE_SPDIFIN_CFG1_FIFOSTART_5POINTS |
			   AFE_SPDIFIN_CFG1_SEL_DEC0_CLK_EN |
			   AFE_SPDIFIN_CFG1_SEL_DEC0_DATA_EN);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_INT_EXT,
			   AFE_SPDIFIN_INT_EXT_SET_MASK,
			   AFE_SPDIFIN_INT_EXT_DATALAT_ERR_EN |
			   port_sel);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_SET_MASK,
			   AFE_SPDIFIN_CFG0_FLIP |
			   AFE_SPDIFIN_CFG0_GMAT_BC_256_CYCLES |
			   AFE_SPDIFIN_CFG0_INT_EN |
			   AFE_SPDIFIN_CFG0_DE_CNT(4) |
			   AFE_SPDIFIN_CFG0_DE_SEL_CNT |
			   AFE_SPDIFIN_CFG0_MAX_LEN_NUM(237));

	mt8512_afe_enable_irq(afe, MT8512_AFE_IRQ2);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_EN,
			   AFE_SPDIFIN_CFG0_EN);

	return 0;
}

static int mt8512_afe_disable_spdif_in(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_EN,
			   0x0);

	mt8512_afe_disable_irq(afe, MT8512_AFE_IRQ2);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_INTDIR);

	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_SPDIF_IN]);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

int mt8512_afe_handle_spdif_in_port_change(struct mtk_base_afe *afe,
	unsigned int port)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	int ret = 0;

	if (port != SPDIF_IN_PORT_NONE &&
	    port != SPDIF_IN_PORT_OPT &&
	    port != SPDIF_IN_PORT_COAXIAL &&
	    port != SPDIF_IN_PORT_ARC)
		return -EINVAL;

	if (spdif_in->port != port) {
		if (spdif_in->port != SPDIF_IN_PORT_NONE)
			ret = mt8512_afe_disable_spdif_in(afe);

		spdif_in->port = port;

		if (spdif_in->port != SPDIF_IN_PORT_NONE)
			ret = mt8512_afe_enable_spdif_in(afe);
	}

	return ret;
}

int mt8512_afe_block_dpidle(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int cpu_dump_latency = 1200;

	dev_dbg(afe->dev, "%s, block_dpidle_ref_cnt = %d\n",
		__func__, afe_priv->block_dpidle_ref_cnt);

	mutex_lock(&afe_priv->block_dpidle_mutex);
	if (afe_priv->block_dpidle_ref_cnt == 0)
		pm_qos_add_request(&qos_request,
			PM_QOS_CPU_DMA_LATENCY, cpu_dump_latency);
	afe_priv->block_dpidle_ref_cnt++;
	mutex_unlock(&afe_priv->block_dpidle_mutex);
	return 0;
}

int mt8512_afe_unblock_dpidle(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s, block_dpidle_ref_cnt = %d\n",
		__func__, afe_priv->block_dpidle_ref_cnt);

	mutex_lock(&afe_priv->block_dpidle_mutex);
	afe_priv->block_dpidle_ref_cnt--;
	if (afe_priv->block_dpidle_ref_cnt == 0)
		pm_qos_remove_request(&qos_request);
	else if (afe_priv->block_dpidle_ref_cnt < 0)
		afe_priv->block_dpidle_ref_cnt = 0;

	mutex_unlock(&afe_priv->block_dpidle_mutex);
	return 0;
}

int mt8512_afe_enable_apll_tuner_cfg(struct mtk_base_afe *afe,
	unsigned int apll)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	mutex_lock(&afe_priv->afe_clk_mutex);

	afe_priv->apll_tuner_ref_cnt[apll]++;
	if (afe_priv->apll_tuner_ref_cnt[apll] != 1) {
		mutex_unlock(&afe_priv->afe_clk_mutex);
		return 0;
	}

	if (apll == MT8512_AFE_APLL1) {
		regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			AFE_APLL_TUNER_CFG_MASK, 0x412);
		regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			AFE_APLL_TUNER_CFG_EN_MASK, 0x1);
	} else {
		regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG1,
			AFE_APLL_TUNER_CFG1_MASK, 0x414);
		regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG1,
			AFE_APLL_TUNER_CFG1_EN_MASK, 0x1);
	}

	mutex_unlock(&afe_priv->afe_clk_mutex);
	return 0;
}

int mt8512_afe_disable_apll_tuner_cfg(struct mtk_base_afe *afe,
	unsigned int apll)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	mutex_lock(&afe_priv->afe_clk_mutex);

	afe_priv->apll_tuner_ref_cnt[apll]--;
	if (afe_priv->apll_tuner_ref_cnt[apll] == 0) {
		if (apll == MT8512_AFE_APLL1)
			regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
				AFE_APLL_TUNER_CFG_EN_MASK, 0x0);
		else
			regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG1,
				AFE_APLL_TUNER_CFG1_EN_MASK, 0x0);

	} else if (afe_priv->apll_tuner_ref_cnt[apll] < 0) {
		afe_priv->apll_tuner_ref_cnt[apll] = 0;
	}

	mutex_unlock(&afe_priv->afe_clk_mutex);
	return 0;
}

int mt8512_afe_enable_apll_associated_cfg(struct mtk_base_afe *afe,
	unsigned int apll)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	if (apll == MT8512_AFE_APLL1) {
		clk_prepare_enable(afe_priv->clocks[MT8512_CLK_FAPLL1]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_APLL);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_APLL_TUNER);
		mt8512_afe_enable_apll_tuner_cfg(afe, MT8512_AFE_APLL1);
	} else {
		clk_prepare_enable(afe_priv->clocks[MT8512_CLK_FAPLL2]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_APLL2);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_APLL2_TUNER);
		mt8512_afe_enable_apll_tuner_cfg(afe, MT8512_AFE_APLL2);
	}

	return 0;
}

int mt8512_afe_disable_apll_associated_cfg(struct mtk_base_afe *afe,
	unsigned int apll)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	if (apll == MT8512_AFE_APLL1) {

		mt8512_afe_disable_apll_tuner_cfg(afe, MT8512_AFE_APLL1);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_APLL_TUNER);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_APLL);
		clk_disable_unprepare(afe_priv->clocks[MT8512_CLK_FAPLL1]);
	} else {
		mt8512_afe_disable_apll_tuner_cfg(afe, MT8512_AFE_APLL2);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_APLL2_TUNER);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_APLL2);
		clk_disable_unprepare(afe_priv->clocks[MT8512_CLK_FAPLL2]);
	}

	return 0;
}

int mt8512_afe_dump_all_registers(struct mtk_base_afe *afe)
{
	int i;
	int read_val;

	for (i = 0; i < ARRAY_SIZE(etdm_dump_regs); i++) {
		regmap_read(afe->regmap, etdm_dump_regs[i].offset, &read_val);
		dev_info(afe->dev, "%s = 0x%08x\n", etdm_dump_regs[i].name,
			read_val);
	}

	for (i = 0; i < ARRAY_SIZE(memif_dump_regs); i++) {
		regmap_read(afe->regmap, memif_dump_regs[i].offset, &read_val);
		dev_info(afe->dev, "%s = 0x%08x\n", memif_dump_regs[i].name,
			read_val);
	}

	return 0;
}


int mt8512_afe_force_etdm2_out_enable(struct snd_soc_platform *plat, int enable)
{
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(plat);

	pr_info("%s()+ enable %d\n", __func__, enable);

	if (!afe) {
		pr_err("%s() Invalid platform struct!\n", __func__);
		return -1;
	}

	mt8512_afe_enable_main_clk(afe);

	if (enable)
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   0x1, 0x1);
	else
		regmap_update_bits(afe->regmap, ETDM_OUT2_CON0,
				   0x1, 0x0);

	mt8512_afe_disable_main_clk(afe);

	pr_info("%s()-\n", __func__);

	return 0;
}
