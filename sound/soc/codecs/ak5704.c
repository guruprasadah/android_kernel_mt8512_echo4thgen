/*
 * ak5704.c  --  audio driver for ak5704
 *
 * Copyright (C) 2019 Asahi Kasei Microdevices Corporation
 *	Author				  Date		  Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *						19/03/29	1.0		KERNEL_4_9_XX
 *						19/05/07	1.1		KERNEL_4_9_XX
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include "ak5704.h"

//#define AK5704_DEBUG

#ifdef AK5704_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

//#define KERNEL_3_18_XX
//#define KERNEL_4_4_XX
#define KERNEL_4_9_XX

/* AK5704 Codec Private Data */
struct ak5704_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct regulator *reset_adc_regulator;
	int fs;				  //  0:11.2896MHz 1:11.288
	int tmdMode;		  //  0:Stereo,  1:TDM128	2:TDM256 3: 512fs
	int pllMode;		  //  0: Ext Mode,	 1: BCLK PLL Mode 2:MCLK PLL Mode
	int masterMode;		  //  0: Slave Mode, 1: Master Mode
	int mclkFreq;		  //  0:128fs, 1:192fs, 2:256fs, 3:384fs, 4:512fs, 5:768fs, 6:1024fs
	int bickFreq;		  //  0:32fs, 1:64fs
	int lpf1Fc;
	int hpf1Fc;
	int lpf2Fc;
	int hpf2Fc;
	int valpfFc;
	int vahpfFc;
	int cmMode;
	int vadminth;
	int vadnldth;
};

/* ak5704 register cache & default register settings */
static const struct reg_default ak5704_reg[] = {
	{ 0x00, 0x00 },   /*  0x00	AK5704_00_FLOW_CONTROLL					*/
	{ 0x01, 0x00 },   /*  0x01	AK5704_01_POWER_MANAGEMENT_1			*/
	{ 0x02, 0x00 },   /*  0x02	AK5704_02_POWER_MANAGEMENT_2			*/
	{ 0x03, 0x00 },   /*  0x03	AK5704_03_POWER_MANAGEMENT_3			*/
	{ 0x04, 0x00 },   /*  0x04	AK5704_04_MIC_INPUT_MIC_POWER_SETTING	*/
	{ 0x05, 0x66 },   /*  0x05	AK5704_05_MIC_AMPLIFER_1_GAIN			*/
	{ 0x06, 0x66 },   /*  0x06	AK5704_06_MIC_AMPLIFER_2_GAIN			*/
	{ 0x07, 0x00 },   /*  0x07	AK5704_07_DIGITAL_MIC_SETTING			*/
	{ 0x08, 0x0A },   /*  0x08	AK5704_08_CLOCK_MODE_SELECT				*/
	{ 0x09, 0x00 },   /*  0x09	AK5704_09_PLL_CLK_SOURCE_SELECT			*/
	{ 0x0A, 0x00 },   /*  0x0A	AK5704_0A_PLL_REF_CLK_DIVIDER_1			*/
	{ 0x0B, 0x00 },   /*  0x0B	AK5704_0B_PLL_REF_CLK_DIVIDER_2			*/
	{ 0x0C, 0x00 },   /*  0x0C	AK5704_0C_PLL_FB_CLK_DIVIDER_1			*/
	{ 0x0D, 0x00 },   /*  0x0D	AK5704_0D_PLL_FB_CLK_DIVIDER_2			*/
	{ 0x0E, 0x00 },   /*  0x0E	AK5704_0E_AUDIO_IF_FORMAL				*/
	{ 0x0F, 0x00 },   /*  0x0F	AK5704_0F_PHASE_CORRECTION_1A			*/
	{ 0x10, 0x00 },   /*  0x10	AK5704_10_PHASE_CORRECTION_1B			*/
	{ 0x11, 0x00 },   /*  0x11	AK5704_11_PHASE_CORRECTION_2A			*/
	{ 0x12, 0x00 },   /*  0x12	AK5704_12_PHASE_CORRECTION_2B			*/
	{ 0x13, 0x00 },   /*  0x13	AK5704_13_ADC_HIGH_PASS_FILTER			*/
	{ 0x14, 0x00 },   /*  0x14	AK5704_14_DIGITAL_FILTER_SELECT			*/
	{ 0x15, 0x80 },   /*  0x15	AK5704_15_MIC_SENSITIVITY_ADJ_1A		*/
	{ 0x16, 0x80 },   /*  0x16	AK5704_16_MIC_SENSITIVITY_ADJ_1B		*/
	{ 0x17, 0x80 },   /*  0x17	AK5704_17_MIC_SENSITIVITY_ADJ_2A		*/
	{ 0x18, 0x80 },   /*  0x18	AK5704_18_MIC_SENSITIVITY_ADJ_2B		*/
	{ 0x19, 0x00 },   /*  0x19	AK5704_19_FILTER_1_SELECT				*/
	{ 0x1A, 0x00 },   /*  0x1A	AK5704_1A_FILTER_2_SELECT				*/
	{ 0x1B, 0x00 },   /*  0x1B	AK5704_1B_VAD_SETTING_1					*/
	{ 0x1C, 0x07 },   /*  0x1C	AK5704_1C_VAD_SETTING_2					*/
	{ 0x1D, 0x01 },   /*  0x1D	AK5704_1D_VAD_SETTING_3					*/
	{ 0x1E, 0x01 },   /*  0x1E	AK5704_1E_VAD_SETTING_4					*/
	{ 0x1F, 0x01 },   /*  0x1F	AK5704_1F_VAD_SETTING_5					*/
	{ 0x20, 0x72 },   /*  0x20	AK5704_20_VAD_SETTING_6					*/
	{ 0x21, 0x00 },   /*  0x21	AK5704_21_VAD_SETTING_7					*/
	{ 0x22, 0x01 },   /*  0x22	AK5704_22_VAD_SETTING_8					*/
	{ 0x23, 0x0F },   /*  0x23	AK5704_23_VAD_SETTING_9					*/
	{ 0x24, 0x00 },   /*  0x24	AK5704_24_VAD_SETTING_10				*/
	{ 0x25, 0x00 },   /*  0x25	AK5704_25_ALC_SELECT					*/
	{ 0x26, 0x40 },   /*  0x26	AK5704_26_ALC_CONTROLL_1				*/
	{ 0x27, 0x91 },   /*  0x27	AK5704_27_INPUT_DIGITAL_VOLUME_1A		*/
	{ 0x28, 0x91 },   /*  0x28	AK5704_28_INPUT_DIGITAL_VOLUME_1B		*/
	{ 0x29, 0xE1 },   /*  0x29	AK5704_29_ALC_1_REFERENCE_LEVEL			*/
	{ 0x2A, 0x80 },   /*  0x2A	AK5704_2A_ALC_1_CONTROLL		　　　	*/
	{ 0x2B, 0x91 },   /*  0x2B	AK5704_2B_INPUT_DIGITAL_VOLUME_2A		*/
	{ 0x2C, 0x91 },   /*  0x2C	AK5704_2C_INPUT_DIGITAL_VOLUME_2B		*/
	{ 0x2D, 0xE1 },   /*  0x2D	AK5704_2D_ALC_2_REFERENCE_LEVEL			*/
	{ 0x2E, 0x80 },   /*  0x2E	AK5704_2E_ALC_2_CONTROLL		　　　    */
	{ 0x2F, 0x7E },   /*  0x2F	AK5704_2F_HPF_1_COEFFICIENT_A			*/
	{ 0x30, 0xC1 },   /*  0x30	AK5704_30_HPF_1_COEFFICIENT_A			*/
	{ 0x31, 0x82 },   /*  0x31	AK5704_31_HPF_1_COEFFICIENT_B			*/
	{ 0x32, 0x7D },   /*  0x32	AK5704_32_HPF_1_COEFFICIENT_B			*/
	{ 0x33, 0x00 },   /*  0x33	AK5704_33_LPF_1_COEFFICIENT_A			*/
	{ 0x34, 0x00 },   /*  0x34	AK5704_34_LPF_1_COEFFICIENT_A			*/
	{ 0x35, 0x00 },   /*  0x35	AK5704_35_LPF_1_COEFFICIENT_B			*/
	{ 0x36, 0x00 },   /*  0x36	AK5704_36_LPF_1_COEFFICIENT_B			*/
	{ 0x37, 0x7E },   /*  0x37	AK5704_37_HPF_2_COEFFICIENT_A			*/
	{ 0x38, 0xC1 },   /*  0x38	AK5704_38_HPF_2_COEFFICIENT_A			*/
	{ 0x39, 0x82 },   /*  0x39	AK5704_39_HPF_2_COEFFICIENT_B			*/
	{ 0x3A, 0x7D },   /*  0x3A	AK5704_3A_HPF_2_COEFFICIENT_B			*/
	{ 0x3B, 0x00 },   /*  0x3B	AK5704_3B_LPF_2_COEFFICIENT_A			*/
	{ 0x3C, 0x00 },   /*  0x3C	AK5704_3C_LPF_2_COEFFICIENT_A			*/
	{ 0x3D, 0x00 },   /*  0x3D	AK5704_3D_LPF_2_COEFFICIENT_B			*/
	{ 0x3E, 0x00 },   /*  0x3E	AK5704_3E_LPF_2_COEFFICIENT_B			*/
	{ 0x3F, 0x78 },   /*  0x3F	AK5704_3F_VAHPF_COEFFICIENT_A			*/
	{ 0x40, 0xDF },   /*  0x40	AK5704_40_VAHPF_COEFFICIENT_A			*/
	{ 0x41, 0x8E },   /*  0x41	AK5704_41_VAHPF_COEFFICIENT_B			*/
	{ 0x42, 0x42 },   /*  0x42	AK5704_42_VAHPF_COEFFICIENT_B			*/
	{ 0x43, 0x00 },   /*  0x43	AK5704_43_VALPF_COEFFICIENT_A			*/
	{ 0x44, 0x00 },   /*  0x44	AK5704_44_VALPF_COEFFICIENT_A			*/
	{ 0x45, 0x00 },   /*  0x45	AK5704_45_VALPF_COEFFICIENT_B			*/
	{ 0x46, 0x00 },   /*  0x46	AK5704_46_VALPF_COEFFICIENT_B			*/
};


static int ak5704_write_lpf(struct snd_soc_codec *codec, int lpfSelect, int lpfMode);
static int ak5704_write_hpf(struct snd_soc_codec *codec, int hpfSelect, int hpfMode);

/******************* KCONTROL DECLARE *******************/
/* AK5522 Gain control:
 * from +12 to -3 dB in 1 dB steps
 * AK5704 Gain control:
 * from +30 to 0  dB in 3 dB steps
 */
