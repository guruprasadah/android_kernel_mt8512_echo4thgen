// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020, Maxim Integrated

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <sound/tlv.h>
#include "max98396.h"

#define MAX_CHANNELS 2

static struct reg_default max98396_reg[] = {
	{MAX98396_R2000_SW_RESET, 0x00},
	{MAX98396_R2001_INT_RAW1, 0x00},
	{MAX98396_R2002_INT_RAW2, 0x00},
	{MAX98396_R2003_INT_RAW3, 0x00},
	{MAX98396_R2004_INT_RAW4, 0x00},
	{MAX98396_R2006_INT_STATE1, 0x00},
	{MAX98396_R2007_INT_STATE2, 0x00},
	{MAX98396_R2008_INT_STATE3, 0x00},
	{MAX98396_R2009_INT_STATE4, 0x00},
	{MAX98396_R200B_INT_FLAG1, 0x00},
	{MAX98396_R200C_INT_FLAG2, 0x00},
	{MAX98396_R200D_INT_FLAG3, 0x00},
	{MAX98396_R200E_INT_FLAG4, 0x00},
	{MAX98396_R2010_INT_EN1, 0x02},
	{MAX98396_R2011_INT_EN2, 0x00},
	{MAX98396_R2012_INT_EN3, 0x00},
	{MAX98396_R2013_INT_EN4, 0x00},
	{MAX98396_R2015_INT_FLAG_CLR1, 0x00},
	{MAX98396_R2016_INT_FLAG_CLR2, 0x00},
	{MAX98396_R2017_INT_FLAG_CLR3, 0x00},
	{MAX98396_R2018_INT_FLAG_CLR4, 0x00},
	{MAX98396_R201F_IRQ_CTRL, 0x00},
	{MAX98396_R2020_THERM_WARN_THRESH, 0x46},
	{MAX98396_R2021_THERM_WARN_THRESH2, 0x46},
	{MAX98396_R2022_THERM_SHDN_THRESH, 0x64},
	{MAX98396_R2023_THERM_HYSTERESIS, 0x02},
	{MAX98396_R2024_THERM_FOLDBACK_SET, 0xC5},
	{MAX98396_R2027_THERM_FOLDBACK_EN, 0x01},
	{MAX98396_R2030_NOISE_GATE_IDLE_MODE_CTRL, 0x32},
	{MAX98396_R2033_NOISE_GATE_IDLE_MODE_EN, 0x00},
	{MAX98396_R2038_CLK_MON_CTRL, 0x00},
	{MAX98396_R2039_DATA_MON_CTRL, 0x00},
	{MAX98396_R203F_ENABLE_CTRLS, 0x0F},
	{MAX98396_R2040_PIN_CFG, 0x55},
	{MAX98396_R2041_PCM_MODE_CFG, 0xC0},
	{MAX98396_R2042_PCM_CLK_SETUP, 0x04},
	{MAX98396_R2043_PCM_SR_SETUP, 0x88},
	{MAX98396_R2044_PCM_TX_CTRL_1, 0x00},
	{MAX98396_R2045_PCM_TX_CTRL_2, 0x00},
	{MAX98396_R2046_PCM_TX_CTRL_3, 0x00},
	{MAX98396_R2047_PCM_TX_CTRL_4, 0x00},
	{MAX98396_R2048_PCM_TX_CTRL_5, 0x00},
	{MAX98396_R2049_PCM_TX_CTRL_6, 0x00},
	{MAX98396_R204A_PCM_TX_CTRL_7, 0x00},
	{MAX98396_R204B_PCM_TX_CTRL_8, 0x00},
	{MAX98396_R204C_PCM_TX_HIZ_CTRL_1, 0xFF},
	{MAX98396_R204D_PCM_TX_HIZ_CTRL_2, 0xFF},
	{MAX98396_R204E_PCM_TX_HIZ_CTRL_3, 0xFF},
	{MAX98396_R204F_PCM_TX_HIZ_CTRL_4, 0xFF},
	{MAX98396_R2050_PCM_TX_HIZ_CTRL_5, 0xFF},
	{MAX98396_R2051_PCM_TX_HIZ_CTRL_6, 0xFF},
	{MAX98396_R2052_PCM_TX_HIZ_CTRL_7, 0xFF},
	{MAX98396_R2053_PCM_TX_HIZ_CTRL_8, 0xFF},
	{MAX98396_R2055_PCM_RX_SRC1, 0x00},
	{MAX98396_R2056_PCM_RX_SRC2, 0x00},
	{MAX98396_R2058_PCM_BYPASS_SRC, 0x00},
	{MAX98396_R205D_PCM_TX_SRC_EN, 0x00},
	{MAX98396_R205E_PCM_RX_EN, 0x00},
	{MAX98396_R205F_PCM_TX_EN, 0x00},
	{MAX98396_R2070_ICC_RX_EN_A, 0x00},
	{MAX98396_R2071_ICC_RX_EN_B, 0x00},
	{MAX98396_R2072_ICC_TX_CTRL, 0x00},
	{MAX98396_R207F_ICC_EN, 0x00},
	{MAX98396_R2083_TONE_GEN_DC_CFG, 0x04},
	{MAX98396_R2084_TONE_GEN_DC_LVL1, 0x00},
	{MAX98396_R2085_TONE_GEN_DC_LVL2, 0x00},
	{MAX98396_R2086_TONE_GEN_DC_LVL3, 0x00},
	{MAX98396_R208F_TONE_GEN_EN, 0x00},
	{MAX98396_R2090_AMP_VOL_CTRL, 0x00},
	{MAX98396_R2091_AMP_PATH_GAIN, 0x0B},
	{MAX98396_R2092_AMP_DSP_CFG, 0x23},
	{MAX98396_R2093_SSM_CFG, 0x0D},
	{MAX98396_R2094_SPK_CLS_DG_THRESH, 0x12},
	{MAX98396_R2095_SPK_CLS_DG_HDR, 0x17},
	{MAX98396_R2096_SPK_CLS_DG_HOLD_TIME, 0x17},
	{MAX98396_R2097_SPK_CLS_DG_DELAY, 0x00},
	{MAX98396_R2098_SPK_CLS_DG_MODE, 0x00},
	{MAX98396_R2099_SPK_CLS_DG_VBAT_LVL, 0x03},
	{MAX98396_R209A_SPK_EDGE_CTRL, 0x00},
	{MAX98396_R209C_SPK_EDGE_CTRL1, 0x0A},
	{MAX98396_R209D_SPK_EDGE_CTRL2, 0xAA},
	{MAX98396_R209E_AMP_CLIP_GAIN, 0x00},
	{MAX98396_R209F_BYPASS_PATH_CFG, 0x00},
	{MAX98396_R20A0_AMP_SUPPLY_CTL, 0x00},
	{MAX98396_R20AF_AMP_EN, 0x00},
	{MAX98396_R20B0_MEAS_ADC_SR, 0x30},
	{MAX98396_R20B1_MEAS_ADC_PVDD_CFG, 0x00},
	{MAX98396_R20B2_MEAS_ADC_VBAT_CFG, 0x00},
	{MAX98396_R20B3_MEAS_ADC_THERMAL_CFG, 0x00},
	{MAX98396_R20B4_ADC_READBACK_CTRL1, 0x00},
	{MAX98396_R20B5_ADC_READBACK_CTRL2, 0x00},
	{MAX98396_R20B6_ADC_PVDD_READBACK_MSB, 0x00},
	{MAX98396_R20B7_ADC_PVDD_READBACK_LSB, 0x00},
	{MAX98396_R20B8_ADC_VBAT_READBACK_MSB, 0x00},
	{MAX98396_R20B9_ADC_VBAT_READBACK_LSB, 0x00},
	{MAX98396_R20BA_TEMP_READBACK_MSB, 0x00},
	{MAX98396_R20BB_TEMP_READBACK_LSB, 0x00},
	{MAX98396_R20BC_LO_PVDD_READBACK_MSB, 0xFF},
	{MAX98396_R20BD_LO_PVDD_READBACK_LSB, 0x01},
	{MAX98396_R20BE_LO_VBAT_READBACK_MSB, 0xFF},
	{MAX98396_R20BF_LO_VBAT_READBACK_LSB, 0x01},
	{MAX98396_R20C7_MEAS_ADC_CFG, 0x00},
	{MAX98396_R20D0_DHT_CFG1, 0x00},
	{MAX98396_R20D1_LIMITER_CFG1, 0x08},
	{MAX98396_R20D2_LIMITER_CFG2, 0x00},
	{MAX98396_R20D3_DHT_CFG2, 0x14},
	{MAX98396_R20D4_DHT_CFG3, 0x02},
	{MAX98396_R20D5_DHT_CFG4, 0x04},
	{MAX98396_R20D6_DHT_HYSTERESIS_CFG, 0x07},
	{MAX98396_R20DF_DHT_EN, 0x00},
	{MAX98396_R20E0_IV_SENSE_PATH_CFG, 0x04},
	{MAX98396_R20E4_IV_SENSE_PATH_EN, 0x00},
	{MAX98396_R20E5_BPE_STATE, 0x00},
	{MAX98396_R20E6_BPE_L3_THRESH_MSB, 0x00},
	{MAX98396_R20E7_BPE_L3_THRESH_LSB, 0x00},
	{MAX98396_R20E8_BPE_L2_THRESH_MSB, 0x00},
	{MAX98396_R20E9_BPE_L2_THRESH_LSB, 0x00},
	{MAX98396_R20EA_BPE_L1_THRESH_MSB, 0x00},
	{MAX98396_R20EB_BPE_L1_THRESH_LSB, 0x00},
	{MAX98396_R20EC_BPE_L0_THRESH_MSB, 0x00},
	{MAX98396_R20ED_BPE_L0_THRESH_LSB, 0x00},
	{MAX98396_R20EE_BPE_L3_DWELL_HOLD_TIME, 0x00},
	{MAX98396_R20EF_BPE_L2_DWELL_HOLD_TIME, 0x00},
	{MAX98396_R20F0_BPE_L1_DWELL_HOLD_TIME, 0x00},
	{MAX98396_R20F1_BPE_L0_HOLD_TIME, 0x00},
	{MAX98396_R20F2_BPE_L3_ATTACK_REL_STEP, 0x00},
	{MAX98396_R20F3_BPE_L2_ATTACK_REL_STEP, 0x00},
	{MAX98396_R20F4_BPE_L1_ATTACK_REL_STEP, 0x00},
	{MAX98396_R20F5_BPE_L0_ATTACK_REL_STEP, 0x00},
	{MAX98396_R20F6_BPE_L3_MAX_GAIN_ATTN, 0x00},
	{MAX98396_R20F7_BPE_L2_MAX_GAIN_ATTN, 0x00},
	{MAX98396_R20F8_BPE_L1_MAX_GAIN_ATTN, 0x00},
	{MAX98396_R20F9_BPE_L0_MAX_GAIN_ATTN, 0x00},
	{MAX98396_R20FA_BPE_L3_GAIN_ATTACK_REL_RATE, 0x00},
	{MAX98396_R20FB_BPE_L2_GAIN_ATTACK_REL_RATE, 0x00},
	{MAX98396_R20FC_BPE_L1_GAIN_ATTACK_REL_RATE, 0x00},
	{MAX98396_R20FD_BPE_L0_GAIN_ATTACK_REL_RATE, 0x00},
	{MAX98396_R20FE_BPE_L3_LIMITER_CFG, 0x00},
	{MAX98396_R20FF_BPE_L2_LIMITER_CFG, 0x00},
	{MAX98396_R2100_BPE_L1_LIMITER_CFG, 0x00},
	{MAX98396_R2101_BPE_L0_LIMITER_CFG, 0x00},
	{MAX98396_R2102_BPE_L3_LIMITER_ATTACK_REL_RATE, 0x00},
	{MAX98396_R2103_BPE_L2_LIMITER_ATTACK_REL_RATE, 0x00},
	{MAX98396_R2104_BPE_L1_LIMITER_ATTACK_REL_RATE, 0x00},
	{MAX98396_R2105_BPE_L0_LIMITER_ATTACK_REL_RATE, 0x00},
	{MAX98396_R2106_BPE_THRESH_HYSTERESIS, 0x00},
	{MAX98396_R2107_BPE_INFINITE_HOLD_CLR, 0x00},
	{MAX98396_R2108_BPE_SUPPLY_SRC, 0x00},
	{MAX98396_R2109_BPE_LOW_STATE, 0x00},
	{MAX98396_R210A_BPE_LOW_GAIN, 0x00},
	{MAX98396_R210B_BPE_LOW_LIMITER, 0x00},
	{MAX98396_R210D_BPE_EN, 0x00},
	{MAX98396_R210E_AUTO_RESTART_BEHAVIOR, 0x00},
	{MAX98396_R210F_GLOBAL_EN, 0x00},
	{MAX98396_R21FF_REVISION_ID, 0x00},
};

