/*
 * mt8512-codec.c  --  Mediatek 8512 ALSA SoC Codec driver
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

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "mt8512-codec.h"
#include <linux/iio/consumer.h>

#define MT8512_CODEC_NAME "mt8512-codec"

enum regmap_module_id {
	REGMAP_AFE = 0,
	REGMAP_APMIXEDSYS,
	REGMAP_NUMS,
};

enum codec_pga_gain_enum_id {
	UL_L_PGA_GAIN = 0,
	UL_R_PGA_GAIN,
	PGA_GAIN_MAX,
};

enum codec_adda_gain_enum_id {
	MUTE_GAIN = 0,
	ANALOG_GAIN,
	DIGITAL_GAIN,
	GAIN_MAX,
};

enum auxadc_channel_id {
	AUXADC_CH_0 = 0,
	AUXADC_CH_1,
	AUXADC_CH_MAX,
};

struct mt8512_codec_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct regmap *regmap_modules[REGMAP_NUMS];
	int adda_afe_on_ref_cnt;
	int adda_26mclk_on_ref_cnt;
	spinlock_t adda_afe_on_lock;
	spinlock_t adda_26mclk_lock;
	spinlock_t adda_rc_calibration_lock;
	struct clk *clk;
	unsigned int pga_gain[PGA_GAIN_MAX];
	unsigned int micbias0_setup_time_us;
	unsigned int micbias0_val;
	unsigned int ul_sgen_en;
	unsigned int dl_lpbk_en;
	unsigned int dl_nle_support;
	unsigned int dl_vol_mode;/*fix or manual*/
	unsigned int dl_analog_digital_gain;/*1:digital,2:analog*/
	unsigned int dl_gain_val;
	unsigned int ul_analog_digital_gain;/*1:digital,2:analog*/
	unsigned int ul_gain_val;
	bool	     ul_no_rc_cali; /*L&R do rc cali*/
	struct iio_channel          *adc_ch5;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};

static int module_reg_read(void *context,
	unsigned int reg, unsigned int *val,
	enum regmap_module_id id, unsigned int offset)
{
	struct mt8512_codec_priv *mt8512_codec =
			(struct mt8512_codec_priv *) context;
	int ret = 0;

	if (!(mt8512_codec && mt8512_codec->regmap_modules[id]))
		return -EIO;

	ret = regmap_read(mt8512_codec->regmap_modules[id],
			(reg & (~offset)), val);

	return ret;
}

static int module_reg_write(void *context,
	unsigned int reg, unsigned int val,
	enum regmap_module_id id, unsigned int offset)
{
	struct mt8512_codec_priv *mt8512_codec =
			(struct mt8512_codec_priv *) context;
	int ret = 0;

	if (!(mt8512_codec && mt8512_codec->regmap_modules[id]))
		return -EIO;

	ret = regmap_write(mt8512_codec->regmap_modules[id],
			(reg & (~offset)), val);

	return ret;
}

struct mt8512_codec_module_info {
	int id;
	unsigned int offset;
};

static const struct mt8512_codec_module_info mt8512_codec_modules[] = {
	{ .id = REGMAP_AFE, .offset = AFE_OFFSET },
	{ .id = REGMAP_APMIXEDSYS, .offset = APMIXED_OFFSET },
};

static int mt8512_codec_found_module_id(unsigned int reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8512_codec_modules); i++)
		if (reg & mt8512_codec_modules[i].offset)
			return mt8512_codec_modules[i].id;

	return -EINVAL;
}

static unsigned int mt8512_codec_found_module_offset(unsigned int reg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8512_codec_modules); i++)
		if (reg & mt8512_codec_modules[i].offset)
			return mt8512_codec_modules[i].offset;

	return 0;
}

static int mt8512_codec_reg_read(void *context,
	unsigned int reg, unsigned int *val)
{
	int id;
	unsigned int offset;

	id = mt8512_codec_found_module_id(reg);
	offset = mt8512_codec_found_module_offset(reg);

	if ((id < 0) || (offset == 0))
		return -EIO;

	return module_reg_read(context, reg, val,
			id, offset);
}

static int mt8512_codec_reg_write(void *context,
		unsigned int reg, unsigned int val)
{
	int id;
	unsigned int offset;

	id = mt8512_codec_found_module_id(reg);
	offset = mt8512_codec_found_module_offset(reg);

	if ((id < 0) || (offset == 0))
		return -EIO;

	return module_reg_write(context, reg, val,
			id, offset);
}

static void mt8512_codec_regmap_lock(void *lock_arg)
{
}

static void mt8512_codec_regmap_unlock(void *lock_arg)
{
}

static const struct regmap_config mt8512_codec_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_read = mt8512_codec_reg_read,
	.reg_write = mt8512_codec_reg_write,
	.lock = mt8512_codec_regmap_lock,
	.unlock = mt8512_codec_regmap_unlock,
	.cache_type = REGCACHE_NONE,
};

/* Audio_PGA1_Setting
 * Audio_PGA2_Setting
 * {-6, 0, +6, +12, +18, +24} dB
 */
static const char *const ul_pga_gain_text[] = {
	"-6dB", "-4dB", "-2dB", "0dB", "2dB", "4dB", "6dB", "8dB", "10dB",
	"12dB", "14dB", "16dB", "18dB", "20dB", "22dB", "24dB"
};

static const struct soc_enum mt8512_codec_pga_gain_enums[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_pga_gain_text),
		ul_pga_gain_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ul_pga_gain_text),
		ul_pga_gain_text),
};

static int mt8512_codec_get_gain_enum_id(const char *name)
{
	if (!strcmp(name, "Audio_PGA1_Setting"))
		return UL_L_PGA_GAIN;
	if (!strcmp(name, "Audio_PGA2_Setting"))
		return UL_R_PGA_GAIN;
	return -EINVAL;
}

static int mt8512_codec_pga_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	int id = mt8512_codec_get_gain_enum_id(kcontrol->id.name);
	uint32_t value = 0;

	switch (id) {
	case UL_L_PGA_GAIN:
	case UL_R_PGA_GAIN:
		value = codec_data->pga_gain[id];
		break;
	default:
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mt8512_codec_pga_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int id = mt8512_codec_get_gain_enum_id(kcontrol->id.name);
	uint32_t value = ucontrol->value.integer.value[0];

	if (value >= e->items)
		return -EINVAL;

	dev_dbg(codec_data->codec->dev,
		"%s id %d, value %u\n", __func__, id, value);

	switch (id) {
	case UL_L_PGA_GAIN:
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_PGA_GAIN_MASK,
			AUDUL_CON00_AUDULL_PGA_GAIN_VAL(value));
		break;
	case UL_R_PGA_GAIN:
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_GAIN_MASK,
			AUDUL_CON04_AUDULR_PGA_GAIN_VAL(value));
		break;
	default:
		return -EINVAL;
	}

	codec_data->pga_gain[id] = value;

	return 0;
}

static int mt8512_codec_uplink_sgen_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->ul_sgen_en;
	return 0;
}

static int mt8512_codec_uplink_sgen_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value = ucontrol->value.integer.value[0];

	if (!codec_data->ul_sgen_en && value) {
		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH1_MASK,
			ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH1_VAL(5));

		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH2_MASK,
			ABB_ULAFE_CON1_UL_SINEGEN_AMPDIV_CH2_VAL(5));

		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH1_MASK,
			ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH1_VAL(1));

		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH2_MASK,
			ABB_ULAFE_CON1_UL_SINEGEN_FREQDIV_CH2_VAL(2));

		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_OUTPUT_MASK,
			ABB_ULAFE_CON1_UL_SINEGEN_OUTPUT);

		snd_soc_component_update_bits(component,
			AUDIO_TOP_CON0,
			AUD_TCON0_PDN_UPLINK_TML,
			0);
	} else if (codec_data->ul_sgen_en && !value) {
		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_SINEGEN_OUTPUT_MASK,
			0x0);
	}

	codec_data->ul_sgen_en = ucontrol->value.integer.value[0];
	return 0;
}
static int mt8512_codec_downlink_vol_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dl_vol_mode;

	return 0;
}