//gain controll
static	DECLARE_TLV_DB_SCALE(mgain1_tlv, 0, 300, 0); //3dB/step
static	DECLARE_TLV_DB_SCALE(mgain2_tlv, 0, 300, 0);
//input volume controll
static	DECLARE_TLV_DB_SCALE(inputvol_tlv_1, -5437, 37, 0); //0.375dB/step
static	DECLARE_TLV_DB_SCALE(inputvol_tlv_2, -5437, 37, 0);
//ALC Reference Level
static	DECLARE_TLV_DB_SCALE(alc_reference_1, -5250, 37, 1); //0.375dB/step
static	DECLARE_TLV_DB_SCALE(alc_reference_2, -5250, 37, 1);


static const char *micpw_texts[] = {
	"2.8V", "2.5V", "1.8V", "AVDD"
};

static const struct soc_enum ak5704_micpw_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 0, ARRAY_SIZE(micpw_texts), micpw_texts),
};

static const char *dlc_texts[] = {
	"24-bit Linear", "16-bit Linear", "32-bit Linear"
};

static const struct soc_enum ak5704_dlc_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_0E_AUDIO_IF_FORMAL, 4, ARRAY_SIZE(dlc_texts), dlc_texts),
};

static const char *input_cycle_texts[] = {
	"656/fs", "164/fs", "1312/fs", "328/fs", "2624/fs", "128/fs", "256/fs", "512/fs"
};

static const struct soc_enum ak5704_input_cycle_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_03_POWER_MANAGEMENT_3, 5, ARRAY_SIZE(input_cycle_texts), input_cycle_texts)
};

static const char *adc_cycle_texts[] = {
	"1059/fs", "267/fs", "2115/fs", "531/fs", "4230/fs", "8/fs", "16/fs", "32/fs"
};

static const struct soc_enum ak5704_adc_cycle_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_13_ADC_HIGH_PASS_FILTER, 4, ARRAY_SIZE(adc_cycle_texts), adc_cycle_texts)
};

static const char *syncw_texts[] = {
	"4CLK", "8CLK", "2CLK"
};

static const struct soc_enum ak5704_syncw_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_08_CLOCK_MODE_SELECT, 6, ARRAY_SIZE(syncw_texts), syncw_texts)
};

static const char *dmice1_texts[] = {
	"Low", "64fs"
};

static const char *dmicp1_texts[] = {
	"High", "Low"
};

static const char *dmice2_texts[] = {
	"Low", "64fs"
};

static const char *dmicp2_texts[] = {
	"High", "Low"
};

static const struct soc_enum ak5704_dmic_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 1, ARRAY_SIZE(dmice1_texts), dmice1_texts),
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 2, ARRAY_SIZE(dmicp1_texts), dmicp1_texts),
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 5, ARRAY_SIZE(dmice2_texts), dmice1_texts),
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 6, ARRAY_SIZE(dmicp2_texts), dmicp1_texts)
};


static const char *ivol1_texts[] = {
	"Independent", "Dependent"
};

static const char *ivol2_texts[] = {
	"Independent", "Dependent"
};

static const struct soc_enum ak5704_ivol_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_2A_ALC_1_CONTROLL, 7, ARRAY_SIZE(ivol1_texts), ivol1_texts),
	SOC_ENUM_SINGLE(AK5704_2E_ALC_2_CONTROLL, 7, ARRAY_SIZE(ivol2_texts), ivol2_texts)
};

static const char *alc1_outlevel_texts[] = {
	"-4.1dBFS_-2.5dBFS", "-6.0dBFS_-4.1dBFS", "-8.5dBFS_-6.0dBFS", "-12dBFS_-8.5dBFS"
};

static const char *alc2_outlevel_texts[] = {
	"-4.1dBFS_-2.5dBFS", "-6.0dBFS_-4.1dBFS", "-8.5dBFS_-6.0dBFS", "-12dBFS_-8.5dBFS"
};

static const struct soc_enum alc_outlevel_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_2A_ALC_1_CONTROLL, 0, ARRAY_SIZE(alc1_outlevel_texts), alc1_outlevel_texts),
	SOC_ENUM_SINGLE(AK5704_2E_ALC_2_CONTROLL, 0, ARRAY_SIZE(alc2_outlevel_texts), alc2_outlevel_texts)
};


static const char *bcko_texts[] = {
	"32fs", "64fs"
};

static const char *mclk_polarity_texts[] = {
	"Disable", "Enable"
};

static const struct soc_enum ak5704_set_enum[] = {
	SOC_ENUM_SINGLE(AK5704_09_PLL_CLK_SOURCE_SELECT, 4, ARRAY_SIZE(mclk_polarity_texts), mclk_polarity_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bcko_texts), bcko_texts)
};

static int mcktab[] = {
	11289600, 12288000, 22579600, 24576000, 12000000,
	16000000, 24000000, 19200000, 9600000, 13000000,
	26000000, 13500000, 27000000
};

static const char *tdm_mode_texts[] = {
	"Stereo", "TDM128", "TDM256", "TDM512"
};

static const char *cm_mode_texts[] = {
	"256fs", "512fs", "1028fs", "128fs"
};

static const char *clock_mode_texts[] = {
	"EXT", "BCLK PLL Mode", "MKCI PLL Mode"
};

static const char *pll_master_mclk_texts[] = {
	"11.2896MHz", "12.288MHz", "22.5796MHz", "24.576MHz", "12MHz",
	"16MHz", "24MHz",  "19.2MHz", "9.6MHz", "13MHz", "26MHz",
	"13.5MHz", "27MHz"
};

static const struct soc_enum ak5704_clock_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_mode_texts), tdm_mode_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(cm_mode_texts), cm_mode_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pll_master_mclk_texts), pll_master_mclk_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(clock_mode_texts), clock_mode_texts)
};

//tdm select
static int get_tdmmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt(" [AK5704] %s ak5704->tmdMode=%d\n", __FUNCTION__, ak5704->tmdMode);
	ucontrol->value.enumerated.item[0] = ak5704->tmdMode;

	return 0;
}

static int set_tdmmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(tdm_mode_texts)) {
		ak5704->tmdMode = currMode;
		akdbgprt(" [AK5704] %s tmdMode=%d\n", __FUNCTION__, ak5704->tmdMode);
		snd_soc_update_bits(codec, AK5704_0E_AUDIO_IF_FORMAL, 0x0C, (currMode << 2));
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}

	return 0;
}

//Clock Master
static int get_cmmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt(" [AK5704] %s ak5704->cmMode=%d\n", __FUNCTION__, ak5704->cmMode);
	ucontrol->value.enumerated.item[0] = ak5704->cmMode;

	return 0;
}

static int set_cmmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(cm_mode_texts)) {
		ak5704->cmMode = currMode;
		snd_soc_update_bits(codec, AK5704_08_CLOCK_MODE_SELECT, 0x30, (currMode << 4));
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}
	return 0;
}



static int get_clockmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt(" [AK5704] %s ak5704->pllMode=%d\n", __FUNCTION__, ak5704->pllMode);

	ucontrol->value.enumerated.item[0] = ak5704->pllMode;

	return 0;
}

static int set_clockmode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(clock_mode_texts)) {
		ak5704->pllMode = currMode;
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}
	return 0;
}


static int get_mclkfreq(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt(" [AK5704] %s ak5704->mclkFreq=%d\n", __FUNCTION__, ak5704->mclkFreq);

	ucontrol->value.enumerated.item[0] = ak5704->mclkFreq;

	return 0;
}

static int set_mclkfreq(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(pll_master_mclk_texts)) {
		ak5704->mclkFreq = currMode;
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}
	return 0;
}

static int get_bickfreq(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt(" [AK5704] %s ak5704->bickFreq=%d\n", __FUNCTION__, ak5704->bickFreq);

	ucontrol->value.enumerated.item[0] = ak5704->bickFreq;

	return 0;
}

static int set_bickfreq(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(pll_master_mclk_texts)) {
		ak5704->bickFreq = currMode;
		snd_soc_update_bits(codec, AK5704_09_PLL_CLK_SOURCE_SELECT, 0x20, (currMode << 5));
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}
	return 0;
}



static const char *ak5704_min_th_texts[] = {
	"Zero", "-102dB", "-96dB", "-93dB", "-90dB",
	"-88dB", "-87dB", "-85dB", "-84dB", "-83dB",
	"-82dB", "-81dB", "-80dB", "-79dB", "-78dB",
	"-77dB", "-76dB", "-75dB", "-74dB", "-73dB",
	"-72dB", "-71dB", "-70dB", "-69dB", "-68dB",
	"-67dB", "-66dB", "-65dB", "-64dB", "-63dB",
	"-62dB", "-61dB", "-60dB", "-59dB", "-58dB",
	"-57dB", "-56dB", "-55dB", "-54dB", "-53dB",
	"-52dB", "-51dB", "-50dB", "-49dB", "-48dB",
	"-47dB", "-46dB", "-45dB", "-44dB", "-43dB",
	"-42dB", "-41dB", "-40dB",
};


static const char *ak5704_nld_th_texts[] = {
	"Zero", "-42dB", "-36dB", "-33dB", "-30dB",
	"-28dB", "-27dB", "-25dB", "-24dB", "-23dB",
	"-22dB", "-21dB", "-20dB", "-19dB", "-18dB",
	"-17dB", "-16dB", "-15dB", "-14dB", "-13dB",
	"-12dB", "-11dB", "-10dB", "-9dB", "-8dB",
	"-7dB", "-6dB", "-5dB", "-4dB", "-3dB",
	"-2dB", "-1dB", "0dB", "1dB", "2dB",
	"3dB", "4dB", "5dB", "6dB", "7dB",
	"8dB", "9dB", "10dB", "11dB", "12dB",
	"13dB", "14dB", "15dB", "16dB", "17dB",
	"18dB", "19dB", "20dB", "21dB", "22dB",
	"23dB", "24dB", "25dB", "26dB", "27dB",
	"28dB", "29dB", "30dB",
};

static unsigned int MinThVol[] = {
	0x0, 0x1, 0x2, 0x3, 0x4,
	0x5, 0x6, 0x7, 0x8, 0x9,
	0xA, 0xC, 0xD, 0xF, 0x11,
	0x13, 0x15, 0x17, 0x1A, 0x1D,
	0x21, 0x25, 0x29, 0x2F, 0x34,
	0x3B, 0x42, 0x4A, 0x53, 0x5D,
	0x68, 0x75, 0x83, 0x93, 0xA5,
	0xB9, 0xD0, 0xE9, 0x106, 0x125,
	0x149, 0x171, 0x19E, 0x1D1, 0x20A,
	0x249, 0x291, 0x2E1, 0x33B, 0x3A0,
	0x411, 0x490, 0x51F,
};