static int max98396_dai_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);
	unsigned int format = 0;
	unsigned int invert = 0;

	dev_info(component->dev, "%s+: dac %u fmt 0x%08X\n",
		__func__, max98396->dac_num, fmt);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = MAX98396_PCM_MODE_CFG_PCM_BCLKEDGE;
		break;
	default:
		dev_err(component->dev, "%s: DAI invert mode unsupported\n",
			__func__);
		return -EINVAL;
	}

	regmap_update_bits(max98396->regmap,
		MAX98396_R2042_PCM_CLK_SETUP,
		MAX98396_PCM_MODE_CFG_PCM_BCLKEDGE,
		invert);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format = MAX98396_PCM_FORMAT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format = MAX98396_PCM_FORMAT_LJ;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		format = MAX98396_PCM_FORMAT_TDM_MODE1;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		format = MAX98396_PCM_FORMAT_TDM_MODE0;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(max98396->regmap,
		MAX98396_R2041_PCM_MODE_CFG,
		MAX98396_PCM_MODE_CFG_FORMAT_MASK,
		format << MAX98396_PCM_MODE_CFG_FORMAT_SHIFT);

	dev_info(component->dev, "%s-", __func__);

	return 0;
}

/* BCLKs per LRCLK */
static const int bclk_sel_table[] = {
	32, 48, 64, 96, 128, 192, 256, 384, 512, 320,
};