/* fix mode    :0  hardware auto adjust volume */
/* manual mode :1  software adjust volume */
static int mt8512_codec_downlink_vol_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value = ucontrol->value.integer.value[0];

	snd_soc_component_update_bits(component,
			AFE_NLE_GAIN_IMP_LCH_CFG0,
			AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_MODE,
			AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_VAL(value));
	snd_soc_component_update_bits(component,
			AFE_NLE_GAIN_IMP_LCH_CFG0,
			AFE_NLE_ANALOG_GAIN_FIX_MANUAL_MODE,
			AFE_NLE_ANALOG_GAIN_FIX_MANUAL_VAL(value));
	snd_soc_component_update_bits(component,
			AFE_NLE_GAIN_IMP_RCH_CFG0,
			AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_MODE,
			AFE_NLE_DIGITAL_GAIN_FIX_MANUAL_VAL(value));
	snd_soc_component_update_bits(component,
			AFE_NLE_GAIN_IMP_RCH_CFG0,
			AFE_NLE_ANALOG_GAIN_FIX_MANUAL_MODE,
			AFE_NLE_ANALOG_GAIN_FIX_MANUAL_VAL(value));

	codec_data->dl_vol_mode = ucontrol->value.integer.value[0];

	return 0;
}
static int mt8512_codec_downlink_vol_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dl_analog_digital_gain;
	ucontrol->value.integer.value[1] = codec_data->dl_gain_val;

	return 0;
}

/* Digital  gain: 0-> 0db  ; 1->  1db .......32->32db */
/* 'Audio Downlink Vol' 1 32 */
/* Analog gain: 0->-20db; 1->-19db .....32->12db */
/* 'Audio Downlink Vol' 2 32 */
/* Mute    gain: 0:mute ; 1:unmute */
/* 'Audio Downlink Vol' 3 0 */
/* notes: only for samll signal -40db,normal use dg 0db ag -20~2db */
static int mt8512_codec_downlink_vol_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value1 = ucontrol->value.integer.value[0];
	uint32_t value2 = ucontrol->value.integer.value[1];

	dev_info(codec_data->codec->dev, "%s, value1 %u, value2 %u\n",
			__func__, value1, value2);

	if (value1 == DIGITAL_GAIN) { /*digtial gain*/
	snd_soc_component_update_bits(component,
		AFE_NLE_GAIN_IMP_LCH_CFG0,
		AFE_NLE_DIGITAL_GAIN_MANUAL_VAL_MASK,
		AFE_NLE_DIGITAL_GAIN_MANUAL_VAL(value2));
	snd_soc_component_update_bits(component,
		AFE_NLE_GAIN_IMP_RCH_CFG0,
		AFE_NLE_DIGITAL_GAIN_MANUAL_VAL_MASK,
		AFE_NLE_DIGITAL_GAIN_MANUAL_VAL(value2));
	} else if (value1 == ANALOG_GAIN) { /*analog gain*/
	snd_soc_component_update_bits(component,
		AFE_NLE_GAIN_IMP_LCH_CFG0,
		AFE_NLE_ANALOG_GAIN_MANUAL_VAL_MASK,
		AFE_NLE_ANALOG_GAIN_MANUAL_VAL(value2));
	snd_soc_component_update_bits(component,
		AFE_NLE_GAIN_IMP_RCH_CFG0,
		AFE_NLE_ANALOG_GAIN_MANUAL_VAL_MASK,
		AFE_NLE_ANALOG_GAIN_MANUAL_VAL(value2));
	} else if (value1 == MUTE_GAIN) { /*mute gain*/
		snd_soc_component_update_bits(component, ABB_AFE_CON0,
			ABB_AFE_CON0_DL_EN_MASK,
			value2);
	} else {
		// todo analog gain &digital gain
		dev_info(codec_data->codec->dev, "%s no this case!\n",
				__func__);
	}

	codec_data->dl_analog_digital_gain = ucontrol->value.integer.value[0];
	codec_data->dl_gain_val = ucontrol->value.integer.value[1];

	return 0;
}

static int mt8512_codec_uplink_vol_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->ul_analog_digital_gain;
	ucontrol->value.integer.value[1] = codec_data->ul_gain_val;
	return 0;
}
static int mt8512_codec_uplink_vol_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value1 = ucontrol->value.integer.value[0];
	uint32_t value2 = ucontrol->value.integer.value[1];

	dev_info(codec_data->codec->dev, "%s , value1 %u,value2 %u\n",
			__func__, value1, value2);

	if (value1 == DIGITAL_GAIN) { /*digtial gain*/
		/*ul gain not bypass*/
		snd_soc_component_update_bits(component,
			ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_GAIN_BYPASS_MASK,
			0x0);
	       /*downstep 0.25*/
		snd_soc_component_update_bits(component,
			AMIC_GAIN_CON2,
			AMIC_GAIN_CON2_DOWN_STEP_MASK,
			AMIC_GAIN_CON2_DOWN_STEP_VAL(0xF8BD));
	       /*upstep 0.25*/
		snd_soc_component_update_bits(component,
			AMIC_GAIN_CON3,
			AMIC_GAIN_CON3_UP_STEP_MASK,
			AMIC_GAIN_CON2_DOWN_STEP_VAL(0x1077A));
	       /*sample per step 200*/
		snd_soc_component_update_bits(component,
			AMIC_GAIN_CON0,
			AMIC_GAIN_CON0_SAMPLE_PER_STEP_MASK,
			AMIC_GAIN_CON0_SAMPLE_PER_STEP_VAL(0xC8));
			/*target gain 20log(x/0x10000)*/
	       snd_soc_component_update_bits(component,
			AMIC_GAIN_CON1,
			AMIC_GAIN_CON1_TARGET_MASK,
			AMIC_GAIN_CON1_TARGET_VAL(value2));
	      /*enable*/
		  snd_soc_component_update_bits(component,
			AMIC_GAIN_CON0,
			AMIC_GAIN_CON0_GAIN_EN_MASK,
			AMIC_GAIN_CON0_GAIN_ON);
	} else if (value1 == ANALOG_GAIN) { /*analog gain*/
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_PGA_GAIN_MASK,
			AUDUL_CON00_AUDULL_PGA_GAIN_VAL(value2));
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_GAIN_MASK,
			AUDUL_CON04_AUDULR_PGA_GAIN_VAL(value2));
	} else {
		// todo analog gain &digital gain
		dev_info(codec_data->codec->dev,
			"%s no this case!\n", __func__);
	}

	codec_data->ul_analog_digital_gain = ucontrol->value.integer.value[0];
	codec_data->ul_gain_val = ucontrol->value.integer.value[1];

	return 0;
}
static int mt8512_codec_downlink_loopback_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = codec_data->dl_lpbk_en;
	return 0;
}

static int mt8512_codec_downlink_loopback_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt8512_codec_priv *codec_data =
			snd_soc_component_get_drvdata(component);
	uint32_t value = ucontrol->value.integer.value[0];

	if (!codec_data->dl_lpbk_en && value) {
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0,
			AFE_ADDA_UL_DL_CON0_DL_LOOPBACK_MASK,
			AFE_ADDA_UL_DL_CON0_DL_LOOPBACK_ON);
	} else if (codec_data->dl_lpbk_en && !value) {
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0,
			AFE_ADDA_UL_DL_CON0_DL_LOOPBACK_MASK,
			0x0);
	}

	codec_data->dl_lpbk_en = ucontrol->value.integer.value[0];
	return 0;
}

#if 0
/* PGA Capture Volume
 * {-6, 0, +6, +12, +18, +24} dB
 */
static const DECLARE_TLV_DB_SCALE(ul_pga_gain_tlv, -600, 600, 0);
#endif

static const struct snd_kcontrol_new mt8512_codec_snd_controls[] = {
#if 0
	/* UL PGA gain adjustment */
	SOC_DOUBLE_R_TLV("PGA Capture Volume",
		AADC_CODEC_CON00, AADC_CODEC_CON01, 19, 5, 0,
		ul_pga_gain_tlv),
#endif
	/* Audio_PGA1_Setting */
	SOC_ENUM_EXT("Audio_PGA1_Setting",
		mt8512_codec_pga_gain_enums[UL_L_PGA_GAIN],
		mt8512_codec_pga_gain_get,
		mt8512_codec_pga_gain_put),
	/* Audio_PGA2_Setting */
	SOC_ENUM_EXT("Audio_PGA2_Setting",
		mt8512_codec_pga_gain_enums[UL_R_PGA_GAIN],
		mt8512_codec_pga_gain_get,
		mt8512_codec_pga_gain_put),
	/* for uplink debug */
	SOC_SINGLE_EXT("Audio Uplink SGEN",
		SND_SOC_NOPM, 0, 1, 0,
		mt8512_codec_uplink_sgen_get,
		mt8512_codec_uplink_sgen_put),
	/* for downlink debug */
	SOC_SINGLE_EXT("Audio Downlink Loopback",
		SND_SOC_NOPM, 0, 1, 0,
		mt8512_codec_downlink_loopback_get,
		mt8512_codec_downlink_loopback_put),
	/* Audio_DAC_AMP Vol mode */
	SOC_SINGLE_EXT("Audio Downlink Vol Mode",
		SND_SOC_NOPM, 0, 1, 0,
		mt8512_codec_downlink_vol_mode_get,
		mt8512_codec_downlink_vol_mode_put),
	/* Audio_DAC_AMP Vol */
	SOC_DOUBLE_EXT("Audio Downlink Vol",
		SND_SOC_NOPM, 0, 1, 32, 0,
		mt8512_codec_downlink_vol_get,
		mt8512_codec_downlink_vol_put),
		/* Audio_ADC Vol :655360:20db*/
	SOC_DOUBLE_EXT("Audio Uplink Vol",
		SND_SOC_NOPM, 0, 1, 655360, 0,
		mt8512_codec_uplink_vol_get,
		mt8512_codec_uplink_vol_put),
};