static unsigned int NldThVol[] = {
	0x0, 0x1, 0x2, 0x3, 0x4,
	0x5, 0x6, 0x7, 0x8, 0x9,
	0xA, 0xB, 0xD, 0xE, 0x10,
	0x12, 0x14, 0x17, 0x1A, 0x1D,
	0x20, 0x24, 0x28, 0x2D, 0x33,
	0x39, 0x40, 0x48, 0x51, 0x5B,
	0x66, 0x72, 0x80, 0x90, 0xA1,
	0xB5, 0xCB, 0xE4, 0xFF, 0x11F,
	0x142, 0x169, 0x195, 0x1C6, 0x1FE,
	0x23C, 0x282, 0x2D0, 0x328, 0x38A,
	0x3F9, 0x475, 0x500, 0x59C, 0x64B,
	0x710, 0x7ED, 0x8E4, 0x9FA, 0xB32,
	0xC8F, 0xE18, 0xFD0,
};

static const struct soc_enum ak5704_minth_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak5704_min_th_texts), ak5704_min_th_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak5704_nld_th_texts), ak5704_nld_th_texts)
};

static unsigned char HpfCoefTab[8][4] = {
	{ 0x7F, 0xAA, 0x80, 0xAB }, // 40
	{ 0x7F, 0x60, 0x81, 0x40 }, // 75
	{ 0x7E, 0xC1, 0x82, 0x7D }, // 150(hpf1/2 default)
	{ 0x7E, 0x59, 0x83, 0x4F }, // 200
	{ 0x7D, 0x89, 0x84, 0xEE }, // 300
	{ 0x7B, 0x29, 0x89, 0xAE }, // 600
	{ 0x78, 0xDF, 0x8E, 0x42 }, // 900(vad hpf default)
	{ 0x72, 0x74, 0x9B, 0x18 }, // 1800
	//register point check
};

 static unsigned char LpfCoefTab[8][4] = {
	{ 0x07, 0xE0, 0x8F, 0xBF }, // 1000
	{ 0x0E, 0xE4, 0x9D, 0xC8 }, // 2000
	{ 0x15, 0x3D, 0xAA, 0x79 }, // 3000
	{ 0x1B, 0x0D, 0xB6, 0x19 }, // 4000(driver default)
	{ 0x25, 0x7E, 0xCA, 0xFB }, // 6000
	{ 0x2E, 0xDA, 0xDD, 0xB4 }, // 8000
	{ 0x37, 0x93, 0xEF, 0x26 }, // 10000
	{ 0x40, 0x00, 0x00, 0x00 }, // 12000
};

static const char *ak5704_hpf1_mode_text[] = {"Off", "1st Order", "2nd Order"};
static const char *ak5704_lpf1_mode_text[] = {"Off", "1st Order", "2nd Order"};
static const char *ak5704_hpf2_mode_text[] = {"Off", "1st Order", "2nd Order"};
static const char *ak5704_lpf2_mode_text[] = {"Off", "1st Order", "2nd Order"};
static const char *ak5704_vahpf_mode_text[] = {"Off", "1st Order", "2nd Order"};
static const char *ak5704_valpf_mode_text[] = {"Off", "1st Order", "2nd Order"};

static const struct soc_enum ak5704_filter_enable[] = {
	SOC_ENUM_SINGLE(AK5704_19_FILTER_1_SELECT, 0, ARRAY_SIZE(ak5704_hpf1_mode_text), ak5704_hpf1_mode_text),
	SOC_ENUM_SINGLE(AK5704_1A_FILTER_2_SELECT, 0, ARRAY_SIZE(ak5704_hpf2_mode_text), ak5704_lpf1_mode_text),
	SOC_ENUM_SINGLE(AK5704_19_FILTER_1_SELECT, 2, ARRAY_SIZE(ak5704_lpf1_mode_text), ak5704_hpf2_mode_text),
	SOC_ENUM_SINGLE(AK5704_1A_FILTER_2_SELECT, 2, ARRAY_SIZE(ak5704_lpf2_mode_text), ak5704_lpf2_mode_text),
	SOC_ENUM_SINGLE(AK5704_1B_VAD_SETTING_1, 4, ARRAY_SIZE(ak5704_vahpf_mode_text), ak5704_vahpf_mode_text),
	SOC_ENUM_SINGLE(AK5704_1B_VAD_SETTING_1, 6, ARRAY_SIZE(ak5704_valpf_mode_text), ak5704_valpf_mode_text),
};

static const char *ak5704_hpf_fc_texts[] = {
	"fs/1200(40Hz)", "fs/640(75Hz)", "fs/320(150Hz)", "fs/240(200Hz)",
	"fs/160(300Hz)",  "fs/80(600Hz)",  "fs/53.3(900Hz)",  "fs/26.7(1800Hz)"
};

static const char *ak5704_lpf_fc_texts[] = {
	"fs/48(1kHz)", "fs/24(2kHz)", "fs/16(3kHz)", "fs/12(4kHz)",
	"fs/8(6kHz)",  "fs/6(8kHz)",  "fs/4(12kHz)",  "fs/3(16kHz)"
};

//Filter
static const struct soc_enum ak5704_filter_setup[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak5704_hpf_fc_texts), ak5704_hpf_fc_texts),	// 0
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak5704_lpf_fc_texts), ak5704_lpf_fc_texts),	// 1
};


//getter filter

static int ak5704_get_lpf1_fc(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->lpf1Fc;

	return 0;
}

static int ak5704_get_hpf1_fc(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->hpf1Fc;

	return 0;
}

static int ak5704_get_lpf2_fc(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->lpf2Fc;

	return 0;
}

static int ak5704_get_hpf2_fc(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->hpf2Fc;

	return 0;
}

static int ak5704_get_valpf_fc(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->valpfFc;

	return 0;
}

static int ak5704_get_vahpf_fc(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->vahpfFc;

	return 0;
}

//setter filter