static int max98396_get_bclk_sel(int bclk)
{
	int i;
	/* match BCLKs per LRCLK */
	for (i = 0; i < ARRAY_SIZE(bclk_sel_table); i++) {
		if (bclk_sel_table[i] == bclk)
			return i + 2;
	}
	return 0;
}

static int max98396_set_clock(struct snd_soc_component *component,
	struct snd_pcm_hw_params *params)
{
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);
	unsigned int ch = params_channels(params);
	unsigned int blr_clk_ratio;
	int value = 0;

	dev_info(component->dev, "%s+: dac %u ch %d tdm %d", __func__,
		max98396->dac_num, params_channels(params), max98396->tdm_mode);

	/* Limit to 2 channel I2S */
	if (ch > MAX_CHANNELS)
		ch = MAX_CHANNELS;

	/* BCLK/LRCLK ratio calculation */
	blr_clk_ratio = ch * max98396->ch_size;

	if (!max98396->tdm_mode) {
		/* BCLK configuration */
		value = max98396_get_bclk_sel(blr_clk_ratio);
		if (!value) {
			dev_err(component->dev, "%s: format unsupported %d\n",
				__func__, params_format(params));
			return -EINVAL;
		}

		regmap_update_bits(max98396->regmap,
			MAX98396_R2042_PCM_CLK_SETUP,
			MAX98396_PCM_CLK_SETUP_BSEL_MASK,
			value);
	}

	dev_info(component->dev, "%s-: blr_clk_ratio %u value %u",
		__func__, blr_clk_ratio, value);

	return 0;
}

static void max98396_reset(struct max98396_priv *max98396, struct device *dev)
{
	int ret, reg, count;

	max98396->init_done = false;
	/* Software Reset */
	ret = regmap_write(max98396->regmap,
		MAX98396_R2000_SW_RESET, 1);
	if (ret)
		dev_err(dev, "%s: Reset command failed. (ret:%d) dac %u\n",
			__func__, ret, max98396->dac_num);

	count = 0;
	while (count < 3) {
		usleep_range(10000, 11000);
		/* Software Reset Verification */
		ret = regmap_read(max98396->regmap,
			MAX98396_R21FF_REVISION_ID, &reg);
		if (!ret) {
			dev_info(dev, "%s Reset completed (retry:%d) dac %u\n",
				__func__, count, max98396->dac_num);
			return;
		}
		count++;
	}
	dev_err(dev, "%s-: Reset failed. (ret:%d) dac %u\n",
		__func__, ret, max98396->dac_num);
}

static int max98396_init_setup(struct snd_soc_component *component)
{
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s: dac %u init_done %d", __func__,
		max98396->dac_num, max98396->init_done);

	if (max98396->init_done)
		return 0;
	#ifdef MAX98396_ENABLE_SW_RESET
	/* Software Reset */
	max98396_reset(max98396, component->dev);
	#endif

	regmap_write(max98396->regmap,
		MAX98396_R2038_CLK_MON_CTRL, 0x01);

	/* L/R mix configuration */
	regmap_write(max98396->regmap,
		MAX98396_R2055_PCM_RX_SRC1, 0x02);

	regmap_write(max98396->regmap,
		MAX98396_R2056_PCM_RX_SRC2, 0x10);
	/* Enable DC blocker */
	regmap_update_bits(max98396->regmap,
		MAX98396_R2092_AMP_DSP_CFG, 1, 1);
	/* Disble Safe Mode */
	regmap_update_bits(max98396->regmap,
		MAX98396_R2092_AMP_DSP_CFG,
		MAX98396_DSP_SPK_SAFE_EN_SHIFT, 0);
	/* Enable wband filter */
	regmap_update_bits(max98396->regmap,
		MAX98396_R2092_AMP_DSP_CFG,
		MAX98396_DSP_SPK_WBAND_FLT_EN_SHIFT, 1);
	/* Enable IMON VMON DC blocker */
	regmap_update_bits(max98396->regmap,
		MAX98396_R20E0_IV_SENSE_PATH_CFG, 3, 3);
	/* voltage, current slot configuration */
	regmap_write(max98396->regmap,
		MAX98396_R2044_PCM_TX_CTRL_1,
		max98396->v_slot);
	regmap_write(max98396->regmap,
		MAX98396_R2045_PCM_TX_CTRL_2,
		max98396->i_slot);

	if (max98396->v_slot < 8)
		regmap_update_bits(max98396->regmap,
			MAX98396_R2053_PCM_TX_HIZ_CTRL_8,
			1 << max98396->v_slot, 0);
	else
		regmap_update_bits(max98396->regmap,
			MAX98396_R2052_PCM_TX_HIZ_CTRL_7,
			1 << (max98396->v_slot - 8), 0);

	if (max98396->i_slot < 8)
		regmap_update_bits(max98396->regmap,
			MAX98396_R2053_PCM_TX_HIZ_CTRL_8,
			1 << max98396->i_slot, 0);
	else
		regmap_update_bits(max98396->regmap,
			MAX98396_R2052_PCM_TX_HIZ_CTRL_7,
			1 << (max98396->i_slot - 8), 0);

	/* Set interleave mode */
	if (max98396->interleave_mode)
		regmap_update_bits(max98396->regmap,
			MAX98396_R2041_PCM_MODE_CFG,
			MAX98396_PCM_TX_CH_INTERLEAVE_MASK,
			MAX98396_PCM_TX_CH_INTERLEAVE_MASK);

	regmap_write(max98396->regmap,
		MAX98396_R20A0_AMP_SUPPLY_CTL,
		max98396->novbat);


	max98396->init_done = true;
	dev_info(component->dev, "%s-", __func__);

	return 0;
}