static int mt8512_codec_enable_adda_afe_on(
	struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8512_codec->adda_afe_on_lock, flags);

	mt8512_codec->adda_afe_on_ref_cnt++;
	if (mt8512_codec->adda_afe_on_ref_cnt == 1)
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&mt8512_codec->adda_afe_on_lock, flags);

	return 0;
}

static int mt8512_codec_disable_adda_afe_on(
	struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8512_codec->adda_afe_on_lock, flags);

	mt8512_codec->adda_afe_on_ref_cnt--;
	if (mt8512_codec->adda_afe_on_ref_cnt == 0)
		snd_soc_component_update_bits(component,
			AFE_ADDA_UL_DL_CON0, 0x1, 0x0);
	else if (mt8512_codec->adda_afe_on_ref_cnt < 0)
		mt8512_codec->adda_afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&mt8512_codec->adda_afe_on_lock, flags);

	return 0;
}

/*enable adc 26M clk ,dac 26m clk from adc*/
static int mt8512_codec_enable_26m_clk(
	struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8512_codec->adda_26mclk_lock, flags);

	mt8512_codec->adda_26mclk_on_ref_cnt++;
	if (mt8512_codec->adda_26mclk_on_ref_cnt == 1) {
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_EN_MASK,
			AUDUL_CON20_CLK_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_SEL_MASK,
			AUDUL_CON20_CLK_SEL_MASK);
	}

	spin_unlock_irqrestore(&mt8512_codec->adda_26mclk_lock, flags);

	return 0;
}

static int mt8512_codec_disable_26m_clk(
	struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	spin_lock_irqsave(&mt8512_codec->adda_26mclk_lock, flags);

	mt8512_codec->adda_26mclk_on_ref_cnt--;
	if (mt8512_codec->adda_26mclk_on_ref_cnt == 0) {
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_SEL_MASK,
			0x0);
	} else if (mt8512_codec->adda_26mclk_on_ref_cnt < 0)
		mt8512_codec->adda_26mclk_on_ref_cnt = 0;

	spin_unlock_irqrestore(&mt8512_codec->adda_26mclk_lock, flags);

	return 0;
}

static int mt8512_codec_dac_depop_setup(
	struct snd_soc_component *component)
{

	/*LDO Power Low avoid pop noise*/
	snd_soc_component_update_bits(component,
			AUDUL_CON19,
			AUDUL_CON19_DP_PL_EN_MASK,
			AUDUL_CON19_DP_PL_EN_MASK);
	snd_soc_component_update_bits(component,
			AUDUL_CON19,
			AUDUL_CON19_DP_PL_SEL_MASK,
			AUDUL_CON19_DP_PL_SEL_MASK);
	usleep_range(10, 11);

	/*LDO Enable*/
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_LDO1P8_ADAC_EN_MASK,
			AUDDL_CON00_LDO1P8_ADAC_EN_MASK);
	snd_soc_component_update_bits(component,
		       AUDDL_CON00,
		       AUDDL_CON00_LDO_VOLSEL_ADAC_MASK,
		       AUDDL_CON00_LDO_VOLSEL_ADAC_VAL(0x4));
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_LDO_LAT_EN_MASK,
			AUDDL_CON00_LDO_LAT_EN_MASK);
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_LDO_LAT_IQSEL_MASK,
			AUDDL_CON00_LDO_LAT_IQSEL_VAL(0x2)); /*60uA*/
	snd_soc_component_update_bits(component,
			AUDUL_CON19,
			AUDUL_CON19_LDO_LAT_VOSEL_MASK,
			AUDUL_CON19_LDO_LAT_VOSEL_VAL(0x0)); /*0.9v*/
	usleep_range(10, 11);

	/* VCM RAMP UP*/
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_VMID_PWDB_MASK,
			AUDDL_CON00_VMID_PWDB_MASK);
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_VMID_FASTUP_EN_MASK,
			AUDDL_CON00_VMID_FASTUP_EN_MASK);
	usleep_range(50000, 51000); /*50ms*/

	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_VMID_FASTUP_EN_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_VMID_RSEL_MASK,
			AUDDL_CON18_DEPOP_VMID_RSEL_VAL(0x2));
	usleep_range(10, 11);

	/*depop*/
	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_GLBIAS_ADAC_EN_MASK,
			AUDDL_CON00_GLBIAS_ADAC_EN_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S0_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S1_MASK,
			AUDDL_CON18_DP_S1_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENDP_MASK,
			AUDDL_CON18_ENDP_MASK);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENVO_MASK,
			0x0);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_I2VL_PWDB_MASK,
			AUDDL_CON01_I2VL_PWDB_MASK);
	snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_I2VR_PWDB_MASK,
			AUDDL_CON01_I2VR_PWDB_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_EN_MASK,
			AUDUL_CON20_CLK_EN_MASK);
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_SEL_MASK,
			AUDUL_CON20_CLK_SEL_MASK);

	snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_CLK26MHZ_EN_MASK,
			AUDDL_CON00_CLK26MHZ_EN_MASK);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_RAMPGEN_CAP_RESET_MASK,
			AUDDL_CON18_DEPOP_RAMPGEN_CAP_RESET_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_RAMPGEN_EN_MASK,
			AUDDL_CON18_DEPOP_RAMPGEN_EN_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_RAMPGEN_START_MASK,
			AUDDL_CON18_DEPOP_RAMPGEN_START_MASK);

	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_CLK_EN_MASK,
			AUDDL_CON18_DEPOP_CLK_EN_MASK);

	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDUL_CON19,
			AUDUL_CON19_DP_PL_SEL_MASK,
			0x0);
	usleep_range(2000000, 2001000);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S0_MASK,
			AUDDL_CON18_DP_S0_MASK);
	usleep_range(10, 11);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENVO_MASK,
			AUDDL_CON18_ENVO_MASK);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENDP_MASK,
			0x0);
	usleep_range(10, 11);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S1_MASK,
			0x0);
	usleep_range(10, 11);

	snd_soc_component_update_bits(component,
			AUDUL_CON19,
			AUDUL_CON19_DP_PL_EN_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_RAMPGEN_EN_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DEPOP_CLK_EN_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_RAMP_SEL_MASK,
			AUDDL_CON18_DP_RAMP_SEL_VAL(0x0));
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_CLK_EN_MASK,
			0x0);

	return 0;
}

static int mt8512_codec_adc_setup(
	struct snd_soc_component *component)
{
	/*8512 must disable loopback(DAC to ADC)*/
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
			ABB_ULAFE_CON0_AD_DA_LOOPBACK_DIS_MASK,
			ABB_ULAFE_CON0_AD_DA_LOOPBACK_DIS);


	snd_soc_component_update_bits(component,
			AUDUL_CON01,
			AUDUL_CON01_AUDULL_RC_TRIM_SEL_MASK,
			AUDUL_CON01_AUDULL_RC_TRIM_SEL);

	snd_soc_component_update_bits(component,
			AUDUL_CON05,
			AUDUL_CON05_AUDULR_RC_TRIM_SEL_MASK,
			AUDUL_CON05_AUDULR_RC_TRIM_SEL);

	snd_soc_component_update_bits(component,
			AP_TENSE_CON00,
			AP_TENSE_CON00_BRG_FASTUP_MASK,
			0x0);

	snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_VREF_CURRENT_ADJUST_MASK,
			AUDUL_CON22_VREF_CURRENT_ADJUST_VAL(0x7));

	snd_soc_component_update_bits(component,
			AUDUL_CON09,
			AUDUL_CON09_ADUUL_REV_MASK,
			AUDUL_CON05_AUDUL_REV_VAL(0x3));

	/*for performance,need bypass digtial gain*/
	snd_soc_component_update_bits(component, ABB_ULAFE_CON1,
			ABB_ULAFE_CON1_UL_GAIN_BYPASS_MASK,
			ABB_ULAFE_CON1_UL_GAIN_NO_BYPASS);
	/*for remove DC offset*/
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
			ABB_ULAFE_CON0_UL_IIR_ON_MASK,
			ABB_ULAFE_CON0_UL_IIR_ON);

	return 0;
}