static int ak5704_set_lpf1_fc(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_lpf_fc_texts)) {
		if (ak5704->lpf1Fc != currMode) {
			ret = ak5704_write_lpf(codec, 0, currMode);
			if (ret < 0)
				return ret;
			ak5704->lpf1Fc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_set_hpf1_fc(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_hpf_fc_texts)) {
		if (ak5704->hpf1Fc != currMode) {
			ret = ak5704_write_hpf(codec, 0, currMode);
			if (ret < 0)
				return ret;
			ak5704->hpf1Fc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_set_lpf2_fc(struct snd_kcontrol *kcontrol,
									struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_lpf_fc_texts)) {
		if (ak5704->lpf2Fc != currMode) {
			ret = ak5704_write_lpf(codec, 1, currMode);
			if (ret < 0)
				return ret;
			ak5704->lpf2Fc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_set_hpf2_fc(struct snd_kcontrol *kcontrol,
									struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_hpf_fc_texts)) {
		if (ak5704->hpf2Fc != currMode) {
			ret = ak5704_write_hpf(codec, 1, currMode);
			if (ret < 0)
				return ret;
			ak5704->hpf2Fc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_set_valpf_fc(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_lpf_fc_texts)) {
		if (ak5704->valpfFc != currMode) {
			ret = ak5704_write_lpf(codec, 2, currMode);
			if (ret < 0)
				return ret;
			ak5704->valpfFc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_set_vahpf_fc(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];
	int    ret = 0;

	if (currMode < ARRAY_SIZE(ak5704_hpf_fc_texts)) {
		if (ak5704->vahpfFc != currMode) {
			ret = ak5704_write_hpf(codec, 2, currMode);
			if (ret < 0)
				return ret;
			ak5704->vahpfFc = currMode;
		}
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
	}

	return 0;
}

static int ak5704_write_lpf(struct snd_soc_codec *codec, int lpfSelect, int lpfMode)
{
	int ret;
	int i;
	int addr;

	switch (lpfSelect) {
	case 0:
		addr = AK5704_33_LPF_1_COEFFICIENT_A;
		break;
	case 1:
		addr = AK5704_3B_LPF_2_COEFFICIENT_A;
		break;
	case 2:
		addr = AK5704_43_VALPF_COEFFICIENT_A;
		break;
	default:
		addr = AK5704_33_LPF_1_COEFFICIENT_A;
		break;
	}

	for (i = 0 ; i < 4 ; i++) {
		ret = snd_soc_write(codec, addr, LpfCoefTab[lpfMode][i]);
		if (ret < 0)
			return ret;
		addr++;
	}

	return ret;
}

static int ak5704_write_hpf(struct snd_soc_codec *codec, int hpfSelect, int hpfMode)
{
	int ret;
	int i;
	int addr;

	switch (hpfSelect) {
	case 0:
		addr = AK5704_2F_HPF_1_COEFFICIENT_A;
		break;
	case 1:
		addr = AK5704_37_HPF_2_COEFFICIENT_A;
		break;
	case 2:
		addr = AK5704_3F_VAHPF_COEFFICIENT_A;
		break;
	default:
		addr = AK5704_2F_HPF_1_COEFFICIENT_A;
		break;
	}

	for (i = 0 ; i < 4 ; i++) {
		ret = snd_soc_write(codec, addr, HpfCoefTab[hpfMode][i]);
		if (ret < 0)
			return ret;
		addr++;
	}

	return ret;
}

//getter vad
static int get_minth_mode(struct snd_kcontrol *kcontrol,
							struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->vadminth;

	return 0;
}

static int set_minth_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(ak5704_min_th_texts)) {
		ak5704->vadminth = currMode;
		snd_soc_update_bits(codec, AK5704_1D_VAD_SETTING_3, 0x01, (0x10000 & (MinThVol[currMode]) >> 16));
		snd_soc_update_bits(codec, AK5704_1E_VAD_SETTING_4, 0xFF, ((0xFF00 & MinThVol[currMode]) >> 8));
		snd_soc_update_bits(codec, AK5704_1F_VAD_SETTING_5, 0xFF, (0x00FF & MinThVol[currMode]));
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}

	return 0;
}


static int get_nldth_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak5704->vadnldth;

	return 0;
}

static int set_nldth_mode(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int    currMode = ucontrol->value.enumerated.item[0];

	akdbgprt(" [AK5704] %s currMode=%d\n", __FUNCTION__, currMode);

	if (currMode < ARRAY_SIZE(ak5704_nld_th_texts)) {
		ak5704->vadnldth = currMode;
		snd_soc_update_bits(codec, AK5704_20_VAD_SETTING_6, 0x0F, ((0xFF00 & NldThVol[currMode]) >> 8));
		snd_soc_update_bits(codec, AK5704_21_VAD_SETTING_7, 0xFF, (0x00FF & NldThVol[currMode]));
	} else {
		akdbgprt(" [AK5704] %s Invalid Value selected!\n", __FUNCTION__);
		return -EINVAL;
	}

	return 0;
}


#ifdef AK5704_DEBUG
static const char *test_reg_select[] = {
	"read AK5704 Reg 00:04",
};

static const struct soc_enum ak5704_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo;

static int get_test_reg(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
};

static int set_test_reg(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	u32 currMode = ucontrol->value.enumerated.item[0];
	int	i, value;
	int	regs, rege;

	nTestRegNo = currMode;
	regs = 0x0;
	rege = 0x46;

	for (i = regs ; i <= rege ; i++) {
		value = snd_soc_read(codec, i);
		printk("***AK5704 Addr,Reg=(%x, %x)\n", i, value);
	}

	return 0;
};
#endif

static const struct snd_kcontrol_new ak5704_snd_controls[] = { //gain controll

	SOC_SINGLE_TLV("AK5704 MIC Amp Gain 1A", AK5704_05_MIC_AMPLIFER_1_GAIN, 0, 0x0F, 0, mgain1_tlv),
	SOC_SINGLE_TLV("AK5704 MIC Amp Gain 1B", AK5704_05_MIC_AMPLIFER_1_GAIN, 4, 0x0F, 0, mgain1_tlv),
	SOC_SINGLE_TLV("AK5704 MIC Amp Gain 2A", AK5704_06_MIC_AMPLIFER_2_GAIN, 0, 0x0F, 0, mgain2_tlv),
	SOC_SINGLE_TLV("AK5704 MIC Amp Gain 2B", AK5704_06_MIC_AMPLIFER_2_GAIN, 4, 0x0F, 0, mgain2_tlv),

	SOC_SINGLE_TLV("AK5704 Input Digital Volume 1A", AK5704_27_INPUT_DIGITAL_VOLUME_1A, 0, 0xFF, 0, inputvol_tlv_1),
	SOC_SINGLE_TLV("AK5704 Input Digital Volume 1B", AK5704_28_INPUT_DIGITAL_VOLUME_1B, 0, 0xFF, 0, inputvol_tlv_1),
	SOC_SINGLE_TLV("AK5704 Input Digital Volume 2A", AK5704_2B_INPUT_DIGITAL_VOLUME_2A, 0, 0xFF, 0, inputvol_tlv_2),
	SOC_SINGLE_TLV("AK5704 Input Digital Volume 2B", AK5704_2C_INPUT_DIGITAL_VOLUME_2B, 0, 0xFF, 0, inputvol_tlv_2),

	SOC_SINGLE("AK5704 ADC Mic Sensitivity Volume 1A", AK5704_15_MIC_SENSITIVITY_ADJ_1A, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 ADC Mic Sensitivity Volume 1B", AK5704_16_MIC_SENSITIVITY_ADJ_1B, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 ADC Mic Sensitivity Volume 2A", AK5704_17_MIC_SENSITIVITY_ADJ_2A, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 ADC Mic Sensitivity Volume 2B", AK5704_18_MIC_SENSITIVITY_ADJ_2B, 0, 0xFF, 0),

	SOC_SINGLE_TLV("AK5704 ALC1 Refelence Level", AK5704_29_ALC_1_REFERENCE_LEVEL, 0, 0xFF, 0, alc_reference_1),
	SOC_SINGLE_TLV("AK5704 ALC2 Refelence Level", AK5704_2D_ALC_2_REFERENCE_LEVEL, 0, 0xFF, 0, alc_reference_2),

//mic input setting
	SOC_ENUM("AK5704 Mic Power Output Voltage", ak5704_micpw_set_enum[0]),
	SOC_SINGLE("AK5704 ADC Stereo Output", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 2, 1, 0),
	SOC_SINGLE("AK5704 Input COM Contact", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 3, 1, 0),
	SOC_SINGLE("AK5704 Full-differential Input AIN1A", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 4, 1, 0),
	SOC_SINGLE("AK5704 Full-differential Input AIN1B", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 5, 1, 0),
	SOC_SINGLE("AK5704 Full-differential Input AIN2A", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 6, 1, 0),
	SOC_SINGLE("AK5704 Full-differential Input AIN2B", AK5704_04_MIC_INPUT_MIC_POWER_SETTING, 7, 1, 0),

	//Filter Select
	SOC_SINGLE("AK5704 HPF Control for ADC1", AK5704_14_DIGITAL_FILTER_SELECT, 0, 1, 0),
	SOC_SINGLE("AK5704 HPF Control for ADC2", AK5704_14_DIGITAL_FILTER_SELECT, 1, 1, 0),

	/* Digital Mic Setting */
	SOC_ENUM("AK5704 DMCLK1 Output Clock", ak5704_dmic_set_enum[0]),
	SOC_ENUM("AK5704 DMDAT1 Data Latching Edge", ak5704_dmic_set_enum[1]),
	SOC_ENUM("AK5704 DMCLK2 Output Clock", ak5704_dmic_set_enum[2]),
	SOC_ENUM("AK5704 DMDAT2 Data Latching Edge", ak5704_dmic_set_enum[3]),

	//ADC1, 2
	SOC_SINGLE("AK5704 Phase Correction Enable 1A", AK5704_0F_PHASE_CORRECTION_1A, 7, 1, 0),
	SOC_SINGLE("AK5704 Phase Correction Enable 1B", AK5704_10_PHASE_CORRECTION_1B, 7, 1, 0),
	SOC_SINGLE("AK5704 Phase Correction Enable 2A", AK5704_11_PHASE_CORRECTION_2A, 7, 1, 0),
	SOC_SINGLE("AK5704 Phase Correction Enable 2B", AK5704_12_PHASE_CORRECTION_2B, 7, 1, 0),

	SOC_SINGLE("AK5704 Phase Correction Setting 1A", AK5704_0F_PHASE_CORRECTION_1A, 0, 0x3F, 0),
	SOC_SINGLE("AK5704 Phase Correction Setting 1B", AK5704_10_PHASE_CORRECTION_1B, 0, 0x3F, 0),
	SOC_SINGLE("AK5704 Phase Correction Setting 2A", AK5704_11_PHASE_CORRECTION_2A, 0, 0x3F, 0),
	SOC_SINGLE("AK5704 Phase Correction Setting 2B", AK5704_12_PHASE_CORRECTION_2B, 0, 0x3F, 0),

	SOC_SINGLE("AK5704 ADC1 HPF Cut-off Frequency", AK5704_13_ADC_HIGH_PASS_FILTER, 0, 3, 0),
	SOC_SINGLE("AK5704 ADC2 HPF Cut-off Frequency", AK5704_13_ADC_HIGH_PASS_FILTER, 2, 3, 0),

	//Filter Select
	SOC_SINGLE("AK5704 ADC1 Mono Mixer", AK5704_19_FILTER_1_SELECT, 4, 1, 0),
	SOC_SINGLE("AK5704 ADC2 Mono Mixer", AK5704_1A_FILTER_2_SELECT, 4, 1, 0),

	SOC_ENUM_EXT("AK5704 HPF1 Cut-off Frequency", ak5704_filter_setup[0], ak5704_get_hpf1_fc, ak5704_set_hpf1_fc),
	SOC_ENUM_EXT("AK5704 HPF2 Cut-off Frequency", ak5704_filter_setup[0], ak5704_get_hpf2_fc, ak5704_set_hpf2_fc),
	SOC_ENUM_EXT("AK5704 LPF1 Cut-off Frequency", ak5704_filter_setup[1], ak5704_get_lpf1_fc, ak5704_set_lpf1_fc),
	SOC_ENUM_EXT("AK5704 LPF2 Cut-off Frequency", ak5704_filter_setup[1], ak5704_get_lpf2_fc, ak5704_set_lpf2_fc),
	SOC_ENUM_EXT("AK5704 VAD HPF1 Cut-off Frequency", ak5704_filter_setup[0], ak5704_get_vahpf_fc, ak5704_set_vahpf_fc),
	SOC_ENUM_EXT("AK5704 VAD LPF1 Cut-off Frequency", ak5704_filter_setup[1], ak5704_get_valpf_fc, ak5704_set_valpf_fc),

	SOC_ENUM("AK5704 HPF1 Enable", ak5704_filter_enable[0]),
	SOC_ENUM("AK5704 HPF2 Enable", ak5704_filter_enable[1]),
	SOC_ENUM("AK5704 LPF1 Enable", ak5704_filter_enable[2]),
	SOC_ENUM("AK5704 LPF2 Enable", ak5704_filter_enable[3]),

	//ALC Select
	SOC_SINGLE("AK5704 ALC1 Enable", AK5704_25_ALC_SELECT, 0, 1, 0),
	SOC_SINGLE("AK5704 ALC2 Enable", AK5704_25_ALC_SELECT, 1, 1, 0),
	SOC_SINGLE("AK5704 ALC 4ch Link Enable", AK5704_25_ALC_SELECT, 2, 1, 0),
	SOC_SINGLE("AK5704 ALC1 ATT Limiter Enable", AK5704_25_ALC_SELECT, 5, 1, 0),
	SOC_SINGLE("AK5704 ALC2 ATT Limiter Enable", AK5704_25_ALC_SELECT, 6, 1, 0),
	SOC_SINGLE("AK5704 Fast Recovery Enable", AK5704_25_ALC_SELECT, 7, 1, 0),

	//ALC Control
	SOC_SINGLE("AK5704 ALC Recovery Waiting Period", AK5704_26_ALC_CONTROLL_1, 0, 3, 0),
	SOC_SINGLE("AK5704 ALC Fast Recovery Volume ATT Step", AK5704_26_ALC_CONTROLL_1, 2, 3, 0),
	SOC_SINGLE("AK5704 Input Volume Soft Transition Time", AK5704_26_ALC_CONTROLL_1, 6, 3, 0),
	SOC_SINGLE("AK5704 ALCEQ Enable", AK5704_26_ALC_CONTROLL_1, 4, 1, 0),

	//VAD setting
	SOC_SINGLE("AK5704 VAD Delay Enable", AK5704_1B_VAD_SETTING_1, 1, 1, 0),
	SOC_SINGLE("AK5704 HPF Setting for VALPF1", AK5704_1B_VAD_SETTING_1, 3, 1, 0),
	SOC_ENUM("AK5704 VAD HPF Enable", ak5704_filter_enable[4]),
	SOC_ENUM("AK5704 VAD LPF Enable", ak5704_filter_enable[5]),
	SOC_SINGLE("AK5704 VAD Noise Level Detector Peak", AK5704_1C_VAD_SETTING_2, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 VAD Rising Limit Select", AK5704_1D_VAD_SETTING_3, 4, 3, 0),
	SOC_ENUM_EXT("AK5704 Min Noise Level Setting", ak5704_minth_enum[0], get_minth_mode, set_minth_mode),
	SOC_ENUM_EXT("AK5704 Noise Level Threshold", ak5704_minth_enum[1], get_nldth_mode, set_nldth_mode),
	SOC_SINGLE("AK5704 Noise Level Detector Average Value", AK5704_20_VAD_SETTING_6, 4, 0xF, 0),
	SOC_SINGLE("AK5704 VAD On Guard Timer", AK5704_22_VAD_SETTING_8, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 VAD Off Guard Timer", AK5704_23_VAD_SETTING_9, 0, 0xFF, 0),
	SOC_SINGLE("AK5704 VAD Ach Output Selector", AK5704_24_VAD_SETTING_10, 0, 7, 0),
	SOC_SINGLE("AK5704 VAD Bch Output Selector", AK5704_24_VAD_SETTING_10, 4, 7, 0),
	SOC_SINGLE("AK5704 VAD Non-updatedTime atVON Detection", AK5704_24_VAD_SETTING_10, 7, 1, 0),

	//ALCn Control
	SOC_ENUM("AK5704 ALC1 Output Level", alc_outlevel_set_enum[0]),
	SOC_SINGLE("AK5704 ALC1 Fast Recovery Speed Setup", AK5704_2A_ALC_1_CONTROLL, 2, 3, 0),
	SOC_SINGLE("AK5704 ALC1 Recovery Gain Step", AK5704_2A_ALC_1_CONTROLL, 4, 7, 0),
	SOC_ENUM("AK5704 IVOL1 Volume Control Mode", ak5704_ivol_set_enum[0]),

	SOC_ENUM("AK5704 ALC2 Output Level", alc_outlevel_set_enum[1]),
	SOC_SINGLE("AK5704 ALC2 Fast Recovery Speed Setup", AK5704_2E_ALC_2_CONTROLL, 2, 3, 0),
	SOC_SINGLE("AK5704 ALC2 Recovery Gain Step", AK5704_2E_ALC_2_CONTROLL, 4, 7, 0),
	SOC_ENUM("AK5704 IVOL2 Volume Control Mode", ak5704_ivol_set_enum[1]),

	SOC_ENUM("AK5704 SYNCDET Error Detection Width", ak5704_syncw_set_enum[0]),
	SOC_ENUM("AK5704 AIN StartUp Time", ak5704_input_cycle_set_enum[0]),
	SOC_ENUM("AK5704 ADC Initialization Cycle", ak5704_adc_cycle_set_enum[0]),
	SOC_ENUM("AK5704 SDTO Data Length", ak5704_dlc_set_enum[0]),
	SOC_SINGLE("AK5704 SDTO2 Output Select", AK5704_00_FLOW_CONTROLL, 6, 1, 0),
	SOC_SINGLE("AK5704 Pull-down Disable bits PSW0N", AK5704_00_FLOW_CONTROLL, 0, 1, 0),
	SOC_SINGLE("AK5704 Pull-down Disable bits PSW1N", AK5704_00_FLOW_CONTROLL, 1, 1, 0),
	SOC_SINGLE("AK5704 Pull-down Disable bits PSW2N", AK5704_00_FLOW_CONTROLL, 2, 1, 0),

	SOC_ENUM("AK5704 Master Clock Output", ak5704_set_enum[0]),

	SOC_ENUM_EXT("AK5704 TDM Mode Select", ak5704_clock_enum[0], get_tdmmode, set_tdmmode),
	SOC_ENUM_EXT("AK5704 CODEC MCLK Frequency", ak5704_clock_enum[1], get_cmmode, set_cmmode),
	SOC_ENUM_EXT("AK5704 PLL MCKI Output Frequency", ak5704_clock_enum[2], get_mclkfreq, set_mclkfreq),
	SOC_ENUM_EXT("AK5704 PLL BCLK Output Frequency", ak5704_set_enum[1], get_bickfreq, set_bickfreq),
	SOC_ENUM_EXT("AK5704 Clock Mode Select", ak5704_clock_enum[3], get_clockmode, set_clockmode),
#ifdef AK5704_DEBUG
	SOC_ENUM_EXT("Reg Read", ak5704_test_enum[0], get_test_reg, set_test_reg),
#endif
};


//defined path
static const char *ak5704_lin1rin1_select_texts[] = {"AIN1BIN1", "DMD1"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_lin1rin1_mux_enum, ak5704_lin1rin1_select_texts);

static const struct snd_kcontrol_new ak5704_lin1rin1_mux_control =
	SOC_DAPM_ENUM("AIN1BIN1 Switch", ak5704_lin1rin1_mux_enum);

static const char *ak5704_lin2rin2_select_texts[] = {"AIN2BIN2", "DMD2"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_lin2rin2_mux_enum, ak5704_lin2rin2_select_texts);

static const struct snd_kcontrol_new ak5704_lin2rin2_mux_control =
	SOC_DAPM_ENUM("AIN2BIN2 Switch", ak5704_lin2rin2_mux_enum);

static const char *ak5704_adca1sw_select_texts[] = {"Off", "On"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_adca1sw_mux_enum, ak5704_adca1sw_select_texts);

static const struct snd_kcontrol_new ak5704_adca1sw_mux_control =
	SOC_DAPM_ENUM("ADCA1 Switch", ak5704_adca1sw_mux_enum);

static const char *ak5704_adca2sw_select_texts[] = {"Off", "On"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_adca2sw_mux_enum, ak5704_adca2sw_select_texts);

static const struct snd_kcontrol_new ak5704_adca2sw_mux_control =
	SOC_DAPM_ENUM("ADCA2 Switch", ak5704_adca2sw_mux_enum);

static const char *ak5704_adcb1sw_select_texts[] = {"Off", "On"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_adcb1sw_mux_enum, ak5704_adcb1sw_select_texts);

static const struct snd_kcontrol_new ak5704_adcb1sw_mux_control =
	SOC_DAPM_ENUM("ADCB1 Switch", ak5704_adcb1sw_mux_enum);

static const char *ak5704_adcb2sw_select_texts[] = {"Off", "On"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_adcb2sw_mux_enum, ak5704_adcb2sw_select_texts);

static const struct snd_kcontrol_new ak5704_adcb2sw_mux_control =
	SOC_DAPM_ENUM("ADCB2 Switch", ak5704_adcb2sw_mux_enum);

//Digital mic selector
static const char *ak5704_mic1_select_texts[] = {"AMIC", "DMIC"};
static const struct soc_enum ak5704_mic_mux1_enum =
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 0, ARRAY_SIZE(ak5704_mic1_select_texts), ak5704_mic1_select_texts);
static const struct snd_kcontrol_new ak5704_mic_mux1_control =
	SOC_DAPM_ENUM("MIC1 Select", ak5704_mic_mux1_enum);

static const char *ak5704_mic2_select_texts[] = {"AMIC", "DMIC"};
static const struct soc_enum ak5704_mic_mux2_enum =
	SOC_ENUM_SINGLE(AK5704_07_DIGITAL_MIC_SETTING, 4, ARRAY_SIZE(ak5704_mic2_select_texts), ak5704_mic2_select_texts);
static const struct snd_kcontrol_new ak5704_mic_mux2_control =
	SOC_DAPM_ENUM("MIC2 Select", ak5704_mic_mux2_enum);

// Input MUX(VAD)
static const char *ak5704_vadins_texts[] = {"AIN1A", "AIN1B", "AIN2A", "AIN2B"};

static const struct soc_enum ak5704_vad_mux_enum =
	SOC_ENUM_SINGLE(AK5704_1D_VAD_SETTING_3, 6, ARRAY_SIZE(ak5704_vadins_texts), ak5704_vadins_texts);

static const struct snd_kcontrol_new ak5704_vad_mux_control =
	SOC_DAPM_ENUM("ADC MUX", ak5704_vad_mux_enum);


// VADSEL, VAD Enable
static const char *ak5704_vadsel_texts[] = {"MIX1", "Filter1"};
static const struct soc_enum ak5704_vadsel_enum =
	SOC_ENUM_SINGLE(AK5704_1B_VAD_SETTING_1, 2, ARRAY_SIZE(ak5704_vadsel_texts), ak5704_vadsel_texts);
static const struct snd_kcontrol_new ak5704_vadsel_control =
	SOC_DAPM_ENUM("VADSEL", ak5704_vadsel_enum);

static const char *ak5704_vaden_texts[] = {"Off", "On"};
static const struct soc_enum ak5704_vaden_enum =
	SOC_ENUM_SINGLE(AK5704_03_POWER_MANAGEMENT_3, 4, ARRAY_SIZE(ak5704_vaden_texts), ak5704_vaden_texts);
static const struct snd_kcontrol_new ak5704_vaden_control =
	SOC_DAPM_ENUM("VAD Enable", ak5704_vaden_enum);


//VADOE
static const char *ak5704_vadoe_texts[] = {"Off", "On"};
static const struct soc_enum ak5704_vadoe_enum =
	SOC_ENUM_SINGLE(AK5704_1B_VAD_SETTING_1, 0, ARRAY_SIZE(ak5704_vadoe_texts), ak5704_vadoe_texts);
static const struct snd_kcontrol_new ak5704_vadoe_control =
	SOC_DAPM_ENUM("VADO Enable", ak5704_vadoe_enum);

// PFTHR 1, 2
static const char *ak5704_pfthr1_texts[] = {"Filter1", "MIX1"};
static const struct soc_enum ak5704_pfthr1_enum =
	SOC_ENUM_SINGLE(AK5704_19_FILTER_1_SELECT, 7, ARRAY_SIZE(ak5704_pfthr1_texts), ak5704_pfthr1_texts);
static const struct snd_kcontrol_new ak5704_pfthr1_control =
	SOC_DAPM_ENUM("PFTHR1 Select", ak5704_pfthr1_enum);

static const char *ak5704_pfthr2_texts[] = {"Filter2", "MIX2"};
static const struct soc_enum ak5704_pfthr2_enum =
	SOC_ENUM_SINGLE(AK5704_1A_FILTER_2_SELECT, 7, ARRAY_SIZE(ak5704_pfthr2_texts), ak5704_pfthr2_texts);
static const struct snd_kcontrol_new ak5704_pfthr2_control =
	SOC_DAPM_ENUM("PFTHR2 Select", ak5704_pfthr2_enum);


//PFSDO1, 2
static const char *ak5704_pfsdo1_texts[] = {"ADC1", "ALC1_VADO"};
static const struct soc_enum ak5704_pfsdo1_enum =
	SOC_ENUM_SINGLE(AK5704_03_POWER_MANAGEMENT_3, 2, ARRAY_SIZE(ak5704_pfsdo1_texts), ak5704_pfsdo1_texts);
static const struct snd_kcontrol_new ak5704_pfsdo1_control =
	SOC_DAPM_ENUM("PFTHR1 Select", ak5704_pfsdo1_enum);

static const char *ak5704_pfsdo2_texts[] = {"ADC2", "ALC2"};
static const struct soc_enum ak5704_pfsdo2_enum =
	SOC_ENUM_SINGLE(AK5704_03_POWER_MANAGEMENT_3, 3, ARRAY_SIZE(ak5704_pfsdo2_texts), ak5704_pfsdo2_texts);
static const struct snd_kcontrol_new ak5704_pfsdo2_control =
	SOC_DAPM_ENUM("PFTHR2 Select", ak5704_pfsdo2_enum);


//AIN1, 2
static const char *ak5704_ain1sw_select_texts[] = {"AIN1", "Mic1 Power"};
static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_ain1sw_mux_enum, ak5704_ain1sw_select_texts);
static const struct snd_kcontrol_new ak5704_ain1sw_mux_control =
	SOC_DAPM_ENUM("MIC POWER1 Switch", ak5704_ain1sw_mux_enum);

static const char *ak5704_ain2sw_select_texts[] = {"AIN2", "Mic2 Power"};
static SOC_ENUM_SINGLE_VIRT_DECL(ak5704_ain2sw_mux_enum, ak5704_ain2sw_select_texts);
static const struct snd_kcontrol_new ak5704_ain2sw_mux_control =
	SOC_DAPM_ENUM("MIC POWER2 Switch", ak5704_ain2sw_mux_enum);

//PLL
static int ak5704PowerUp(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
#ifdef KERNEL_3_18_XX
	struct snd_soc_codec *codec = w->codec;
#else
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#endif

	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		// power on programmable filters
		snd_soc_update_bits(codec, AK5704_03_POWER_MANAGEMENT_3,
					AK5704_PMPFIL1_MASK,
					AK5704_PMPFIL1_MASK);
		snd_soc_update_bits(codec, AK5704_03_POWER_MANAGEMENT_3,
					AK5704_PMPFIL2_MASK,
					AK5704_PMPFIL2_MASK);
		if (ak5704->pllMode > 0) {
			msleep(5);
			snd_soc_update_bits(codec, AK5704_01_POWER_MANAGEMENT_1, 0x40, 0x40);
			msleep(3);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, AK5704_01_POWER_MANAGEMENT_1, 0x40, 0x00);
		// turn off programmable filters
		snd_soc_update_bits(codec, AK5704_03_POWER_MANAGEMENT_3,
			AK5704_PMPFIL1_MASK | AK5704_PMPFIL2_MASK, 0x0);

		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget ak5704_dapm_widgets[] = {
	SND_SOC_DAPM_ADC("ADC1A", NULL, AK5704_02_POWER_MANAGEMENT_2, 0, 0),
	SND_SOC_DAPM_ADC("ADC1B", NULL, AK5704_02_POWER_MANAGEMENT_2, 1, 0),
	SND_SOC_DAPM_ADC("ADC2A", NULL, AK5704_02_POWER_MANAGEMENT_2, 2, 0),
	SND_SOC_DAPM_ADC("ADC2B", NULL, AK5704_02_POWER_MANAGEMENT_2, 3, 0),

	SND_SOC_DAPM_MUX("AK5704 ADC1A Enable", AK5704_01_POWER_MANAGEMENT_1, 0, 0, &ak5704_adca1sw_mux_control),
	SND_SOC_DAPM_MUX("AK5704 ADC1B Enable", AK5704_01_POWER_MANAGEMENT_1, 1, 0, &ak5704_adcb1sw_mux_control),
	SND_SOC_DAPM_MUX("AK5704 ADC2A Enable", AK5704_01_POWER_MANAGEMENT_1, 2, 0, &ak5704_adca2sw_mux_control),
	SND_SOC_DAPM_MUX("AK5704 ADC2B Enable", AK5704_01_POWER_MANAGEMENT_1, 3, 0, &ak5704_adcb2sw_mux_control),

	SND_SOC_DAPM_MUX("AK5704 ADC1A MUX", SND_SOC_NOPM, 0, 0, &ak5704_vad_mux_control),
	SND_SOC_DAPM_PGA("AK5704 ADC1B", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK5704 ADC2A", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK5704 ADC2B", SND_SOC_NOPM, 0, 0, NULL, 0),

	//Analog Input
	SND_SOC_DAPM_INPUT("AIN1"),
	SND_SOC_DAPM_INPUT("AIN2"),

	//Input Selector
	SND_SOC_DAPM_MUX("AK5704 AIN1 MUX", SND_SOC_NOPM, 0, 0, &ak5704_ain1sw_mux_control),
	SND_SOC_DAPM_MUX("AK5704 AIN2 MUX", SND_SOC_NOPM, 0, 0, &ak5704_ain2sw_mux_control),

	//Digital MIC Input
	SND_SOC_DAPM_INPUT("DMIC1A IN"),
	SND_SOC_DAPM_INPUT("DMIC1B IN"),
	SND_SOC_DAPM_INPUT("DMIC2A IN"),
	SND_SOC_DAPM_INPUT("DMIC2B IN"),

	SND_SOC_DAPM_ADC("DMIC1A", NULL, AK5704_02_POWER_MANAGEMENT_2, 4, 0),
	SND_SOC_DAPM_ADC("DMIC1B", NULL, AK5704_02_POWER_MANAGEMENT_2, 5, 0),
	SND_SOC_DAPM_ADC("DMIC2A", NULL, AK5704_02_POWER_MANAGEMENT_2, 6, 0),
	SND_SOC_DAPM_ADC("DMIC2B", NULL, AK5704_02_POWER_MANAGEMENT_2, 7, 0),

	//MIC BIAS PM
	SND_SOC_DAPM_MICBIAS("Mic1 Power", AK5704_01_POWER_MANAGEMENT_1, 4, 0),
	SND_SOC_DAPM_MICBIAS("Mic2 Power", AK5704_01_POWER_MANAGEMENT_1, 5, 0),

	//PFTHR1, 2
	SND_SOC_DAPM_MUX("AK5704 PFTHR1 MUX", SND_SOC_NOPM, 0, 0, &ak5704_pfthr1_control),
	SND_SOC_DAPM_MUX("AK5704 PFTHR2 MUX", SND_SOC_NOPM, 0, 0, &ak5704_pfthr2_control),

	//PFSDO1, 2
	SND_SOC_DAPM_MUX("AK5704 SDTO1 MUX", SND_SOC_NOPM, 0, 0, &ak5704_pfsdo1_control),
	SND_SOC_DAPM_MUX("AK5704 SDTO2 MUX", SND_SOC_NOPM, 0, 0, &ak5704_pfsdo2_control),

	//VADOE
	SND_SOC_DAPM_MUX("AK5704 VADOE MUX", SND_SOC_NOPM, 0, 0, &ak5704_vadoe_control),

	//VADSEL
	SND_SOC_DAPM_MUX("AK5704 VADSEL MUX", SND_SOC_NOPM, 0, 0, &ak5704_vadsel_control),
	SND_SOC_DAPM_MUX("AK5704 VAD Enable", SND_SOC_NOPM, 0, 0, &ak5704_vaden_control),

	//MIC Selector
	SND_SOC_DAPM_MUX("AK5704 MIC1 MUX", SND_SOC_NOPM, 0, 0, &ak5704_mic_mux1_control),
	SND_SOC_DAPM_MUX("AK5704 MIC2 MUX", SND_SOC_NOPM, 0, 0, &ak5704_mic_mux2_control),

	//ALC
	SND_SOC_DAPM_PGA("ALC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ALC2", SND_SOC_NOPM, 0, 0, NULL, 0),

	//MIX
	SND_SOC_DAPM_PGA("MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIX2", SND_SOC_NOPM, 1, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Filter1 Power", SND_SOC_NOPM, 0, 0, NULL, 0),

	//Filter
	SND_SOC_DAPM_PGA("Filter1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Filter2", SND_SOC_NOPM, 1, 0, NULL, 0),

	//PMVAD
	SND_SOC_DAPM_PGA("VADO", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VADO2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("VAD Power", AK5704_03_POWER_MANAGEMENT_3, 4, 0, NULL, 0),

	//PLL ON
	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, ak5704PowerUp, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	//output
	SND_SOC_DAPM_AIF_OUT("SDTO1", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDTO2", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("WINTN"),
};

static const struct snd_soc_dapm_route ak5704_intercon[] = {
//input(mic select)
	// MIC power select
	{"Mic1 Power", NULL, "AIN1"},
	{"AK5704 AIN1 MUX", "AIN1", "AIN1"},
	{"AK5704 AIN1 MUX", "Mic1 Power", "Mic1 Power"},

	{"Mic2 Power", NULL, "AIN2"},
	{"AK5704 AIN2 MUX", "AIN2", "AIN2"},
	{"AK5704 AIN2 MUX", "Mic2 Power", "Mic2 Power"},

	// analog mic select
	{"AK5704 ADC1A MUX", "AIN1A", "AK5704 AIN1 MUX"},
	{"AK5704 ADC1A MUX", "AIN1B", "AK5704 AIN1 MUX"},
	{"AK5704 ADC1A MUX", "AIN2A", "AK5704 AIN2 MUX"},
	{"AK5704 ADC1A MUX", "AIN2B", "AK5704 AIN2 MUX"},

	{"AK5704 ADC1B", NULL, "AK5704 AIN1 MUX"},
	{"AK5704 ADC2A", NULL, "AK5704 AIN2 MUX"},
	{"AK5704 ADC2B", NULL, "AK5704 AIN2 MUX"},

	{"AK5704 ADC1A Enable", "On", "AK5704 ADC1A MUX"},
	{"AK5704 ADC1B Enable", "On", "AK5704 ADC1B"},
	{"AK5704 ADC2A Enable", "On", "AK5704 ADC2A"},
	{"AK5704 ADC2B Enable", "On", "AK5704 ADC2B"},

	{"ADC1A", NULL, "AK5704 ADC1A Enable"},
	{"ADC1B", NULL, "AK5704 ADC1B Enable"},
	{"ADC2A", NULL, "AK5704 ADC2A Enable"},
	{"ADC2B", NULL, "AK5704 ADC2B Enable"},

	//PLL connect
	{"ADC1A", NULL, "PLL"},
	{"ADC1B", NULL, "PLL"},
	{"ADC2A", NULL, "PLL"},
	{"ADC2B", NULL, "PLL"},

	{"AK5704 MIC1 MUX", "AMIC", "ADC1A"},
	{"AK5704 MIC1 MUX", "AMIC", "ADC1B"},
	{"AK5704 MIC2 MUX", "AMIC", "ADC2A"},
	{"AK5704 MIC2 MUX", "AMIC", "ADC2B"},

	// digital mic select
	{"DMIC1A", NULL, "DMIC1A IN"},
	{"DMIC1B", NULL, "DMIC1B IN"},
	{"DMIC2A", NULL, "DMIC2A IN"},
	{"DMIC2B", NULL, "DMIC2B IN"},

	{"AK5704 MIC1 MUX", "DMIC", "DMIC1A"},
	{"AK5704 MIC1 MUX", "DMIC", "DMIC1B"},
	{"AK5704 MIC2 MUX", "DMIC", "DMIC2A"},
	{"AK5704 MIC2 MUX", "DMIC", "DMIC2B"},

// Filter Select (ADC-ALC)
  //ADC1-ALC1
	{"MIX1", NULL, "Filter1 Power"},
	//PFTHR1
	{"MIX1", NULL, "AK5704 MIC1 MUX"},
	{"Filter1", NULL, "MIX1"},
	{"AK5704 PFTHR1 MUX", "MIX1", "MIX1"},
	{"AK5704 PFTHR1 MUX", "Filter1", "Filter1"},
	{"ALC1", NULL, "AK5704 PFTHR1 MUX"},

	//VADSEL
	{"AK5704 VADSEL MUX", "MIX1", "MIX1"},
	{"AK5704 VADSEL MUX", "Filter1", "Filter1"},
	{"AK5704 VAD Enable", "On", "AK5704 VADSEL MUX"},

  //ADC2-ALC2
	{"MIX2", NULL, "Filter1 Power"},
	//PFTHR2
	{"MIX2", NULL, "AK5704 MIC2 MUX"},
	{"Filter2", NULL, "MIX2"},
	{"AK5704 PFTHR2 MUX", "MIX2", "MIX2"},
	{"AK5704 PFTHR2 MUX", "Filter2", "Filter2"},
	//ALCin
	{"ALC2", NULL, "AK5704 PFTHR2 MUX"},

// output select (ALC-OUTPUT)
  //ALC1-SDTO1
	//VADO Select
	{"VADO", NULL, "ALC1"},
	{"AK5704 VADOE MUX", "On", "VADO"},
	{"AK5704 VADOE MUX", "Off", "ALC1"},
	// PSFDO1 VADO enable (SDTO1)
	{"AK5704 SDTO1 MUX", "ADC1", "AK5704 MIC1 MUX"},
	{"AK5704 SDTO1 MUX", "ALC1_VADO", "AK5704 VADOE MUX"},

  //ALC2-SDTO2
	// PSFDO2 ADC Enable
	{"AK5704 SDTO2 MUX", "ADC2", "AK5704 MIC2 MUX"},
	// PSFDO2 Filter enable (SDTO2)
	{"AK5704 SDTO2 MUX", "ALC2", "ALC2"},

  //output
	{"SDTO1", NULL, "AK5704 SDTO1 MUX"},
	{"SDTO2", NULL, "AK5704 SDTO2 MUX"},

	{"VADO", NULL, "VAD Power"},
	{"VADO2", NULL, "VAD Power"},

	{"VADO2", NULL, "AK5704 VAD Enable"},
	{"WINTN", NULL, "VADO2"},
};

static int ak5704_set_pllblock(struct snd_soc_codec *codec, int fs)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	const int pllRefClockMin = 64000;
	const int pllRefClockMax = 640000;
	const int pldMax = 65536;

	int pllClock;
	int pllInFreq;
	int pldRefFreq;
	int rest;
	int pls, plm, pld;

	if (ak5704->pllMode == 1) {		//BICK_PLL (Slave)
		pls = 1;
		if (ak5704->bickFreq > 4) {
			pr_err("%s: bickFreq value Error!\n", __FUNCTION__);
			return -EINVAL;
		}
		if (ak5704->tmdMode == 0) {
			pllInFreq = (32 << (ak5704->bickFreq)) * fs;
		} else {
			pllInFreq = (32 << (ak5704->tmdMode + 1)) * fs;
		}
	} else if (ak5704->pllMode == 2) {		//MCKI PLL (Master)
		if ((ak5704->mclkFreq < 0) || (ak5704->mclkFreq >= ARRAY_SIZE(mcktab))) {
			pr_err("%s: mclkFreq value Error!\n", __FUNCTION__);
			return -EINVAL;
		}
		pls = 0;
		pllInFreq = mcktab[ak5704->mclkFreq];
	} else {
		return 0;
	}

	if ((fs % 4000) == 0) {
		pllClock = PLLOUT_CLOCK_48;
	} else {
		pllClock = PLLOUT_CLOCK_44;
	}

	akdbgprt("\t[ak5704] pllClock 1 = %d \n", pllClock);


	if (pllInFreq == 13000000) {
		if ((fs % 4000) == 0) {
			pld = 0x54;
			plm = 0x18D;
		} else {
			pld = 0x26;
			plm = 0xA5;
		}
	} else if ((pllInFreq == 26000000) &&	((fs % 4000) != 0)) {
		pld = 0x4C;
		plm = 0xA5;
	} else if ((pllInFreq == 13500000) &&	((fs % 4000) != 0)) {
		pld = 0xB6;
		plm = 0x2F9;
	} else if ((pllInFreq == 27000000) &&	((fs % 4000) != 0)) {
		pld = 0x182;
		plm = 0x327;
	} else {
		plm = 1;
		pld = 1;

		do {
			akdbgprt("\t[ak5704] %s pllInFreq=%d, pld=%d \n", __FUNCTION__, pllInFreq, pld);
			pldRefFreq = pllInFreq / pld;
			if (pldRefFreq < pllRefClockMin)
				break;
			rest = (pllInFreq % pld);
			if ((pldRefFreq > pllRefClockMax) || (rest != 0)) {
				pld++;
				continue;
			}
			rest = (pllClock % pldRefFreq);
			if (rest == 0) {
				plm = pllClock / pldRefFreq;
				if (ak5704->pllMode == 2) {
					break;
				} else {
					if ((plm == 0x78) || (plm == 0xA0) || (plm == 0xF0)) {
						break;
					}
				}
			}
			pld++;
			akdbgprt("\t[ak5704] %s pldRefFreq=%d, pllRefClockMin=%d \n",
										__FUNCTION__, pldRefFreq, pllRefClockMin);
		} while (pld < pldMax);
		if ((pldRefFreq < pllRefClockMin) || (pld > pldMax)) {
			pr_err("%s: Error PLL Setting 2 \n", __FUNCTION__);
			return -EINVAL;
		}
	}

	pld--;
	plm--;

	akdbgprt("\t[ak5704] %s pllInFreq=%dHz, pldRefFreq=%dHz\n", __FUNCTION__, pllInFreq, pldRefFreq);
	akdbgprt("\t[ak5704] %s PLD bit=%XH, PLM bit=%XH\n", __FUNCTION__, pld, plm);

	//PLD15-0
	snd_soc_update_bits(codec, AK5704_0A_PLL_REF_CLK_DIVIDER_1, 0xFF, ((pld & 0xFF00) >> 8));
	//snd_soc_update_bits(codec, AK5704_0B_PLL_REF_CLK_DIVIDER_2, 0xFF, ((pld & 0x00FF) >> 0));

	//PLM15-0
	snd_soc_update_bits(codec, AK5704_0C_PLL_FB_CLK_DIVIDER_1, 0xFF, ((plm & 0xFF00) >> 8));
	//snd_soc_update_bits(codec, AK5704_0D_PLL_FB_CLK_DIVIDER_2, 0xFF, ((plm & 0x00FF) >> 0));

	//hardcode
	// TODO: DEE-215246
	snd_soc_update_bits(codec, AK5704_0B_PLL_REF_CLK_DIVIDER_2, 0xFF, 0x13);
	snd_soc_update_bits(codec, AK5704_0D_PLL_FB_CLK_DIVIDER_2, 0xFF, 0x63);
	pls <<= 1;

	akdbgprt("\t[ak5704] pls 1 = %d \n", pls);

	snd_soc_update_bits(codec, AK5704_09_PLL_CLK_SOURCE_SELECT, 0x02, pls);

	return 0;
};

static int ak5704_set_mode(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	switch (ak5704->cmMode) {
	case 0: //256fs
		if (ak5704->fs > 96000) {
			pr_err("selected mode is not supported");
			return -EINVAL;
		}
		break;
	case 1: //512fs
		if (ak5704->fs == 32000 || ak5704->fs > 48000) {
			pr_err("selected mode is not supported");
			return -EINVAL;
		}
		break;
	case 2: //1024fs
		if (ak5704->fs == 16000 || ak5704->fs > 24000) {
			pr_err("selected mode is not supported");
			return -EINVAL;
		}
		break;
	case 3: //128fs
		if (ak5704->fs < 176400 || ak5704->fs > 192000) {
			pr_err("selected mode is not supported");
			return -EINVAL;
		}
		break;
	default:
		break;
	}
	return 0;
};


static int ak5704_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	u8 fsBits;
	int ret;

	akdbgprt("\t[AK5704] %s(%d)\n", __FUNCTION__, __LINE__);

	ak5704->fs = params_rate(params);

	fsBits = 0;
	switch (ak5704->fs) {
	case 8000:
		fsBits = 0;
		break;
	case 11025:
		fsBits = 1;
		break;
	case 12000:
		fsBits = 2;
		break;
	case 16000:
		fsBits = 4;
		break;
	case 22050:
		fsBits = 5;
		break;
	case 24000:
		fsBits = 6;
		break;
	case 32000:
		fsBits = 8;
		break;
	case 44100:
		fsBits = 9;
		break;
	case 48000:
		fsBits = 10;
		break;
	case 88200:
		fsBits = 12;
		break;
	case 96000:
		fsBits = 13;
		break;
	case 176400:
		fsBits = 14;
		break;
	case 192000:
		fsBits = 15;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AK5704_08_CLOCK_MODE_SELECT, 0x0F, fsBits);
	ak5704_set_pllblock(codec, ak5704->fs);

	ret = ak5704_set_mode(codec);

	return ret;
};

static int ak5704_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	return 0;
};

static int ak5704_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	u8 format_dif;

	akdbgprt("\t[AK5704] %s(%d)\n", __FUNCTION__, __LINE__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS: /* Slave Mode */
		ak5704->masterMode = 0;
		break;
	case SND_SOC_DAIFMT_CBM_CFM: /* Master Mode */
		ak5704->masterMode = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	/* set master / slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format_dif = AK5704_DIF_I2S_MODE;
		akdbgprt("\t[AK5704] AK5704_DIF_I2S_MODE SETUP \n");
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format_dif = AK5704_DIF_MSB_MODE;
		akdbgprt("\t[AK5704] AK5704_DIF_MSB_MODE SETUP \n");
		break;
	case SND_SOC_DAIFMT_DSP_B: /* mode 9 */
		format_dif = (AK5704_DIF & AK5704_DIF_MSB_MODE) |
					(AK5704_TDM & AK5704_TDM_TDM128);
		akdbgprt("\t[AK5704] AK5704_DSP_B SETUP \n");
		break;
	default:
		return -EINVAL;
	}

	if (ak5704->masterMode == 1) {
		snd_soc_update_bits(codec, AK5704_00_FLOW_CONTROLL, AK5704_MSN, AK5704_MSN_ON);  //MSB = 1;
	} else {
		snd_soc_update_bits(codec, AK5704_00_FLOW_CONTROLL, AK5704_MSN, AK5704_MSN_OFF); //MSB = 0;
	}

	snd_soc_update_bits(codec, AK5704_0E_AUDIO_IF_FORMAL, AK5704_TDM | AK5704_DIF, format_dif);

	return 0;
};

static bool ak5704_volatile(struct device *dev, unsigned int reg)
{

#ifdef AK5704_DEBUG
	return true;
#else
	return false;
#endif

};

static bool ak5704_readable(struct device *dev, unsigned int reg)
{
	if (reg > AK5704_MAX_REGISTER) {
		return false;
	}
	return true;
};

static bool ak5704_writeable(struct device *dev, unsigned int reg)
{
	if (reg > AK5704_MAX_REGISTER) {
		return false;
	}
	return true;
};

static int ak5704_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
#ifndef KERNEL_3_18_XX
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#endif

	akdbgprt("\t[AK5704] %s bios level=%d\n", __FUNCTION__, (int)level);

	switch (level) {
	case SND_SOC_BIAS_ON:     //0
	case SND_SOC_BIAS_PREPARE://1
	case SND_SOC_BIAS_STANDBY://2
#ifdef AK5704_VCOM_OFF //PMVCOM = 1
		snd_soc_update_bits(codec, AK5704_01_POWER_MANAGEMENT_1, 0x80, 0x80);
#endif
		break;
	case SND_SOC_BIAS_OFF:
#ifdef AK5704_VCOM_OFF //PMVCOM = 0
		snd_soc_update_bits(codec, AK5704_01_POWER_MANAGEMENT_1, 0x80, 0);
#endif
		break;
	}


#ifdef KERNEL_3_18_XX
	codec->dapm.bias_level = level;
#else
	dapm->bias_level = level;
#endif

	return 0;
};

#define AK5704_RATES		SNDRV_PCM_RATE_8000_192000
#define AK5704_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)


static struct snd_soc_dai_ops ak5704_dai_ops = {
	.hw_params	= ak5704_hw_params,
	.set_sysclk	= ak5704_set_dai_sysclk,
	.set_fmt	= ak5704_set_dai_fmt,
};

struct snd_soc_dai_driver ak5704_dai[] = {
	{
		.name = "ak5704-aif",
		.capture = {
			   .stream_name = "Capture",
			   .channels_min = 1,
			   .channels_max = 4, //5704 - 4ch
			   .rates = AK5704_RATES,
			   .formats = AK5704_FORMATS,
		},
		.ops = &ak5704_dai_ops,
	},
};

static int ak5704_init_reg(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK5704] %s(%d)\n", __FUNCTION__, __LINE__);

	ak5704->fs = 48000;
	ak5704->tmdMode = 0;
	ak5704->pllMode = 0;
	ak5704->masterMode = 0;
	ak5704->mclkFreq = 0;
	ak5704->bickFreq = 0;
	ak5704->lpf1Fc = 0;
	ak5704->hpf1Fc = 2;
	ak5704->lpf2Fc = 0;
	ak5704->hpf2Fc = 2;
	ak5704->vahpfFc = 6;
	ak5704->valpfFc = 0;
	ak5704->cmMode	= 0;
	ak5704->vadminth = 0;
	ak5704->vadnldth = 0;

//ALC setting
	snd_soc_update_bits(codec, AK5704_2A_ALC_1_CONTROLL, 0xFF, 0x80);
	snd_soc_update_bits(codec, AK5704_2E_ALC_2_CONTROLL, 0xFF, 0x80);
	snd_soc_update_bits(codec, AK5704_26_ALC_CONTROLL_1, 0xDF, 0x0);
//Highpass
	snd_soc_update_bits(codec, AK5704_13_ADC_HIGH_PASS_FILTER, 0x7F, 0x71);

//VAHPF coeff
	snd_soc_update_bits(codec, AK5704_41_VAHPF_COEFFICIENT_B, 0xFF, 0xDF);
	snd_soc_update_bits(codec, AK5704_42_VAHPF_COEFFICIENT_B, 0xFF, 0x8E);

#ifdef AK5704_AVDDL_1P8V  //AVDD = 1.8V
	snd_soc_update_bits(codec, AK5704_00_FLOW_CONTROLL, 0x10, 0x10);
#endif

#ifndef AK5704_VCOM_OFF //VCOM_OFF
	snd_soc_update_bits(codec, AK5704_01_POWER_MANAGEMENT_1, 0x80, 0x80);
#endif

	snd_soc_update_bits(codec, AK5704_04_MIC_INPUT_MIC_POWER_SETTING,
				AK5704_DIFF_INPUT_ALL_MASK, AK5704_DIFF_INPUT_ALL_MASK);

	return 0;
};

static int ak5704_parse_dt(struct ak5704_priv *ak5704)
{
	struct device *dev;
	struct device_node *np;
	int ret;

	dev = &(ak5704->i2c->dev);
	np = dev->of_node;

	if (!np) {
		return -1;
	}

	akdbgprt("AK5704 regulator read from device tree\n");

	ak5704->reset_adc_regulator = devm_regulator_get(dev,
						"reset-adc-regulator");
	if (IS_ERR(ak5704->reset_adc_regulator)) {
		pr_err("%s: AK5704 failed to get adc regulator\n", __FUNCTION__);
		ak5704->reset_adc_regulator = NULL;
	} else {
		ret = regulator_enable(ak5704->reset_adc_regulator);
		if (ret) {
			pr_err("%s: AK5704 failed to regulator_enable(%d)\n", __FUNCTION__, ret);
			return ret;
		}
	}

	return 0;
};

static int ak5704_probe(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	akdbgprt("\t[AK5704] %s(%d)\n", __FUNCTION__, __LINE__);

	ret = ak5704_parse_dt(ak5704);
	if (ret < 0) {
		ak5704->reset_adc_regulator = NULL;
	}

	ret = ak5704_init_reg(codec);

	return ret;
};

static int ak5704_remove(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ak5704_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (ak5704->reset_adc_regulator) {
		ret = regulator_disable(ak5704->reset_adc_regulator);
		if (ret) {
			pr_err("%s: AK5704 failed to disable adc regulator(%d)\n", __FUNCTION__, ret);
		}
	}

	return 0;
};

static int ak5704_suspend(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	ak5704_set_bias_level(codec, SND_SOC_BIAS_OFF);

	regcache_cache_only(ak5704->regmap, true);
	regcache_mark_dirty(ak5704->regmap);

	if (ak5704->reset_adc_regulator) {
		ret = regulator_disable(ak5704->reset_adc_regulator);
		if (ret) {
			pr_err("%s: AK5704 failed to disable adc regulator(%d)\n", __FUNCTION__, ret);
		}
	}

	return 0;
};

static int ak5704_resume(struct snd_soc_codec *codec)
{
	struct ak5704_priv *ak5704 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (ak5704->reset_adc_regulator) {
		ret = regulator_enable(ak5704->reset_adc_regulator);
		if (ret) {
			pr_err("%s: AK5704 failed to enable adc regulator(%d)\n", __FUNCTION__, ret);
		}
	}

	regcache_cache_only(ak5704->regmap, false);
	regcache_sync(ak5704->regmap);

	return 0;
};

struct snd_soc_codec_driver soc_codec_dev_ak5704 = {
	.probe = ak5704_probe,
	.remove = ak5704_remove,
	.suspend = ak5704_suspend,
	.resume = ak5704_resume,

	.idle_bias_off = true,
	.set_bias_level = ak5704_set_bias_level,

#ifdef KERNEL_4_9_XX
	.component_driver = {
#endif
	.controls = ak5704_snd_controls,
	.num_controls = ARRAY_SIZE(ak5704_snd_controls),
	.dapm_widgets = ak5704_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak5704_dapm_widgets),
	.dapm_routes = ak5704_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak5704_intercon),
#ifdef KERNEL_4_9_XX
	}