static int max98396_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);
	unsigned int sampling_rate = 0;
	unsigned int chan_sz = 0;

	dev_info(component->dev, "%s+: dac %u",
		__func__, max98396->dac_num);

	max98396_init_setup(component);

	/* pcm mode configuration */
	switch (snd_pcm_format_width(params_format(params))) {
	case 16:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_16;
		break;
	case 24:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_24;
		break;
	case 32:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_32;
		break;
	default:
		dev_err(component->dev, "%s: format unsupported %d\n",
			__func__, params_format(params));
		goto err;
	}

	max98396->ch_size = snd_pcm_format_width(params_format(params));

	regmap_update_bits(max98396->regmap,
		MAX98396_R2041_PCM_MODE_CFG,
		MAX98396_PCM_MODE_CFG_CHANSZ_MASK, chan_sz);

	dev_info(component->dev, "%s format supported %d ch_size %d rate %d",
		__func__, params_format(params),
		max98396->ch_size, params_rate(params));

	/* sampling rate configuration */
	switch (params_rate(params)) {
	case 8000:
		sampling_rate = MAX98396_PCM_SR_8000;
		break;
	case 11025:
		sampling_rate = MAX98396_PCM_SR_11025;
		break;
	case 12000:
		sampling_rate = MAX98396_PCM_SR_12000;
		break;
	case 16000:
		sampling_rate = MAX98396_PCM_SR_16000;
		break;
	case 22050:
		sampling_rate = MAX98396_PCM_SR_22050;
		break;
	case 24000:
		sampling_rate = MAX98396_PCM_SR_24000;
		break;
	case 32000:
		sampling_rate = MAX98396_PCM_SR_32000;
		break;
	case 44100:
		sampling_rate = MAX98396_PCM_SR_44100;
		break;
	case 48000:
		sampling_rate = MAX98396_PCM_SR_48000;
		break;
	case 88200:
		sampling_rate = MAX98396_PCM_SR_88200;
		break;
	case 96000:
		sampling_rate = MAX98396_PCM_SR_96000;
		break;
	default:
		dev_err(component->dev, "rate %d not supported\n",
			params_rate(params));
		goto err;
	}

	/* set DAI_SR to correct LRCLK frequency */
	regmap_update_bits(max98396->regmap,
		MAX98396_R2043_PCM_SR_SETUP,
		MAX98396_PCM_SR_MASK,
		sampling_rate);

	/* set sampling rate of IV */
	if (max98396->interleave_mode &&
		sampling_rate > MAX98396_PCM_SR_16000)
		regmap_update_bits(max98396->regmap,
			MAX98396_R2043_PCM_SR_SETUP,
			MAX98396_IVADC_SR_MASK,
			(sampling_rate - 3) << MAX98396_IVADC_SR_SHIFT);
	else
		regmap_update_bits(max98396->regmap,
			MAX98396_R2043_PCM_SR_SETUP,
			MAX98396_IVADC_SR_MASK,
			sampling_rate << MAX98396_IVADC_SR_SHIFT);

	dev_info(component->dev, "%s-", __func__);

	return max98396_set_clock(component, params);
err:
	dev_info(component->dev, "%s-: error", __func__);
	return -EINVAL;
}

static int max98396_dai_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask,
	int slots, int slot_width)
{
	struct snd_soc_component *component = dai->component;
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);
	int bsel = 0;
	unsigned int chan_sz = 0;

	dev_info(component->dev, "%s+: dac %u",
		__func__, max98396->dac_num);

	if (!tx_mask && !rx_mask && !slots && !slot_width)
		max98396->tdm_mode = false;
	else
		max98396->tdm_mode = true;

	/* BCLK configuration */
	bsel = max98396_get_bclk_sel(slots * slot_width);
	if (bsel == 0) {
		dev_err(component->dev, "%s: BCLK %d not supported\n",
			__func__, slots * slot_width);
		return -EINVAL;
	}

	regmap_update_bits(max98396->regmap,
		MAX98396_R2042_PCM_CLK_SETUP,
		MAX98396_PCM_CLK_SETUP_BSEL_MASK,
		bsel);

	/* Channel size configuration */
	switch (slot_width) {
	case 16:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_16;
		break;
	case 24:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_24;
		break;
	case 32:
		chan_sz = MAX98396_PCM_MODE_CFG_CHANSZ_32;
		break;
	default:
		dev_err(component->dev, "format unsupported %d\n",
			slot_width);
		return -EINVAL;
	}

	regmap_update_bits(max98396->regmap,
		MAX98396_R2041_PCM_MODE_CFG,
		MAX98396_PCM_MODE_CFG_CHANSZ_MASK, chan_sz);

	/* Rx slot configuration */
	regmap_update_bits(max98396->regmap,
		MAX98396_R2056_PCM_RX_SRC2,
		MAX98396_PCM_DMIX_CH0_SRC_MASK,
		rx_mask);
	regmap_update_bits(max98396->regmap,
		MAX98396_R2056_PCM_RX_SRC2,
		MAX98396_PCM_DMIX_CH1_SRC_MASK,
		rx_mask << MAX98396_PCM_DMIX_CH1_SHIFT);

	/* Tx slot Hi-Z configuration */
	regmap_write(max98396->regmap,
		MAX98396_R2053_PCM_TX_HIZ_CTRL_8,
		~tx_mask & 0xFF);
	regmap_write(max98396->regmap,
		MAX98396_R2052_PCM_TX_HIZ_CTRL_7,
		(~tx_mask & 0xFF00) >> 8);

	return 0;
}

#define MAX98396_RATES SNDRV_PCM_RATE_8000_96000

#define MAX98396_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops max98396_dai_ops = {
	.set_fmt = max98396_dai_set_fmt,
	.hw_params = max98396_dai_hw_params,
	.set_tdm_slot = max98396_dai_tdm_slot,
};

static int max98396_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s: dac %u event %d",
		__func__, max98396->dac_num, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(max98396->regmap,
			MAX98396_R205E_PCM_RX_EN,
			MAX98396_PCM_RX_EN_MASK, 1);

		regmap_write(max98396->regmap,
			MAX98396_R210F_GLOBAL_EN, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(max98396->regmap,
			MAX98396_R205E_PCM_RX_EN,
			MAX98396_PCM_RX_EN_MASK, 0);

		regmap_write(max98396->regmap,
			MAX98396_R210F_GLOBAL_EN, 0);
		max98396->tdm_mode = false;
		break;
	default:
		return 0;
	}

	return 0;
}

static const char * const max98396_switch_text[] = {
	"Left", "Right", "LeftRight"};

static const struct soc_enum dai_sel_enum =
	SOC_ENUM_SINGLE(MAX98396_R2055_PCM_RX_SRC1,
		0, 3, max98396_switch_text);

static const struct snd_kcontrol_new max98396_dai_controls_a =
	SOC_DAPM_ENUM("DAI Sel A", dai_sel_enum);