static int mt8512_codec_get_channel_voltage(
	struct snd_soc_component *component,
	struct iio_channel *Channel, int *voltage)
{
	int ret = 0, rawvalue = 0;
	int temp_vol;

	ret = iio_read_channel_processed(Channel, &rawvalue);

	if (ret < 0) {
		dev_info(component->dev,
		"%s get raw value error %d\n", __func__, ret);
		return -1;
	}
	temp_vol = (rawvalue * 1500) / 4096; /* mV */
	*voltage = temp_vol;

	//dev_info(component->dev, "%s voltage= %d mV\n",
	//__func__, *voltage);
	return 0;
}

static int mt8512_codec_adc_rc_calibration_val(
	struct snd_soc_component *component, struct iio_channel *ch, int chinfo)
{

	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);
	unsigned long flags;

	int i = 0, reg_val;
	int Volt_ch[ADC_RC_TRIM_VALUE_MAX];
	int mini = ADC_RC_CAL_BEST_VOLT, idx = 0;

	for (i = 0; i < ADC_RC_TRIM_VALUE_MAX; i++) {
		if (chinfo) /*1: Rch,0:Lch*/
			snd_soc_component_update_bits(component,
					AUDUL_CON05,
					AUDUL_CON05_AUDULR_RC_TRIM_MASK,
					AUDUL_CON05_AUDULR_RC_TRIM_VAL(i));
		else
			snd_soc_component_update_bits(component,
					AUDUL_CON01,
					AUDUL_CON01_AUDULL_RC_TRIM_MASK,
					AUDUL_CON01_AUDULL_RC_TRIM_VAL(i));

		spin_lock_irqsave(&mt8512_codec->adda_rc_calibration_lock,
				flags);

		snd_soc_component_update_bits(component,
				AUDUL_CON21,
				AUDUL_CON21_C_CALB_EN,
				0x0);
		snd_soc_component_update_bits(component,
				AUDUL_CON21,
				AUDUL_CON21_RC_CHARGIN_EN,
				0x0);
		ndelay(1000);
		snd_soc_component_update_bits(component,
				AUDUL_CON21,
				AUDUL_CON21_C_CALB_EN,
				AUDUL_CON21_C_CALB_EN);

		snd_soc_component_update_bits(component,
				AUDUL_CON21,
				AUDUL_CON21_RC_CHARGIN_EN,
				AUDUL_CON21_RC_CHARGIN_EN);
		ndelay(1000);

		snd_soc_component_read(component, AUDUL_CON12, &reg_val);

		if (reg_val != 0x2)
			dev_info(component->dev,
			"WARNING: trim[%d] RC Cali Value Abnormal!\n", i);
		#if 0
		do {
			ndelay(1000);
			snd_soc_component_read(component, AUDUL_CON12,
						&reg_val);
		} while (reg_val != 0x2);
		#endif
		spin_unlock_irqrestore(&mt8512_codec->adda_rc_calibration_lock,
					flags);

		mt8512_codec_get_channel_voltage(component, ch, &Volt_ch[i]);
		//dev_info(component->dev, "%s Volt_ch[%d]=%d\n",
		//__func__, i,Volt_ch[i]);
		if (Volt_ch[i] == ADC_RC_CAL_BEST_VOLT) {
			idx = i;
			break;
		}
		{
			int diff = Volt_ch[i] - ADC_RC_CAL_BEST_VOLT;

			if (diff < 0)
				diff = -diff;

			if (diff < mini) {
				mini = diff;
				idx = i;
			}
		}

		ndelay(2000);
	}

	return idx;
}
static int mt8512_codec_adc_rc_calibration_setup(
	struct snd_soc_component *component)
{
		/*LDO*/
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK);
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK);
				snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO_PWDB_MASK,
			AUDUL_CON20_LDO_PWDB_MASK);
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO18_VOSEL_MASK,    /*1.85v*/
			AUDUL_CON20_LDO18_VOSEL_VAL(0x3));

		/*clock*/
	snd_soc_component_update_bits(component,
			AUDUL_CON09,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK);
	mt8512_codec_enable_26m_clk(component);

	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK);
		/*RG mode*/
	snd_soc_component_update_bits(component,
			AUDUL_CON01,
			AUDUL_CON01_AUDULL_RC_TRIM_SEL_MASK,
			AUDUL_CON01_AUDULL_RC_TRIM_SEL);

	snd_soc_component_update_bits(component,
			AUDUL_CON05,
			AUDUL_CON05_AUDULR_RC_TRIM_SEL_MASK,
			AUDUL_CON05_AUDULR_RC_TRIM_SEL);
	return 0;

}

static int mt8512_codec_adc_rc_calibration_clean(
	struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK,
			0x0);
	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO_PWDB_MASK,
			0x0);

	/*clock*/
	snd_soc_component_update_bits(component,
			AUDUL_CON09,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK,
			0x0);
	mt8512_codec_disable_26m_clk(component);

	snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK,
			0x0);
	/*calbration disable*/

	snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_C_CALB_PATH_SEL,
			AUXADC_CH_0); /*0:Lch,1:Rch*/
	snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_C_CALB_EN,
			0x0);
	snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_RC_CHARGIN_EN,
			0x0);
	return 0;
}
/*L&R ch --> ch5*/
static int mt8512_codec_adc_rc_calibration(
	struct snd_soc_component *component)
{


	struct mt8512_codec_priv *codec_data =
				snd_soc_component_get_drvdata(component);
	int trim_bestL, trim_bestR;

	if (!codec_data->ul_no_rc_cali) {

		mt8512_codec_adc_rc_calibration_setup(component);

		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_C_CALB_PATH_SEL,
			AUXADC_CH_0); /*0:Lch,1:Rch*/
		trim_bestL = mt8512_codec_adc_rc_calibration_val(component,
			codec_data->adc_ch5, AUXADC_CH_0);

		dev_info(component->dev, "trim_bestL=%d\n", trim_bestL);

		snd_soc_component_update_bits(component,
			AUDUL_CON01,
			AUDUL_CON01_AUDULL_RC_TRIM_MASK,
			AUDUL_CON01_AUDULL_RC_TRIM_VAL(trim_bestL));

		ndelay(5000);

		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_C_CALB_PATH_SEL,
			AUXADC_CH_1 << 23); /*0:Lch,1:Rch*/
		trim_bestR = mt8512_codec_adc_rc_calibration_val(component,
				codec_data->adc_ch5, AUXADC_CH_1);

		dev_info(component->dev, "trim_bestR=%d\n", trim_bestR);

		snd_soc_component_update_bits(component,
			AUDUL_CON05,
			AUDUL_CON05_AUDULR_RC_TRIM_MASK,
			AUDUL_CON05_AUDULR_RC_TRIM_VAL(trim_bestR));

		mt8512_codec_adc_rc_calibration_clean(component);
		codec_data->ul_no_rc_cali = 1;
	}
	return 0;
}