#endif
};

static const struct regmap_config ak5704_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AK5704_MAX_REGISTER,
	.volatile_reg = ak5704_volatile,
	.writeable_reg = ak5704_writeable,
	.readable_reg = ak5704_readable,

	.reg_defaults = ak5704_reg,
	.num_reg_defaults = ARRAY_SIZE(ak5704_reg),
	.cache_type = REGCACHE_RBTREE,
};

static int ak5704_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct ak5704_priv *ak5704;
	int ret = 0;

	akdbgprt("\t[AK5704] %s(%d)\n", __FUNCTION__, __LINE__);

	ak5704 = devm_kzalloc(&i2c->dev, sizeof(struct ak5704_priv), GFP_KERNEL);
	if (ak5704 == NULL) {
		return -ENOMEM;
	}

	ak5704->regmap = devm_regmap_init_i2c(i2c, &ak5704_regmap);
	if (IS_ERR(ak5704->regmap)) {
		return PTR_ERR(ak5704->regmap);
	}

	i2c_set_clientdata(i2c, ak5704);
	ak5704->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak5704, &ak5704_dai[0], ARRAY_SIZE(ak5704_dai));

	if (ret < 0) {
		devm_kfree(&i2c->dev, ak5704);
		akdbgprt("\t[AK5704 Error!] %s(%d)\n", __FUNCTION__, __LINE__);
	}

	return ret;
};

static int ak5704_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
};

static struct of_device_id ak5704_i2c_dt_ids[] = {
	{ .compatible = "akm,ak5704"},
	{ }
};

static const struct i2c_device_id ak5704_i2c_id[] = {
	{ "ak5704", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ak5704_i2c_id);

static struct i2c_driver ak5704_i2c_driver = {
	.driver = {
		.name = "ak5704",
#ifdef KERNEL_4_9_XX
		.owner = THIS_MODULE,
#endif
		.of_match_table = of_match_ptr(ak5704_i2c_dt_ids),
	},
	.probe = ak5704_i2c_probe,
	.remove = ak5704_i2c_remove,
	.id_table = ak5704_i2c_id,
};

static int __init ak5704_modinit(void)
{
	akdbgprt("\t[ak5704] %s(%d)\n", __FUNCTION__, __LINE__);

	return i2c_add_driver(&ak5704_i2c_driver);
};

module_init(ak5704_modinit);

static void __exit ak5704_exit(void)
{
	i2c_del_driver(&ak5704_i2c_driver);
};

module_exit(ak5704_exit);

MODULE_DESCRIPTION("ASoC ak5704 codec driver");
MODULE_LICENSE("GPL v2");