static const struct snd_kcontrol_new max98396_dai_controls_b =
	SOC_DAPM_ENUM("DAI Sel B", dai_sel_enum);

static const struct snd_kcontrol_new max98396_dai_controls_c =
	SOC_DAPM_ENUM("DAI Sel C", dai_sel_enum);

static const struct snd_kcontrol_new max98396_vi_control =
	SOC_DAPM_SINGLE("Switch", MAX98396_R205F_PCM_TX_EN, 0, 1, 0);

static const struct snd_soc_dapm_widget max98396_dapm_widgets_a[] = {
SND_SOC_DAPM_DAC_E("Amp Enable A", "Playback",
	MAX98396_R20AF_AMP_EN, 0, 0, max98396_dac_event,
	SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_MUX("DAI Sel Mux A", SND_SOC_NOPM, 0, 0,
	&max98396_dai_controls_a),
SND_SOC_DAPM_OUTPUT("BE_OUT A"),
SND_SOC_DAPM_AIF_OUT("Voltage Sense A", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 0, 0),
SND_SOC_DAPM_AIF_OUT("Current Sense A", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 1, 0),
SND_SOC_DAPM_SWITCH("VI Sense A", SND_SOC_NOPM, 0, 0,
	&max98396_vi_control),
SND_SOC_DAPM_SIGGEN("VMON A"),
SND_SOC_DAPM_SIGGEN("IMON A"),
SND_SOC_DAPM_SIGGEN("FBMON A"),
};

static const struct snd_soc_dapm_widget max98396_dapm_widgets_b[] = {
SND_SOC_DAPM_DAC_E("Amp Enable B", "Playback",
	MAX98396_R20AF_AMP_EN, 0, 0, max98396_dac_event,
	SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_MUX("DAI Sel Mux B", SND_SOC_NOPM, 0, 0,
	&max98396_dai_controls_b),
SND_SOC_DAPM_OUTPUT("BE_OUT B"),
SND_SOC_DAPM_AIF_OUT("Voltage Sense B", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 0, 0),
SND_SOC_DAPM_AIF_OUT("Current Sense B", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 1, 0),
SND_SOC_DAPM_SWITCH("VI Sense B", SND_SOC_NOPM, 0, 0,
	&max98396_vi_control),
SND_SOC_DAPM_SIGGEN("VMON B"),
SND_SOC_DAPM_SIGGEN("IMON B"),
SND_SOC_DAPM_SIGGEN("FBMON B"),
};

static const struct snd_soc_dapm_widget max98396_dapm_widgets_c[] = {
SND_SOC_DAPM_DAC_E("Amp Enable C", "Playback",
	MAX98396_R20AF_AMP_EN, 0, 0, max98396_dac_event,
	SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
SND_SOC_DAPM_MUX("DAI Sel Mux C", SND_SOC_NOPM, 0, 0,
	&max98396_dai_controls_c),
SND_SOC_DAPM_OUTPUT("BE_OUT C"),
SND_SOC_DAPM_AIF_OUT("Voltage Sense C", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 0, 0),
SND_SOC_DAPM_AIF_OUT("Current Sense C", "Capture", 0,
	MAX98396_R20E4_IV_SENSE_PATH_EN, 1, 0),
SND_SOC_DAPM_SWITCH("VI Sense C", SND_SOC_NOPM, 0, 0,
	&max98396_vi_control),
SND_SOC_DAPM_SIGGEN("VMON C"),
SND_SOC_DAPM_SIGGEN("IMON C"),
SND_SOC_DAPM_SIGGEN("FBMON C"),};

static DECLARE_TLV_DB_SCALE(max98396_digital_tlv, -6300, 50, 1);
static const DECLARE_TLV_DB_RANGE(max98396_spk_tlv,
	0, 17, TLV_DB_SCALE_ITEM(400, 100, 0),
);

static bool max98396_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98396_R2027_THERM_FOLDBACK_EN:
	case MAX98396_R2030_NOISE_GATE_IDLE_MODE_CTRL:
	case MAX98396_R2033_NOISE_GATE_IDLE_MODE_EN:
	case MAX98396_R2058_PCM_BYPASS_SRC:
	case MAX98396_R207F_ICC_EN:
	case MAX98396_R20C7_MEAS_ADC_CFG:
	case MAX98396_R20E0_IV_SENSE_PATH_CFG:
	case MAX98396_R21FF_REVISION_ID:
	case MAX98396_R2001_INT_RAW1 ... MAX98396_R2013_INT_EN4:
	case MAX98396_R201F_IRQ_CTRL ... MAX98396_R2024_THERM_FOLDBACK_SET:
	case MAX98396_R2038_CLK_MON_CTRL ... MAX98396_R2053_PCM_TX_HIZ_CTRL_8:
	case MAX98396_R2055_PCM_RX_SRC1 ... MAX98396_R2056_PCM_RX_SRC2:
	case MAX98396_R205D_PCM_TX_SRC_EN ... MAX98396_R205F_PCM_TX_EN:
	case MAX98396_R2070_ICC_RX_EN_A... MAX98396_R2072_ICC_TX_CTRL:
	case MAX98396_R2083_TONE_GEN_DC_CFG ... MAX98396_R2086_TONE_GEN_DC_LVL3:
	case MAX98396_R208F_TONE_GEN_EN ... MAX98396_R209A_SPK_EDGE_CTRL:
	case MAX98396_R209C_SPK_EDGE_CTRL1 ... MAX98396_R20A0_AMP_SUPPLY_CTL:
	case MAX98396_R20AF_AMP_EN ... MAX98396_R20BF_LO_VBAT_READBACK_LSB:
	case MAX98396_R20D0_DHT_CFG1 ... MAX98396_R20D6_DHT_HYSTERESIS_CFG:
	case MAX98396_R20E4_IV_SENSE_PATH_EN
		... MAX98396_R2106_BPE_THRESH_HYSTERESIS:
	case MAX98396_R2108_BPE_SUPPLY_SRC ... MAX98396_R210B_BPE_LOW_LIMITER:
	case MAX98396_R210D_BPE_EN ... MAX98396_R210F_GLOBAL_EN:
		return true;
	default:
		return false;
	}
};

static bool max98396_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98396_R210F_GLOBAL_EN:
	case MAX98396_R21FF_REVISION_ID:
	case MAX98396_R2001_INT_RAW1 ... MAX98396_R200E_INT_FLAG4:
	case MAX98396_R20B6_ADC_PVDD_READBACK_MSB
		... MAX98396_R20BF_LO_VBAT_READBACK_LSB:
		return true;
	default:
		return false;
	}
}