static int mt8512_codec_ana_dac_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_CLK26MHZ_DIV_EN_MASK,
			AUDDL_CON00_CLK26MHZ_DIV_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_CK_6P5M_FIFO_EN_MASK,
			AUDDL_CON18_CK_6P5M_FIFO_EN_MASK);
		mt8512_codec_enable_26m_clk(component);

		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENVO_MASK,
			AUDDL_CON18_ENVO_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S0_MASK,
			AUDDL_CON18_DP_S0_MASK);
		usleep_range(10, 11);

		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENDP_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S1_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_RAMP_SEL_MASK,
			AUDDL_CON18_DP_RAMP_SEL_VAL(0x0));

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_CLK26MHZ_DIV_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_CK_6P5M_FIFO_EN_MASK,
			0x0);
		mt8512_codec_disable_26m_clk(component);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENDP_MASK,
			AUDDL_CON18_ENDP_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S1_MASK,
			AUDDL_CON18_DP_S1_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_RAMP_SEL_MASK,
			AUDDL_CON18_DP_RAMP_SEL_VAL(0x2));

		usleep_range(10, 11);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_ENVO_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_DP_S0_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_dac_vcm_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_V2I_PWDB_MASK,
			AUDDL_CON00_V2I_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_IDACL_PWDB_MASK,
			AUDDL_CON01_IDACL_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_IDACR_PWDB_MASK,
			AUDDL_CON01_IDACR_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_RELATCH_EN_MASK,
			AUDDL_CON18_RELATCH_EN_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDDL_CON00,
			AUDDL_CON00_V2I_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_IDACL_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON01,
			AUDDL_CON01_IDACR_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDDL_CON18,
			AUDDL_CON18_RELATCH_EN_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON09,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK);
		mt8512_codec_enable_26m_clk(component);

		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON09,
			AUDUL_CON09_AUDUL_CHOPPER_CLK_EN_MASK,
			0x0);
		mt8512_codec_disable_26m_clk(component);

		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_GLBIAS_EN_AUDUL_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_int1_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_INT1_RESET_MASK,
			AUDUL_CON22_AUDULL_INT1_RESET_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_INT1_RESET_MASK,
			AUDUL_CON23_AUDULR_INT1_RESET_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_INT_CHP_EN_MASK,
			AUDUL_CON00_AUDULL_INT_CHP_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_INT_CHP_EN_MASK,
			AUDUL_CON04_AUDULR_INT_CHP_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PDN_INT1OP_MASK,
			AUDUL_CON22_AUDULL_PDN_INT1OP_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_PDN_INT1OP_MASK,
			AUDUL_CON23_AUDULR_PDN_INT1OP_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_INT1_RESET_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_INT1_RESET_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_INT_CHP_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_INT_CHP_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PDN_INT1OP_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_PDN_INT1OP_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_int2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_INT2_RESET_MASK,
			AUDUL_CON22_AUDULL_INT2_RESET_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_INT2_RESET_MASK,
			AUDUL_CON23_AUDULR_INT2_RESET_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_INT2_RESET_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_INT2_RESET_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_saradc_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_EN_MASK,
			AUDUL_CON22_AUDULL_SARADC_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_EN_MASK,
			AUDUL_CON24_AUDULR_SARADC_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_RESET_MASK,
			AUDUL_CON22_AUDULL_SARADC_RESET_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_RESET_MASK,
			AUDUL_CON24_AUDULR_SARADC_RESET_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_MASK,
			AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_VAL(0x1));
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_MASK,
			AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_VAL(0x1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_RESET_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_RESET_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_MASK,
			AUDUL_CON22_AUDULL_SARADC_SA_DLY_SEL_VAL(0x0));
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_MASK,
			AUDUL_CON24_AUDULR_SARADC_SA_DLY_SEL_VAL(0x0));
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_left_pga_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_OUTPUT_EN_MASK,
			AUDUL_CON22_AUDULL_PGA_OUTPUT_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_CHP_EN_MASK,
			AUDUL_CON22_AUDULL_PGA_CHP_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_PGA_CMFB_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_PWDB_MASK,
			AUDUL_CON22_AUDULL_PGA_PWDB_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_OUTPUT_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_CHP_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON00,
			AUDUL_CON00_AUDULL_PGA_CMFB_EN_MASK,
			AUDUL_CON00_AUDULL_PGA_CMFB_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON22,
			AUDUL_CON22_AUDULL_PGA_PWDB_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_right_pga_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_PGA_OUTPUT_EN_MASK,
			AUDUL_CON23_AUDULR_PGA_OUTPUT_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_CHP_EN_MASK,
			AUDUL_CON04_AUDULR_PGA_CHP_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_CMFB_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_PGA_PWDB_MASK,
			AUDUL_CON24_AUDULR_PGA_PWDB_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON23,
			AUDUL_CON23_AUDULR_PGA_OUTPUT_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_CHP_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON04,
			AUDUL_CON04_AUDULR_PGA_CMFB_EN_MASK,
			AUDUL_CON04_AUDULR_PGA_CMFB_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON24,
			AUDUL_CON24_AUDULR_PGA_PWDB_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_ldo_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO_PWDB_MASK,
			AUDUL_CON20_LDO_PWDB_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO18_VOSEL_MASK,    /*1.85v*/
			AUDUL_CON20_LDO18_VOSEL_VAL(0x3));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULL_LDO08_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUDULR_LDO08_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO_PWDB_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_LDO18_VOSEL_MASK,
			AUDUL_CON20_LDO18_VOSEL_VAL(0x4));
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_vcm_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VCM_EN_MASK,
			AUDUL_CON21_VCM_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VCM_CHP_EN_MASK,
			0x0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VCM_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VCM_CHP_EN_MASK,
			0x0);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_ana_adc_vref_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VREF_EN_MASK,
			AUDUL_CON21_VREF_EN_MASK);
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VREF_CHP_EN_MASK,
			0x0);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VREF_EN_MASK,
			0x0);
		snd_soc_component_update_bits(component,
			AUDUL_CON21,
			AUDUL_CON21_VREF_CHP_EN_MASK,
			0x0);

		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_dig_adda_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8512_codec_enable_adda_afe_on(component);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt8512_codec_disable_adda_afe_on(component);
		break;
	default:
		return 0;
	}

	return 0;
}