static const struct snd_kcontrol_new max98396_snd_controls_a[] = {
/* Volume */
SOC_SINGLE_TLV("Digital Volume A", MAX98396_R2090_AMP_VOL_CTRL,
	0, 0x7F, 1, max98396_digital_tlv),
SOC_SINGLE_TLV("Speaker Volume A", MAX98396_R2091_AMP_PATH_GAIN,
	0, 0x11, 0, max98396_spk_tlv),
/* Speaker Safe Mode Enable*/
SOC_SINGLE("Speaker Safe Mode A", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_SAFE_EN_SHIFT, 1, 0),
/* Volume Ramp Up/Down Enable*/
SOC_SINGLE("Ramp Up Switch A", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPUP_SHIFT, 1, 0),
SOC_SINGLE("Ramp Down Switch A", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPDN_SHIFT, 1, 0),
/* Clock Monitor Enable */
SOC_SINGLE("CLK Monitor Switch A", MAX98396_R203F_ENABLE_CTRLS,
	MAX98396_CTRL_CMON_EN_SHIFT, 1, 0),
/* Dither Enable */
SOC_SINGLE("Dither Switch A", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DITH_EN_SHIFT, 1, 0),
/* DC Blocker Enable */
SOC_SINGLE("DC Blocker Switch A", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DCBLK_EN_SHIFT, 1, 0),
/* PCM Bypass Enable */
SOC_SINGLE("PCM Bypass Switch A", MAX98396_R205E_PCM_RX_EN, 1, 1, 0),
/* Dynamic Headroom Tracking */
SOC_SINGLE("DHT Switch A", MAX98396_R20DF_DHT_EN, 0, 1, 0),
/* Brownout Protection Engine */
SOC_SINGLE("BPE Switch A", MAX98396_R210D_BPE_EN, 0, 1, 0),
/* Slew Rate control for EMI */
SOC_SINGLE("BPE Limiter Switch A", MAX98396_R210D_BPE_EN, 1, 1, 0),
SOC_SINGLE("Speaker SL Rate Mode A", MAX98396_R209C_SPK_EDGE_CTRL1, 0, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate LS A", MAX98396_R209D_SPK_EDGE_CTRL2, 4, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate HS A", MAX98396_R209D_SPK_EDGE_CTRL2, 0, 0x0f, 0),
};

static const struct snd_kcontrol_new max98396_snd_controls_b[] = {
/* Volume */
SOC_SINGLE_TLV("Digital Volume B", MAX98396_R2090_AMP_VOL_CTRL,
	0, 0x7F, 1, max98396_digital_tlv),
SOC_SINGLE_TLV("Speaker Volume B", MAX98396_R2091_AMP_PATH_GAIN,
	0, 0x11, 0, max98396_spk_tlv),
/* Speaker Safe Mode Enable*/
SOC_SINGLE("Speaker Safe Mode B", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_SAFE_EN_SHIFT, 1, 0),
/* Volume Ramp Up/Down Enable*/
SOC_SINGLE("Ramp Up Switch B", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPUP_SHIFT, 1, 0),
SOC_SINGLE("Ramp Down Switch B", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPDN_SHIFT, 1, 0),
/* Clock Monitor Enable */
SOC_SINGLE("CLK Monitor Switch B", MAX98396_R203F_ENABLE_CTRLS,
	MAX98396_CTRL_CMON_EN_SHIFT, 1, 0),
/* Dither Enable */
SOC_SINGLE("Dither Switch B", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DITH_EN_SHIFT, 1, 0),
/* DC Blocker Enable */
SOC_SINGLE("DC Blocker Switch B", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DCBLK_EN_SHIFT, 1, 0),
/* PCM Bypass Enable */
SOC_SINGLE("PCM Bypass Switch B", MAX98396_R205E_PCM_RX_EN, 1, 1, 0),
/* Dynamic Headroom Tracking */
SOC_SINGLE("DHT Switch B", MAX98396_R20DF_DHT_EN, 0, 1, 0),
/* Brownout Protection Engine */
SOC_SINGLE("BPE Switch B", MAX98396_R210D_BPE_EN, 0, 1, 0),
SOC_SINGLE("BPE Limiter Switch B", MAX98396_R210D_BPE_EN, 1, 1, 0),
/* Slew Rate control for EMI */
SOC_SINGLE("Speaker SL Rate Mode B", MAX98396_R209C_SPK_EDGE_CTRL1, 0, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate LS B", MAX98396_R209D_SPK_EDGE_CTRL2, 4, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate HS B", MAX98396_R209D_SPK_EDGE_CTRL2, 0, 0x0f, 0),
};

static const struct snd_kcontrol_new max98396_snd_controls_c[] = {
/* Volume */
SOC_SINGLE_TLV("Digital Volume C", MAX98396_R2090_AMP_VOL_CTRL,
	0, 0x7F, 1, max98396_digital_tlv),
SOC_SINGLE_TLV("Speaker Volume C", MAX98396_R2091_AMP_PATH_GAIN,
	0, 0x11, 0, max98396_spk_tlv),
/* Speaker Safe Mode Enable*/
SOC_SINGLE("Speaker Safe Mode C", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_SAFE_EN_SHIFT, 1, 0),
/* Volume Ramp Up/Down Enable*/
SOC_SINGLE("Ramp Up Switch C", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPUP_SHIFT, 1, 0),
SOC_SINGLE("Ramp Down Switch C", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_VOL_RMPDN_SHIFT, 1, 0),
/* Clock Monitor Enable */
SOC_SINGLE("CLK Monitor Switch C", MAX98396_R203F_ENABLE_CTRLS,
	MAX98396_CTRL_CMON_EN_SHIFT, 1, 0),
/* Dither Enable */
SOC_SINGLE("Dither Switch C", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DITH_EN_SHIFT, 1, 0),
/* DC Blocker Enable */
SOC_SINGLE("DC Blocker Switch C", MAX98396_R2092_AMP_DSP_CFG,
	MAX98396_DSP_SPK_DCBLK_EN_SHIFT, 1, 0),
/* PCM Bypass Enable */
SOC_SINGLE("PCM Bypass Switch C", MAX98396_R205E_PCM_RX_EN, 1, 1, 0),
/* Dynamic Headroom Tracking */
SOC_SINGLE("DHT Switch C", MAX98396_R20DF_DHT_EN, 0, 1, 0),
/* Brownout Protection Engine */
SOC_SINGLE("BPE Switch C", MAX98396_R210D_BPE_EN, 0, 1, 0),
SOC_SINGLE("BPE Limiter Switch C", MAX98396_R210D_BPE_EN, 1, 1, 0),
/* Slew Rate control for EMI */
SOC_SINGLE("Speaker SL Rate Mode C", MAX98396_R209C_SPK_EDGE_CTRL1, 0, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate LS C", MAX98396_R209D_SPK_EDGE_CTRL2, 4, 0x0f, 0),
SOC_SINGLE("Speaker SL Rate HS C", MAX98396_R209D_SPK_EDGE_CTRL2, 0, 0x0f, 0),
};

static const struct snd_soc_dapm_route max98396_audio_map_a[] = {
	/* Plabyack */
	{"DAI Sel Mux A", "Left", "Amp Enable A"},
	{"DAI Sel Mux A", "Right", "Amp Enable A"},
	{"DAI Sel Mux A", "LeftRight", "Amp Enable A"},
	{"BE_OUT A", NULL, "DAI Sel Mux A"},
	/* Capture */
	{ "VI Sense A", "Switch", "VMON A" },
	{ "VI Sense A", "Switch", "IMON A" },
	{ "Voltage Sense A", NULL, "VI Sense A" },
	{ "Current Sense A", NULL, "VI Sense A" },
};

static const struct snd_soc_dapm_route max98396_audio_map_b[] = {
	/* Plabyack */
	{"DAI Sel Mux B", "Left", "Amp Enable B"},
	{"DAI Sel Mux B", "Right", "Amp Enable B"},
	{"DAI Sel Mux B", "LeftRight", "Amp Enable B"},
	{"BE_OUT B", NULL, "DAI Sel Mux B"},
	/* Capture */
	{ "VI Sense B", "Switch", "VMON B" },
	{ "VI Sense B", "Switch", "IMON B" },
	{ "Voltage Sense B", NULL, "VI Sense B" },
	{ "Current Sense B", NULL, "VI Sense B" },
};

static const struct snd_soc_dapm_route max98396_audio_map_c[] = {
	/* Plabyack */
	{"DAI Sel Mux C", "Left", "Amp Enable C"},
	{"DAI Sel Mux C", "Right", "Amp Enable C"},
	{"DAI Sel Mux C", "LeftRight", "Amp Enable C"},
	{"BE_OUT C", NULL, "DAI Sel Mux C"},
	/* Capture */
	{ "VI Sense C", "Switch", "VMON C" },
	{ "VI Sense C", "Switch", "IMON C" },
	{ "Voltage Sense C", NULL, "VI Sense C" },
	{ "Current Sense C", NULL, "VI Sense C" },
};

static struct snd_soc_dai_driver max98396_dai_a[] = {
	{
		.name = "max98396-aif1-a",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.ops = &max98396_dai_ops,
	}
};

static struct snd_soc_dai_driver max98396_dai_b[] = {
	{
		.name = "max98396-aif1-b",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.ops = &max98396_dai_ops,
	}
};

static struct snd_soc_dai_driver max98396_dai_c[] = {
	{
		.name = "max98396-aif1-c",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MAX98396_RATES,
			.formats = MAX98396_FORMATS,
		},
		.ops = &max98396_dai_ops,
	}
};

static int max98396_probe(struct snd_soc_component *component)
{
	struct max98396_priv *max98396 =
		snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s: dac %u init_done %u", __func__,
		max98396->dac_num, max98396->init_done);

	return 0;
}

int max98396_reset_platform(struct snd_soc_codec *codec, int enable)
{
	struct max98396_priv *max98396 = snd_soc_codec_get_drvdata(codec);
	// TODO add lock
	// TODO check if volume ramp reg is needed
	if (!enable) {
		regmap_write(max98396->regmap, MAX98396_R210F_GLOBAL_EN, 0);
		usleep_range(100, 150); //tOff
		regcache_cache_only(max98396->regmap, true);
		regcache_mark_dirty(max98396->regmap);
		gpio_direction_output(max98396->reset_gpio, 0);
		usleep_range(1, 2); //tRESET_LOW, probe does 50ms sleep though, TODO check
	}
	if (enable) {
		gpio_direction_output(max98396->reset_gpio, 1);
		usleep_range(1500, 1600); //tI2C_READY, probe does 20 ms sleep, TODO check
		regcache_cache_only(max98396->regmap, false);
		max98396_reset(max98396, codec->dev);
		regcache_sync(max98396->regmap);
		regmap_write(max98396->regmap, MAX98396_R210F_GLOBAL_EN, 1);
	}

	return 0;
}
EXPORT_SYMBOL(max98396_reset_platform);

#ifdef CONFIG_PM_SLEEP
static int max98396_suspend(struct device *dev)
{
	struct max98396_priv *max98396 = dev_get_drvdata(dev);

	dev_info(dev, "%s: dac %u init_done %u", __func__,
		max98396->dac_num, max98396->init_done);

	regcache_cache_only(max98396->regmap, true);
	regcache_mark_dirty(max98396->regmap);
	return 0;
}
static int max98396_resume(struct device *dev)
{
	struct max98396_priv *max98396 = dev_get_drvdata(dev);

	dev_info(dev, "%s: dac %u init_done %u", __func__,
		max98396->dac_num, max98396->init_done);

	regcache_cache_only(max98396->regmap, false);
	max98396_reset(max98396, dev);
	regcache_sync(max98396->regmap);
	return 0;
}
#endif

static const struct dev_pm_ops max98396_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(max98396_suspend, max98396_resume)
};

static int max98396_codec_probe(struct snd_soc_codec *codec)
{
	struct max98396_priv *max98396 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret = 0;

	pr_info("%s dac %u", __func__, max98396->dac_num);

	if (max98396->dac_num == 0) {
		snd_soc_dapm_new_controls(dapm, max98396_dapm_widgets_a,
					  ARRAY_SIZE(max98396_dapm_widgets_a));

		ret = snd_soc_add_codec_controls(codec, max98396_snd_controls_a,
				ARRAY_SIZE(max98396_snd_controls_a));

		/* set up audio path interconnects */
		snd_soc_dapm_add_routes(dapm, max98396_audio_map_a,
					ARRAY_SIZE(max98396_audio_map_a));
	} else if (max98396->dac_num == 1) {
		snd_soc_dapm_new_controls(dapm, max98396_dapm_widgets_b,
					  ARRAY_SIZE(max98396_dapm_widgets_b));

		ret = snd_soc_add_codec_controls(codec, max98396_snd_controls_b,
				ARRAY_SIZE(max98396_snd_controls_b));

		/* set up audio path interconnects */
		snd_soc_dapm_add_routes(dapm, max98396_audio_map_b,
					ARRAY_SIZE(max98396_audio_map_b));
	} else if (max98396->dac_num == 2) {
		snd_soc_dapm_new_controls(dapm, max98396_dapm_widgets_c,
					  ARRAY_SIZE(max98396_dapm_widgets_c));

		ret = snd_soc_add_codec_controls(codec, max98396_snd_controls_c,
				ARRAY_SIZE(max98396_snd_controls_c));

		/* set up audio path interconnects */
		snd_soc_dapm_add_routes(dapm, max98396_audio_map_c,
					ARRAY_SIZE(max98396_audio_map_c));
	} else {
		pr_err("%s-: dac_num %u wrong\n",
			__func__, max98396->dac_num);
			return -EINVAL;
	}

	pr_info("%s- error %d", __func__, ret);

	return ret;
}