static int mt8512_codec_micbias0_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUD_PWDB_MICBIAS_MASK,
			AUDUL_CON20_AUD_PWDB_MICBIAS_MASK);
		usleep_range(mt8512_codec->micbias0_setup_time_us,
			mt8512_codec->micbias0_setup_time_us + 1);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_update_bits(component,
			AUDUL_CON20,
			AUDUL_CON20_AUD_PWDB_MICBIAS_MASK,
			0x0);
		usleep_range(mt8512_codec->micbias0_setup_time_us,
			mt8512_codec->micbias0_setup_time_us + 1);
		break;
	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mt8512_codec_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("AIF RX", "AIF Playback", 0,
		SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Left DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("AIF TX", "AIF Capture", 0,
		SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("Left ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("Right ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA_E("Left PGA", SND_SOC_NOPM, 0, 0, NULL, 0,
		mt8512_codec_ana_adc_left_pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("Right PGA", SND_SOC_NOPM, 0, 0, NULL, 0,
		mt8512_codec_ana_adc_right_pga_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("INT1", SND_SOC_NOPM, 0, 0, NULL, 0,
		mt8512_codec_ana_adc_int1_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SARADC", SND_SOC_NOPM, 0, 0, NULL, 0,
		mt8512_codec_ana_adc_saradc_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("INT2", SND_SOC_NOPM, 0, 0, NULL, 0,
		mt8512_codec_ana_adc_int2_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("DIG_ADC_CLK", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DIG_DAC_CLK", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_CLK", 5, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_dac_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_LATCH", 4,
		AUDDL_CON18, 5, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ANA_DAC_VCM", 3, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_dac_vcm_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DIG_ADDA_CLK", 2, SND_SOC_NOPM, 0, 0,
		mt8512_codec_dig_adda_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_LDO", 3, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_adc_ldo_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_CLK", 4, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_adc_clk_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_VCM", 5, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_adc_vcm_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ANA_ADC_VREF", 6, SND_SOC_NOPM, 0, 0,
		mt8512_codec_ana_adc_vref_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("AU_MICBIAS0", 7, SND_SOC_NOPM, 0, 0,
		mt8512_codec_micbias0_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AU_VIN0"),
	SND_SOC_DAPM_INPUT("AU_VIN1"),
	SND_SOC_DAPM_OUTPUT("AU_VOL"),
	SND_SOC_DAPM_OUTPUT("AU_VOR"),
};

static const struct snd_soc_dapm_route mt8512_codec_intercon[] = {
	{ "AIF RX", NULL, "ANA_DAC_CLK" },
	{ "AIF RX", NULL, "DIG_ADDA_CLK" },
	{ "AIF RX", NULL, "ANA_DAC_LATCH" },
	{ "AIF RX", NULL, "ANA_DAC_VCM" },
	{ "AIF RX", NULL, "DIG_DAC_CLK" },

	{ "AIF TX", NULL, "ANA_ADC_CLK" },
	{ "AIF TX", NULL, "DIG_ADDA_CLK" },
	{ "AIF TX", NULL, "ANA_ADC_LDO" },
	{ "AIF TX", NULL, "ANA_ADC_VCM" },
	{ "AIF TX", NULL, "ANA_ADC_VREF" },
	{ "AIF TX", NULL, "DIG_ADC_CLK" },

	{"Left DAC", NULL, "AIF RX"},
	{"Right DAC", NULL, "AIF RX"},
	{"AU_VOL", NULL, "Left DAC"},
	{"AU_VOR", NULL, "Right DAC"},

	{"AIF TX", NULL, "Left ADC"},
	{"AIF TX", NULL, "Right ADC"},

	{"Left ADC", NULL, "Left PGA"},
	{"Right ADC", NULL, "Right PGA"},

	{"Left PGA", NULL, "INT1"},
	{"Right PGA", NULL, "INT1"},

	{"INT1", NULL, "SARADC"},
	{"SARADC", NULL, "INT2"},

	{"INT2", NULL, "AU_VIN0"},
	{"INT2", NULL, "AU_VIN1"},

	{"AU_VIN0", NULL, "AU_MICBIAS0"},
	{"AU_VIN1", NULL, "AU_MICBIAS0"},
};

struct mt8512_codec_rate {
	unsigned int rate;
	unsigned int regvalue;
};

static const struct mt8512_codec_rate mt8512_codec_ul_voice_modes[] = {
	{ .rate =  8000, .regvalue = 0, },
	{ .rate = 16000, .regvalue = 1, },
	{ .rate = 32000, .regvalue = 2, },
	{ .rate = 48000, .regvalue = 3, },
};

static const struct mt8512_codec_rate mt8512_codec_ul_rates[] = {
	{ .rate =  8000, .regvalue = 0, },
	{ .rate = 16000, .regvalue = 0, },
	{ .rate = 32000, .regvalue = 0, },
	{ .rate = 48000, .regvalue = 1, },
};

static const struct mt8512_codec_rate mt8512_codec_dl_voice_modes[] = {
	{ .rate =   8000, .regvalue = 1, },
	{ .rate =  11025, .regvalue = 0, },
	{ .rate =  12000, .regvalue = 0, },
	{ .rate =  16000, .regvalue = 1, },
	{ .rate =  22050, .regvalue = 0, },
	{ .rate =  24000, .regvalue = 0, },
	{ .rate =  32000, .regvalue = 0, },
	{ .rate =  44100, .regvalue = 0, },
	{ .rate =  48000, .regvalue = 0, },
};

static const struct mt8512_codec_rate mt8512_codec_dl_input_modes[] = {
	{ .rate =   8000, .regvalue = 0, },
	{ .rate =  11025, .regvalue = 1, },
	{ .rate =  12000, .regvalue = 2, },
	{ .rate =  16000, .regvalue = 3, },
	{ .rate =  22050, .regvalue = 4, },
	{ .rate =  24000, .regvalue = 5, },
	{ .rate =  32000, .regvalue = 6, },
	{ .rate =  44100, .regvalue = 7, },
	{ .rate =  48000, .regvalue = 8, },
};

static const struct mt8512_codec_rate mt8512_codec_dl_rates[] = {
	{ .rate =   8000, .regvalue = 0, },
	{ .rate =  11025, .regvalue = 1, },
	{ .rate =  12000, .regvalue = 2, },
	{ .rate =  16000, .regvalue = 3, },
	{ .rate =  22050, .regvalue = 4, },
	{ .rate =  24000, .regvalue = 5, },
	{ .rate =  32000, .regvalue = 6, },
	{ .rate =  44100, .regvalue = 7, },
	{ .rate =  48000, .regvalue = 8, },
};

static int mt8512_codec_rate_to_val(const struct mt8512_codec_rate *table,
	int table_nums, int rate)
{
	int i;

	for (i = 0; i < table_nums; i++)
		if (table[i].rate == rate)
			return table[i].regvalue;

	return -EINVAL;
}

struct mt8512_codec_rate_ctrl {
	unsigned int top_ctrl_reg;
	unsigned int top_ctrl_w_val;
	unsigned int top_ctrl_r_val;
};

static const struct mt8512_codec_rate_ctrl top_ctrl_regs[] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = {
		.top_ctrl_reg	= ABB_AFE_CON11,
		.top_ctrl_w_val	= ABB_AFE_CON11_TOP_CTRL,
		.top_ctrl_r_val	= ABB_AFE_CON11_TOP_CTRL_STATUS,
	},
	[SNDRV_PCM_STREAM_CAPTURE] = {
		.top_ctrl_reg	= -1,
		.top_ctrl_w_val	= -1,
		.top_ctrl_r_val	= -1,
	},
};

static int mt8512_codec_valid_new_rate(struct snd_soc_component *component,
	int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int reg = top_ctrl_regs[stream].top_ctrl_reg;
		unsigned int w_val = top_ctrl_regs[stream].top_ctrl_w_val;
		unsigned int r_val = top_ctrl_regs[stream].top_ctrl_r_val;
		unsigned int reg_val;

		/* toggle top_ctrl status */
		if (snd_soc_component_read(component, reg, &reg_val) < 0)
			return 0;

		if (reg_val & r_val) {
			snd_soc_component_update_bits(component, reg,
				w_val, 0x0);
		} else {
			snd_soc_component_update_bits(component, reg,
				w_val, w_val);
		}
	}

	return 0;
}

static int mt8512_codec_setup_ul_rate(struct snd_soc_component *component,
	int rate)
{
	uint32_t val = 0;

	val = mt8512_codec_rate_to_val(mt8512_codec_ul_voice_modes,
		ARRAY_SIZE(mt8512_codec_ul_voice_modes), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_VOICE_MODE_MASK,
		ABB_ULAFE_CON0_UL_VOICE_MODE(val));

	return 0;

err:
	dev_info(component->dev, "%s error to setup ul rate\n", __func__);
	return -EINVAL;
}

static int mt8512_codec_setup_dl_rate(struct snd_soc_component *component,
	int rate)
{
	uint32_t val = 0;

	val = mt8512_codec_rate_to_val(mt8512_codec_dl_input_modes,
		ARRAY_SIZE(mt8512_codec_dl_input_modes), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE_MASK,
		AFE_ADDA_DL_SRC2_CON0_DL_INPUT_MODE_VAL(val));

	val = mt8512_codec_rate_to_val(mt8512_codec_dl_rates,
		ARRAY_SIZE(mt8512_codec_dl_rates), rate);
	if (val < 0)
		goto err;

	snd_soc_component_update_bits(component, ABB_AFE_CON1,
		ABB_AFE_CON1_DL_RATE_MASK,
		ABB_AFE_CON1_DL_RATE(val));

	mt8512_codec_valid_new_rate(component, SNDRV_PCM_STREAM_PLAYBACK);

	return 0;

err:
	dev_info(component->dev, "%s error to setup dl rate\n", __func__);
	return -EINVAL;
}

static int mt8512_codec_configure_ul(struct snd_soc_component *component)
{
	return 0;
}

static int mt8512_codec_configure_dl_nle(struct snd_soc_component *component,
	int rate)
{
	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} config_regs[] = {
		{
			AFE_NLE_ZCD_LCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_VAL(0x0),
		},
		{
			AFE_NLE_ZCD_LCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_VAL(0x0),
		},
		{
			AFE_NLE_PWR_DET_LCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_MASK,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_DEF_VAL,
		},
		{
			AFE_NLE_PWR_DET_LCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_MASK,
			/* -40dB: 10^(-40/20) * 2^20 */
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_VAL(0x28F6),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_VAL(0x1),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_VAL(0x3),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_VAL(0xA),
		},
		{
			AFE_NLE_GAIN_ADJ_LCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_VAL(0x2),
		},
		{
			AFE_NLE_GAIN_IMP_LCH_CFG0,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_MASK,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_VAL(0xC),
		},
		{
			AFE_NLE_ZCD_RCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_CHECK_MODE_VAL(0x0),
		},
		{
			AFE_NLE_ZCD_RCH_CFG,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_MASK,
			AFE_NLE_ZCD_CH_CFG_ZCD_MODE_SEL_VAL(0x0),
		},
		{
			AFE_NLE_PWR_DET_RCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_MASK,
			AFE_NLE_PWR_DET_CH_CFG_H2L_HOLD_TIME_DEF_VAL,
		},
		{
			AFE_NLE_PWR_DET_RCH_CFG,
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_MASK,
			/* -40dB: 10^(-40/20) * 2^20 */
			AFE_NLE_PWR_DET_CH_CFG_NLE_VTH_VAL(0x28F6),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_ADJ_BYPASS_ZCD_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_TIME_OUT_VAL(0x1),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_HOLD_TIME_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_JUMP_VAL(0x0),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_GAIN_STEP_PER_ZCD_VAL(0x3),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MIN_VAL(0xA),
		},
		{
			AFE_NLE_GAIN_ADJ_RCH_CFG0,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_MASK,
			AFE_NLE_GAIN_ADJ_CH_CFG0_AG_MAX_VAL(0x2),
		},
		{
			AFE_NLE_GAIN_IMP_RCH_CFG0,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_MASK,
			AFE_NLE_GAIN_IMP_CH_CFG0_AG_DELAY_VAL(0xC),
		},
	};
	int i;

	snd_soc_component_update_bits(component, AFE_NLE_PRE_BUF_CFG,
		AFE_NLE_PRE_BUF_CFG_POINT_END_MASK,
		(rate * (H2L_HOLD_TIME_MS - 1) / 1000) - 1);

	for (i = 0; i < ARRAY_SIZE(config_regs); i++)
		snd_soc_component_update_bits(component, config_regs[i].reg,
			config_regs[i].mask, config_regs[i].val);
	return 0;
}

static int mt8512_codec_configure_dl(struct snd_soc_component *component,
	int rate)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON1,
		AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_MASK,
		AFE_ADDA_DL_SRC2_CON1_GAIN_CTRL_VAL(0xf74f));

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_MUTE_OFF_CTRL_MASK,
		AFE_ADDA_DL_SRC2_CON0_MUTE_OFF);

	mt8512_codec_valid_new_rate(component, SNDRV_PCM_STREAM_PLAYBACK);

	snd_soc_component_update_bits(component, ABB_AFE_CON5,
		ABB_AFE_CON5_SDM_GAIN_VAL_MASK,
		ABB_AFE_CON5_SDM_GAIN_VAL(0x34));

	if (mt8512_codec->dl_nle_support)
		mt8512_codec_configure_dl_nle(component, rate);

	return 0;
}

static int mt8512_codec_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_component *component = dai->component;
	int rate = params_rate(params);

	dev_dbg(component->dev, "%s rate = %dHz\n", __func__, rate);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = mt8512_codec_setup_ul_rate(component, rate);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mt8512_codec_setup_dl_rate(component, rate);

	if (ret < 0) {
		dev_info(component->dev, "%s error to setup rate\n", __func__);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt8512_codec_configure_ul(component);
	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt8512_codec_configure_dl(component, rate);

	return ret;
}

static int mt8512_codec_enable_ul(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_SRC_ON_MASK,
		ABB_ULAFE_CON0_UL_SRC_ON);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_FIFO_ON_MASK,
		ABB_ULAFE_CON0_UL_FIFO_ON);

	return 0;
}

static int mt8512_codec_enable_dl_nle(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_SW_RSTB_MASK,
		AFE_NLE_CFG_SW_RSTB_VAL(0x1));

	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_AFE_NLE_ON_MASK,
		AFE_NLE_CFG_AFE_NLE_ON);

	return 0;
}

static int mt8512_codec_enable_dl(struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);

	if (mt8512_codec->dl_nle_support)
		mt8512_codec_enable_dl_nle(component);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_ADDA_INTF_ON_MASK,
		AFE_ADDA_UL_DL_CON0_ADDA_INTF_ON);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON_MASK,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_MASK,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON);

	snd_soc_component_update_bits(component, ABB_AFE_CON0,
		ABB_AFE_CON0_DL_EN_MASK,
		ABB_AFE_CON0_DL_EN);

	return 0;
}

static int mt8512_codec_disable_ul(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_FIFO_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, ABB_ULAFE_CON0,
		ABB_ULAFE_CON0_UL_SRC_ON_MASK,
		0x0);

	return 0;
}

static int mt8512_codec_disable_dl_nle(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_AFE_NLE_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_NLE_CFG,
		AFE_NLE_CFG_SW_RSTB_MASK,
		AFE_NLE_CFG_SW_RSTB_VAL(0x0));

	return 0;
}

static int mt8512_codec_disable_dl(struct snd_soc_component *component)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_component_get_drvdata(component);

	snd_soc_component_update_bits(component, ABB_AFE_CON0,
		ABB_AFE_CON0_DL_EN_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_GAIN_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_DL_SRC2_CON0,
		AFE_ADDA_DL_SRC2_CON0_DL_SRC_ON_MASK,
		0x0);

	if (mt8512_codec->dl_nle_support)
		mt8512_codec_disable_dl_nle(component);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_ADDA_INTF_ON_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_DL_SW_RESET_MASK,
		0x0);

	snd_soc_component_update_bits(component, AFE_ADDA_UL_DL_CON0,
		AFE_ADDA_UL_DL_CON0_DL_SW_RESET_MASK,
		0x1 << 15);

	return 0;
}

static int mt8512_codec_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			mt8512_codec_disable_ul(component);
		else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mt8512_codec_disable_dl(component);
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			mt8512_codec_enable_ul(component);
		else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			mt8512_codec_enable_dl(component);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define MT8512_CODEC_DL_RATES SNDRV_PCM_RATE_8000_48000
#define MT8512_CODEC_UL_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)

#define MT8512_CODEC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops mt8512_codec_dai_ops = {
	.hw_params	= mt8512_codec_hw_params,
	.trigger	= mt8512_codec_trigger,
};

static struct snd_soc_dai_driver mt8512_codec_dai[] = {
	{
		.name = "mt8512-codec-dai",
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MT8512_CODEC_DL_RATES,
			.formats = MT8512_CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MT8512_CODEC_UL_RATES,
			.formats = MT8512_CODEC_FORMATS,
		},
		.ops = &mt8512_codec_dai_ops,
	},
};

#ifdef CONFIG_DEBUG_FS
struct mt8512_codec_reg_attr {
	uint32_t offset;
	char *name;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt8512_codec_reg_attr mt8512_codec_dump_reg_list[] = {
	/* audio_top_sys */
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_TOP_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_NLE_CFG),
	DUMP_REG_ENTRY(AFE_NLE_PRE_BUF_CFG),
	DUMP_REG_ENTRY(AFE_NLE_PWR_DET_LCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_ZCD_LCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_LCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_IMP_LCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_PWE_DET_LCH_MON),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_LCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_LCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_PWR_DET_RCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_ZCD_RCH_CFG),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_RCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_IMP_RCH_CFG0),
	DUMP_REG_ENTRY(AFE_NLE_PWE_DET_RCH_MON),
	DUMP_REG_ENTRY(AFE_NLE_GAIN_ADJ_RCH_MON0),
	DUMP_REG_ENTRY(AFE_NLE_RCH_MON0),
	DUMP_REG_ENTRY(ABB_AFE_CON0),
	DUMP_REG_ENTRY(ABB_AFE_CON1),
	DUMP_REG_ENTRY(ABB_AFE_CON2),
	DUMP_REG_ENTRY(ABB_AFE_CON3),
	DUMP_REG_ENTRY(ABB_AFE_CON4),
	DUMP_REG_ENTRY(ABB_AFE_CON5),
	DUMP_REG_ENTRY(ABB_AFE_CON6),
	DUMP_REG_ENTRY(ABB_AFE_CON7),
	DUMP_REG_ENTRY(ABB_AFE_CON8),
	DUMP_REG_ENTRY(ABB_AFE_CON10),
	DUMP_REG_ENTRY(ABB_AFE_CON11),
	DUMP_REG_ENTRY(ABB_AFE_SDM_TEST),
	DUMP_REG_ENTRY(AFE_AD_UL_DL_CON0),
	DUMP_REG_ENTRY(ABB_ULAFE_CON0),
	DUMP_REG_ENTRY(ABB_ULAFE_CON1),

	/* apmixedsys */
	DUMP_REG_ENTRY(AP_TENSE_CON00),
	DUMP_REG_ENTRY(AUDDL_CON00),
	DUMP_REG_ENTRY(AUDDL_CON01),
	DUMP_REG_ENTRY(AUDDL_CON02),
	DUMP_REG_ENTRY(AUDDL_CON03),
	DUMP_REG_ENTRY(AUDUL_CON00),
	DUMP_REG_ENTRY(AUDUL_CON01),
	DUMP_REG_ENTRY(AUDUL_CON04),
	DUMP_REG_ENTRY(AUDUL_CON05),
	DUMP_REG_ENTRY(AUDUL_CON09),
	DUMP_REG_ENTRY(AUDDL_CON18),
	DUMP_REG_ENTRY(AUDUL_CON19),
	DUMP_REG_ENTRY(AUDUL_CON20),
	DUMP_REG_ENTRY(AUDUL_CON21),
	DUMP_REG_ENTRY(AUDUL_CON22),
	DUMP_REG_ENTRY(AUDUL_CON23),
	DUMP_REG_ENTRY(AUDUL_CON24),
};

static ssize_t mt8512_codec_debug_read(struct file *file,
	char __user *user_buf,
	size_t count, loff_t *pos)
{
	struct mt8512_codec_priv *mt8512_codec = file->private_data;
	struct snd_soc_codec *codec = mt8512_codec->codec;
	struct snd_soc_component *component = &codec->component;
	ssize_t ret, i;
	char *buf;
	int n = 0;
	unsigned int val;

	if (*pos < 0 || !count)
		return -EINVAL;

	buf = kzalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(mt8512_codec_dump_reg_list); i++) {
		if (snd_soc_component_read(component,
			mt8512_codec_dump_reg_list[i].offset, &val) < 0)
			val = 0;
		n += scnprintf(buf + n, count - n, "%s = 0x%x\n",
			mt8512_codec_dump_reg_list[i].name, val);
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);

	kfree(buf);

	return ret;
}