static struct snd_soc_codec_driver soc_codec_max98396 = {
	.probe = max98396_codec_probe,
	.ignore_pmdown_time = 1,
	.component_driver = {
		.probe			= max98396_probe,
	},
};

static const struct regmap_config max98396_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = MAX98396_R21FF_REVISION_ID,
	.reg_defaults  = max98396_reg,
	.num_reg_defaults = ARRAY_SIZE(max98396_reg),
	.readable_reg = max98396_readable_register,
	.volatile_reg = max98396_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

static int max98396_slot_config(struct i2c_client *i2c,
	struct max98396_priv *max98396)
{
	int value = 0, ret = 0;
	struct device *dev = &i2c->dev;

	dev_info(dev, "%s+", __func__);

	if (!device_property_read_u32(dev, "maxim,vmon-slot-no", &value))
		max98396->v_slot = value & 0xF;
	else
		max98396->v_slot = 0;

	if (!device_property_read_u32(dev, "maxim,imon-slot-no", &value))
		max98396->i_slot = value & 0xF;
	else
		max98396->i_slot = 1;

	ret =  device_property_read_u32(dev, "dac-num",
					&max98396->dac_num);
	if (ret) {
		dev_err(dev, "%s: dac_num DT property missing\n",
			__func__);
		max98396->dac_num = 99;
	} else {
		dev_info(dev, "%s: dac_num %u\n",
		   __func__, max98396->dac_num);
	}

	if (dev->of_node && max98396->dac_num == 0) {
		max98396->reset_gpio = of_get_named_gpio(dev->of_node,
						"maxim,reset-gpio", 0);
		if (!gpio_is_valid(max98396->reset_gpio)) {
			dev_err(dev, "Looking up %s property in node %s failed %d\n",
				"maxim,reset-gpio", dev->of_node->full_name,
				max98396->reset_gpio);
		} else {
			dev_info(dev, "maxim,reset-gpio=%d",
				max98396->reset_gpio);
		}
	} else {
		/* this makes reset_gpio as invalid */
		max98396->reset_gpio = -1;
	}

	pr_info("%s- %d", __func__, ret);
	return ret;
}

static int max98396_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{

	int ret = 0;
	int reg = 0;
	unsigned int val = 0;
	struct max98396_priv *max98396 = NULL;

	dev_info(&i2c->dev, "%s+", __func__);
	max98396 = devm_kzalloc(&i2c->dev, sizeof(*max98396), GFP_KERNEL);

	if (!max98396) {
		ret = -ENOMEM;
		return ret;
	}

	/* regmap initialization */
	max98396->regmap
		= devm_regmap_init_i2c(i2c, &max98396_regmap);
	if (IS_ERR(max98396->regmap)) {
		ret = PTR_ERR(max98396->regmap);
		dev_err(&i2c->dev,
			"Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, max98396);

	/* update interleave mode info */
	if (device_property_read_bool(&i2c->dev, "maxim,interleave_mode"))
		max98396->interleave_mode = true;
	else
		max98396->interleave_mode = false;

	/* voltage/current slot & gpio configuration */
	max98396_slot_config(i2c, max98396);

	if (max98396->dac_num == 0) {
		/* Power on device */
		if (gpio_is_valid(max98396->reset_gpio)) {
			ret = devm_gpio_request(&i2c->dev, max98396->reset_gpio,
						"MAX98396_RESET");
			if (ret) {
				dev_err(&i2c->dev, "%s: Failed to request gpio %d\n",
					__func__, max98396->reset_gpio);
				return -EINVAL;
			}
			gpio_direction_output(max98396->reset_gpio, 0);
			msleep(50);
			gpio_direction_output(max98396->reset_gpio, 1);
			msleep(20);
			dev_info(&i2c->dev, "%s Gpio Reset Done!: %d\n",
				__func__, ret);
		} else {
			dev_err(&i2c->dev, "%s: No gpio found %d\n",
				__func__, max98396->reset_gpio);
		}
	}

	/* Check Revision ID */
	ret = regmap_read(max98396->regmap,
		MAX98396_R21FF_REVISION_ID, &reg);
	if (ret < 0) {
		dev_err(&i2c->dev,
			"Failed to read: 0x%02X\n", MAX98396_R21FF_REVISION_ID);
		return ret;
	}
	dev_info(&i2c->dev, "%s: MAX98396 revisionID: 0x%02X\n", __func__, reg);

	/* codec registration */
	if (max98396->dac_num == 0) {
		ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_max98396,
				max98396_dai_a, ARRAY_SIZE(max98396_dai_a));
	} else if (max98396->dac_num == 1) {
		ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_max98396,
				max98396_dai_b, ARRAY_SIZE(max98396_dai_b));

	} else if (max98396->dac_num == 2) {
		ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_max98396,
				max98396_dai_c, ARRAY_SIZE(max98396_dai_c));
	}
	if (ret < 0)
		dev_err(&i2c->dev, "Failed to register codec_num %d err %d\n",
			max98396->dac_num, ret);

	if (!device_property_read_u32(&i2c->dev, "NOVBAT", &val)) {
		max98396->novbat = val;
	} else {
		max98396->novbat = 0;
		dev_info(&i2c->dev, "novbat property missing\n");
	}

	dev_info(&i2c->dev, "%s-", __func__);

	return ret;
}

static void max98396_i2c_shutdown(struct i2c_client *i2c)
{
	dev_info(&i2c->dev, "%s\n", __func__);
}

static int max98396_i2c_remove(struct i2c_client *i2c)
{
	dev_info(&i2c->dev, "%s\n", __func__);
	return 0;
}

static const struct i2c_device_id max98396_i2c_id[] = {
	{ "max98396", 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, max98396_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id max98396_of_match[] = {
	{ .compatible = "maxim,max98396", },
	{ }
};
MODULE_DEVICE_TABLE(of, max98396_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id max98396_acpi_match[] = {
	{ "MX98396", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, max98396_acpi_match);
#endif

static struct i2c_driver max98396_i2c_driver = {
	.driver = {
		.name = "max98396",
		.of_match_table = of_match_ptr(max98396_of_match),
		.acpi_match_table = ACPI_PTR(max98396_acpi_match),
		.pm = &max98396_pm,
	},
	.probe = max98396_i2c_probe,
	.shutdown	= max98396_i2c_shutdown,
	.remove		= max98396_i2c_remove,
	.id_table = max98396_i2c_id,
};

module_i2c_driver(max98396_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98396 driver");
MODULE_AUTHOR("Ryan Lee <ryans.lee@maximintegrated.com>");
MODULE_LICENSE("GPL");