static ssize_t mt8512_codec_debug_write(struct file *file,
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

	struct mt8512_codec_priv *mt8512_codec = file->private_data;
	struct snd_soc_codec *codec = mt8512_codec->codec;
	struct snd_soc_component *component = &codec->component;

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

	snd_soc_component_write(component, reg, value);

	return buf_size;
}

static const struct file_operations mt8512_codec_debug_ops = {
	.open = simple_open,
	.read = mt8512_codec_debug_read,
	.write = mt8512_codec_debug_write,
	.llseek = default_llseek,
};
#endif

static void mt8512_codec_init_regs(struct snd_soc_component *component)
{
	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} init_regs[] = {
		{
			AUDUL_CON20,
			AUDUL_CON20_AUDULL_LDO08_VOSEL_MASK,
			AUDUL_CON20_AUDULL_LDO08_VOSEL_VAL(0x3), /*0.9v*/
		},
		{
			AUDUL_CON20,
			AUDUL_CON20_AUDULR_LDO08_VOSEL_MASK,
			AUDUL_CON20_AUDULR_LDO08_VOSEL_VAL(0x3), /*0.9v*/
		},
		{
			AUDUL_CON01,
			AUDUL_CON01_AUDULL_RC_TRIM_MASK,
			AUDUL_CON01_AUDULL_RC_TRIM_VAL(0x20),
		},
		{
			AUDUL_CON05,
			AUDUL_CON05_AUDULR_RC_TRIM_MASK,
			AUDUL_CON05_AUDULR_RC_TRIM_VAL(0x20),
		},


	};
	int i;

	for (i = 0; i < ARRAY_SIZE(init_regs); i++)
		snd_soc_component_update_bits(component, init_regs[i].reg,
			init_regs[i].mask, init_regs[i].val);

	mt8512_codec_dac_depop_setup(component);

	mt8512_codec_adc_setup(component);

	mt8512_codec_adc_rc_calibration(component);
}


static struct regmap *mt8512_codec_get_regmap_of(const char *phandle_name,
		struct device *dev)
{
	struct regmap *regmap = NULL;

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
		phandle_name);
	if (!IS_ERR(regmap)) {
		dev_dbg(dev, "%s found regmap of syscon node %s\n",
			__func__, phandle_name);
		return regmap;
	}
	dev_info(dev, "%s failed to get regmap of syscon node %s\n",
		__func__, phandle_name);

	return NULL;
}

static const char * const mt8512_codec_regmap_of_str[REGMAP_NUMS] = {
	"mediatek,afe-regmap",
	"mediatek,apmixedsys-regmap",
};

static int mt8512_codec_parse_of(struct snd_soc_codec *codec)
{
	struct device *dev = codec->dev;
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int i;

	for (i = 0; i < REGMAP_NUMS; i++) {
		mt8512_codec->regmap_modules[i] = mt8512_codec_get_regmap_of(
				mt8512_codec_regmap_of_str[i], dev);
		if (!mt8512_codec->regmap_modules[i]) {
			dev_info(dev, "%s failed to get %s\n", __func__,
				mt8512_codec_regmap_of_str[i]);
			ret = -EPROBE_DEFER;
			break;
		}
	}

	of_property_read_u32(dev->of_node,
		"mediatek,micbias0-setup-time-us",
		&mt8512_codec->micbias0_setup_time_us);

	of_property_read_u32(dev->of_node,
		"mediatek,downlink-nle-support",
		&mt8512_codec->dl_nle_support);

	return ret;
}

static int mt8512_codec_clk_probe(struct snd_soc_codec *codec)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);

	mt8512_codec->clk = devm_clk_get(codec->dev, "bus");
	if (IS_ERR(mt8512_codec->clk)) {
		dev_dbg(codec->dev, "%s devm_clk_get %s fail\n",
			__func__, "bus");
		return PTR_ERR(mt8512_codec->clk);
	}

	return clk_prepare_enable(mt8512_codec->clk);
}

static int mt8512_codec_probe(struct snd_soc_codec *codec)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);

	mt8512_codec->codec = codec;

	ret = mt8512_codec_parse_of(codec);
	if (ret < 0)
		return ret;

	spin_lock_init(&mt8512_codec->adda_afe_on_lock);
	spin_lock_init(&mt8512_codec->adda_26mclk_lock);
	spin_lock_init(&mt8512_codec->adda_rc_calibration_lock);

	ret = mt8512_codec_clk_probe(codec);
	if (ret < 0)
		return ret;

	mt8512_codec_init_regs(&codec->component);

#ifdef CONFIG_DEBUG_FS
	mt8512_codec->debugfs = debugfs_create_file("mt8512_codec_regs",
			0644, NULL, mt8512_codec, &mt8512_codec_debug_ops);
#endif

	return ret;
}

static int mt8512_codec_remove(struct snd_soc_codec *codec)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(mt8512_codec->debugfs);
#endif

	clk_disable_unprepare(mt8512_codec->clk);

	mt8512_codec->codec = NULL;

	return 0;
}

static int mt8512_codec_suspend(struct snd_soc_codec *codec)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);

	clk_disable_unprepare(mt8512_codec->clk);
	return 0;
}

static int mt8512_codec_resume(struct snd_soc_codec *codec)
{
	struct mt8512_codec_priv *mt8512_codec =
		snd_soc_codec_get_drvdata(codec);

	return clk_prepare_enable(mt8512_codec->clk);
}

static struct regmap *mt8512_codec_get_regmap(struct device *dev)
{
	struct mt8512_codec_priv *mt8512_codec = dev_get_drvdata(dev);

	return mt8512_codec->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_mt8512_codec = {
	.probe = mt8512_codec_probe,
	.remove = mt8512_codec_remove,
	.suspend = mt8512_codec_suspend,
	.resume = mt8512_codec_resume,
	.get_regmap = mt8512_codec_get_regmap,

	.component_driver = {
		.controls		= mt8512_codec_snd_controls,
		.num_controls		= ARRAY_SIZE(mt8512_codec_snd_controls),
		.dapm_widgets		= mt8512_codec_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(mt8512_codec_dapm_widgets),
		.dapm_routes		= mt8512_codec_intercon,
		.num_dapm_routes	= ARRAY_SIZE(mt8512_codec_intercon),
	},
};

static int mt8512_codec_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt8512_codec_priv *mt8512_codec = NULL;
	int ret;

	dev_dbg(dev, "%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT8512_CODEC_NAME);
		dev_dbg(dev, "%s set dev name %s\n", __func__, dev_name(dev));
	}

	mt8512_codec = devm_kzalloc(dev,
			sizeof(struct mt8512_codec_priv), GFP_KERNEL);
	if (!mt8512_codec)
		return -ENOMEM;

	/* get regmap of codec */
	mt8512_codec->regmap = devm_regmap_init(dev, NULL, mt8512_codec,
		&mt8512_codec_regmap);
	if (IS_ERR(mt8512_codec->regmap)) {
		dev_info(dev, "%s failed to get regmap of codec\n", __func__);
		devm_kfree(dev, mt8512_codec);
		mt8512_codec->regmap = NULL;
		return -EINVAL;
	}

	dev_set_drvdata(dev, mt8512_codec);

	mt8512_codec->adc_ch5 = iio_channel_get(dev, "adc-ch5");
	pr_err("get iio adc-ch5 = %d\n", IS_ERR(mt8512_codec->adc_ch5));

	if (IS_ERR(mt8512_codec->adc_ch5)) {
		ret = PTR_ERR(mt8512_codec->adc_ch5);
		mt8512_codec->ul_no_rc_cali = 1;
		dev_err(dev, "IIO channel not found:%d\n", ret);
	}

	return snd_soc_register_codec(dev, &soc_codec_dev_mt8512_codec,
			mt8512_codec_dai, ARRAY_SIZE(mt8512_codec_dai));
}

static const struct of_device_id mt8512_codec_of_match[] = {
	{.compatible = "mediatek," MT8512_CODEC_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8512_codec_of_match);

static struct platform_driver mt8512_codec_driver = {
	.probe = mt8512_codec_dev_probe,
	.driver = {
		.name = MT8512_CODEC_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mt8512_codec_of_match,
	},
};

module_platform_driver(mt8512_codec_driver);

MODULE_DESCRIPTION("ASoC MT8512 codec driver");
MODULE_LICENSE("GPL");
