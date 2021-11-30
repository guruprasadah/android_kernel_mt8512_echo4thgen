/*
 * Driver for the TAS5805M Audio Amplifier
 *
 * Original Author: Andy Liu <andy-liu@ti.com>
 *
 * Updated by:  C. Omer Rafique <rafiquec@amazon.com>
 *
 * Copyright (C) 2019-2020 Amazon Lab126 Inc - http://www.lab126.com/
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
#include "pinctrl-mt8518-ipi.h"
#endif

#include "tas5805m.h"
#include "tas5805m_mono.h"
#include "tas5805m_woofer.h"
#include "tas5805m_tweeter.h"

#define TAS5805M_DRV_NAME    "tas5805m"

#define TAS5805M_RATES	     (SNDRV_PCM_RATE_48000)
#define TAS5805M_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_01      (0x01)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_24      (0x24)
#define TAS5805M_REG_25      (0x25)
#define TAS5805M_REG_26      (0x26)
#define TAS5805M_REG_27      (0x27)
#define TAS5805M_REG_28      (0x28)
#define TAS5805M_REG_29      (0x29)
#define TAS5805M_REG_2A      (0x2a)
#define TAS5805M_REG_2B      (0x2b)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_REG_54      (0x54)
#define TAS5805M_REG_70      (0x70)
#define TAS5805M_REG_71      (0x71)
#define TAS5805M_REG_72      (0x72)
#define TAS5805M_REG_73      (0x73)
#define TAS5805M_REG_78      (0x78)
#define TAS5805M_REG_7F      (0x7f)

#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_2A     (0x2a)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)
#define TAS5805M_MAX_REG     TAS5805M_REG_7F

#define TAS5805M_VOLUME_MAX  (158)
#define TAS5805M_VOLUME_MIN  (0)

#define TAS5805M_HIZ_STATE   (0x02)
#define TAS5805M_STATE_MASK  (0x03)
#define TAS5805M_CLEAR_REG   (0x11)
#define TAS5805M_DEEPSLEEP_STATE (0x00)
#define TAS5805M_PLAY_STATE  (0x03)
#define TAS5805M_MUTE_MASK  (0x08)
#define TAS5805M_MUTE       (0x08)

#define TAS5805M_CLEAR_FAULT        (0x80)
#define TAS5805M_CH_FAULT_MASK      (0x0f)
#define TAS5805M_GLOBAL_FAULT_MASK  (0xc7)
#define TAS5805M_OT_SHDN_MASK       (0x01)
#define TAS5805M_OT_WARN_MASK       (0x02)

#define TAS5805M_DELAY      (254)

/* DEE-165952 - Higher delay than datasheet due to HW */
#define PDN_LOW_DELAY_USEC  17000
#define PDN_HIGH_DELAY_USEC  12000

/* Fault status registers count 0x70, 0x71, 0x72, 0x73 */
#define NUM_FAULT_REGS  4
#define FAULT_CLEAR_DELAY_USEC    5000

enum speaker_config {
	STEREO = 0,
	MONO,
	WOOFER,
	TWEETER,
	MONO_MINI,
	TWEETER_MINI,
};

enum {
	ALL_FAULTS = 0,
	OVERCURRENT_FAULT = 1,
	OTHER_FAULTS = 2
};

static const char * const fault_types_text[] = {
	"All", "Overcurrent", "Other"
};

const uint32_t tas5805m_volume[] = {
	0x0000001B,    //0, -110dB
	0x0000001E,    //1, -109dB
	0x00000021,    //2, -108dB
	0x00000025,    //3, -107dB
	0x0000002A,    //4, -106dB
	0x0000002F,    //5, -105dB
	0x00000035,    //6, -104dB
	0x0000003B,    //7, -103dB
	0x00000043,    //8, -102dB
	0x0000004B,    //9, -101dB
	0x00000054,    //10, -100dB
	0x0000005E,    //11, -99dB
	0x0000006A,    //12, -98dB
	0x00000076,    //13, -97dB
	0x00000085,    //14, -96dB
	0x00000095,    //15, -95dB
	0x000000A7,    //16, -94dB
	0x000000BC,    //17, -93dB
	0x000000D3,    //18, -92dB
	0x000000EC,    //19, -91dB
	0x00000109,    //20, -90dB
	0x0000012A,    //21, -89dB
	0x0000014E,    //22, -88dB
	0x00000177,    //23, -87dB
	0x000001A4,    //24, -86dB
	0x000001D8,    //25, -85dB
	0x00000211,    //26, -84dB
	0x00000252,    //27, -83dB
	0x0000029A,    //28, -82dB
	0x000002EC,    //29, -81dB
	0x00000347,    //30, -80dB
	0x000003AD,    //31, -79dB
	0x00000420,    //32, -78dB
	0x000004A1,    //33, -77dB
	0x00000532,    //34, -76dB
	0x000005D4,    //35, -75dB
	0x0000068A,    //36, -74dB
	0x00000756,    //37, -73dB
	0x0000083B,    //38, -72dB
	0x0000093C,    //39, -71dB
	0x00000A5D,    //40, -70dB
	0x00000BA0,    //41, -69dB
	0x00000D0C,    //42, -68dB
	0x00000EA3,    //43, -67dB
	0x0000106C,    //44, -66dB
	0x0000126D,    //45, -65dB
	0x000014AD,    //46, -64dB
	0x00001733,    //47, -63dB
	0x00001A07,    //48, -62dB
	0x00001D34,    //49, -61dB
	0x000020C5,    //50, -60dB
	0x000024C4,    //51, -59dB
	0x00002941,    //52, -58dB
	0x00002E49,    //53, -57dB
	0x000033EF,    //54, -56dB
	0x00003A45,    //55, -55dB
	0x00004161,    //56, -54dB
	0x0000495C,    //57, -53dB
	0x0000524F,    //58, -52dB
	0x00005C5A,    //59, -51dB
	0x0000679F,    //60, -50dB
	0x00007444,    //61, -49dB
	0x00008274,    //62, -48dB
	0x0000925F,    //63, -47dB
	0x0000A43B,    //64, -46dB
	0x0000B845,    //65, -45dB
	0x0000CEC1,    //66, -44dB
	0x0000E7FB,    //67, -43dB
	0x00010449,    //68, -42dB
	0x0001240C,    //69, -41dB
	0x000147AE,    //70, -40dB
	0x00016FAA,    //71, -39dB
	0x00019C86,    //72, -38dB
	0x0001CEDC,    //73, -37dB
	0x00020756,    //74, -36dB
	0x000246B5,    //75, -35dB
	0x00028DCF,    //76, -34dB
	0x0002DD96,    //77, -33dB
	0x00033718,    //78, -32dB
	0x00039B87,    //79, -31dB
	0x00040C37,    //80, -30dB
	0x00048AA7,    //81, -29dB
	0x00051884,    //82, -28dB
	0x0005B7B1,    //83, -27dB
	0x00066A4A,    //84, -26dB
	0x000732AE,    //85, -25dB
	0x00081385,    //86, -24dB
	0x00090FCC,    //87, -23dB
	0x000A2ADB,    //88, -22dB
	0x000B6873,    //89, -21dB
	0x000CCCCD,    //90, -20dB
	0x000E5CA1,    //91, -19dB
	0x00101D3F,    //92, -18dB
	0x0012149A,    //93, -17dB
	0x00144961,    //94, -16dB
	0x0016C311,    //95, -15dB
	0x00198A13,    //96, -14dB
	0x001CA7D7,    //97, -13dB
	0x002026F3,    //98, -12dB
	0x00241347,    //99, -11dB
	0x00287A27,    //100, -10dB
	0x002D6A86,    //101, -9dB
	0x0032F52D,    //102, -8dB
	0x00392CEE,    //103, -7dB
	0x004026E7,    //104, -6dB
	0x0047FACD,    //105, -5dB
	0x0050C336,    //106, -4dB
	0x005A9DF8,    //107, -3dB
	0x0065AC8C,    //108, -2dB
	0x00721483,    //109, -1dB
	0x00800000,    //110, 0dB
	0x008F9E4D,    //111, 1dB
	0x00A12478,    //112, 2dB
	0x00B4CE08,    //113, 3dB
	0x00CADDC8,    //114, 4dB
	0x00E39EA9,    //115, 5dB
	0x00FF64C1,    //116, 6dB
	0x011E8E6A,    //117, 7dB
	0x0141857F,    //118, 8dB
	0x0168C0C6,    //119, 9dB
	0x0194C584,    //120, 10dB
	0x01C62940,    //121, 11dB
	0x01FD93C2,    //122, 12dB
	0x023BC148,    //123, 13dB
	0x02818508,    //124, 14dB
	0x02CFCC01,    //125, 15dB
	0x0327A01A,    //126, 16dB
	0x038A2BAD,    //127, 17dB
	0x03F8BD7A,    //128, 18dB
	0x0474CD1B,    //129, 19dB
	0x05000000,    //130, 20dB
	0x059C2F02,    //131, 21dB
	0x064B6CAE,    //132, 22dB
	0x07100C4D,    //133, 23dB
	0x07ECA9CD,    //134, 24dB
	0x08E43299,    //135, 25dB
	0x09F9EF8E,    //136, 26dB
	0x0B319025,    //137, 27dB
	0x0C8F36F2,    //138, 28dB
	0x0E1787B8,    //139, 29dB
	0x0FCFB725,    //140, 30dB
	0x11BD9C84,    //141, 31dB
	0x13E7C594,    //142, 32dB
	0x16558CCB,    //143, 33dB
	0x190F3254,    //144, 34dB
	0x1C1DF80E,    //145, 35dB
	0x1F8C4107,    //146, 36dB
	0x2365B4BF,    //147, 37dB
	0x27B766C2,    //148, 38dB
	0x2C900313,    //149, 39dB
	0x32000000,    //150, 40dB
	0x3819D612,    //151, 41dB
	0x3EF23ECA,    //152, 42dB
	0x46A07B07,    //153, 43dB
	0x4F3EA203,    //154, 44dB
	0x58E9F9F9,    //155, 45dB
	0x63C35B8E,    //156, 46dB
	0x6FEFA16D,    //157, 47dB
	0x7D982575,    //158, 48dB
};

struct tas5805m_priv {
	struct regmap *regmap;
	bool init_done;
	struct mutex lock;
	struct device *dev;
	int vol;
	int dac_num;
	int config;
	bool swap_lr;
	bool eq_en;
	bool eq_state;
	bool force_unmute;
	bool pcm_active;
	int again;
	unsigned int ch_fault;
	unsigned int global_fault;
	unsigned int ot_shdn;
	unsigned int ot_warn;
	struct snd_soc_codec *codec;
};

/* To update secondary codec */
struct tas5805m_priv *tas5805m_b;

static unsigned int gpio_dsp_ctrl_pin;
static unsigned int reset_amp_gpio;

static bool tas5805m_reg_is_volatile(struct device *dev, unsigned int reg)
{
	return reg >= TAS5805M_REG_70 && reg < TAS5805M_MAX_REG;
}

const struct regmap_config tas5805m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = tas5805m_reg_is_volatile
};

static int tas5805m_apply_config(struct snd_soc_codec *codec);

static int tas5805m_vol_info(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ |
					SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count  = 1;

	uinfo->value.integer.min  = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max  = TAS5805M_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_vol_locked_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->vol;
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < TAS5805M_VOLUME_MIN)
		index = TAS5805M_VOLUME_MIN;

	if (index > TAS5805M_VOLUME_MAX)
		index = TAS5805M_VOLUME_MAX;

	return index;
}

static void tas5805m_set_volume(struct snd_soc_codec *codec, int vol)
{
	unsigned int index;
	uint32_t volume_hex;
	uint8_t byte4;
	uint8_t byte3;
	uint8_t byte2;
	uint8_t byte1;

	pr_info("%s: vol %d\n", __func__, vol);

	index = get_volume_index(vol);
	volume_hex = tas5805m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8)	& 0xFF);
	byte1 = ((volume_hex >> 0)	& 0xFF);

	//w 58 00 00
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 58 7f 8c
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_8C);
	//w 58 00 2a
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_2A);
	//w 58 24 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_24, byte4);
	snd_soc_write(codec, TAS5805M_REG_25, byte3);
	snd_soc_write(codec, TAS5805M_REG_26, byte2);
	snd_soc_write(codec, TAS5805M_REG_27, byte1);
	//w 58 28 xx xx xx xx
	snd_soc_write(codec, TAS5805M_REG_28, byte4);
	snd_soc_write(codec, TAS5805M_REG_29, byte3);
	snd_soc_write(codec, TAS5805M_REG_2A, byte2);
	snd_soc_write(codec, TAS5805M_REG_2B, byte1);
}

static int tas5805m_vol_locked_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);

	tas5805m->vol = ucontrol->value.integer.value[0];
	tas5805m_set_volume(codec, tas5805m->vol);

	mutex_unlock(&tas5805m->lock);

	return 0;
}

static int tas5805m_eq_locked_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->eq_en;
	mutex_unlock(&tas5805m->lock);

	return 0;

}

static int tas5805m_eq_locked_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m->lock);
	tas5805m->eq_en = ucontrol->value.integer.value[0];
	/* Update second codec if exist */
	if (tas5805m_b)
		tas5805m_b->eq_en = ucontrol->value.integer.value[0];
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static int tas5805m_set_register(struct snd_soc_codec *codec,
	unsigned int book, unsigned int page, unsigned int reg,
	unsigned int value)
{
	int ret;

	/* Move to Page 0 */
	ret = snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	/* Set Book Number */
	ret |= snd_soc_write(codec, TAS5805M_REG_7F, book);
	/* Set Page Number */
	ret |= snd_soc_write(codec, TAS5805M_REG_00, page);
	/* Set Register Value */
	ret |= snd_soc_write(codec, reg, value);

	return ret;
}

static unsigned int tas5805m_get_register(struct snd_soc_codec *codec,
	unsigned int book, unsigned int page, unsigned int reg)
{
	int ret;
	unsigned int value;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	/* Move to Page 0 */
	ret = snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	/* Set Book Number */
	ret |= snd_soc_write(codec, TAS5805M_REG_7F, book);
	/* Set Page Number */
	ret |= snd_soc_write(codec, TAS5805M_REG_00, page);

	if (ret)
		pr_err("%s failed to read. codec %d\n",
			__func__, tas5805m_data->dac_num);

	/* Set Register Value */
	value = snd_soc_read(codec, reg);

	return value;
}


static void tas5805m_apply_mute(struct snd_soc_codec *codec, int mute)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	u8 reg03_value = 0;
	u8 reg35_value = 0;

	pr_info("%s: mute %d force_unmute %d\n", __func__,
		mute, tas5805m_data->force_unmute);

	if (mute) {
		//mute both left & right channels
		reg03_value = 0x08;
		reg35_value = 0x00;
	} else {
		//unmute
		reg03_value = 0x00;
		reg35_value = tas5805m_data->swap_lr ? 0x22 : 0x11;
	}

	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_write(codec, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_write(codec, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_update_bits(codec, TAS5805M_REG_03, TAS5805M_MUTE_MASK,
	    reg03_value);
	snd_soc_write(codec, TAS5805M_REG_35, reg35_value);

	if (!mute)
		tas5805m_set_volume(codec, tas5805m_data->vol);
}

static int tas5805m_check_faults(struct snd_soc_codec *codec)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	/* L/R channels: DC Fault MASK = 0x0c, Over Current Mask = 0x03 */
	tas5805m_data->ch_fault = tas5805m_get_register(codec, TAS5805M_BOOK_00,
	TAS5805M_PAGE_00, TAS5805M_REG_70)  & TAS5805M_CH_FAULT_MASK;

	/* Read directly since we are already at book 0, page 0 */
	/* Masks: OTP_CRC_ERROR=0x80, BQ_WR_ERROR=0x40 */
	/*        CLK_FAULT=0x04, PVDD=0x03 */
	tas5805m_data->global_fault = snd_soc_read(codec, TAS5805M_REG_71) &
			TAS5805M_GLOBAL_FAULT_MASK;

	/* Overtemperature Shutdown Mask 0x01*/
	tas5805m_data->ot_shdn = snd_soc_read(codec, TAS5805M_REG_72) &
			TAS5805M_OT_SHDN_MASK;

	/* Overtemperature Warning Mask 0x02*/
	tas5805m_data->ot_warn = snd_soc_read(codec, TAS5805M_REG_73) &
			TAS5805M_OT_WARN_MASK;

	if (tas5805m_data->ch_fault | tas5805m_data->global_fault |
		tas5805m_data->ot_shdn | tas5805m_data->ot_warn) {
		dev_err(codec->dev,
			"%s: ch_fault 0x%X global_fault 0x%X ot_shutdown 0x%X ot_warn 0x%X\n",
			__func__, tas5805m_data->ch_fault,
			tas5805m_data->global_fault,
			tas5805m_data->ot_shdn,
			tas5805m_data->ot_warn);
		return -1;
	}

	dev_info(codec->dev, "%s: no faults detected\n", __func__);

	return 0;
}

static int tas5805m_clear_faults(struct snd_soc_codec *codec)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int status = 0;

	status = tas5805m_set_register(codec, TAS5805M_BOOK_00,
			TAS5805M_PAGE_00, TAS5805M_REG_78,
			TAS5805M_CLEAR_FAULT);

	tas5805m_data->ch_fault = 0;
	tas5805m_data->global_fault = 0;
	tas5805m_data->ot_shdn = 0;
	tas5805m_data->ot_warn = 0;

	dev_info(codec->dev, "%s: status %d\n", __func__, status);

	return 0;
}

static int tas5805m_device_state(struct snd_soc_codec *codec, int sleep)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int status = 0;

	pr_info("%s: sleep %d force_unmute %d\n", __func__,
		sleep, tas5805m_data->force_unmute);

	if (tas5805m_data->force_unmute && sleep)
		return 0;

	if (sleep) {
		status = tas5805m_set_register(codec, TAS5805M_BOOK_00,
		TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_HIZ_STATE);

		status |= tas5805m_set_register(codec, TAS5805M_BOOK_00,
		TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_DEEPSLEEP_STATE);

	} else {
		status = tas5805m_set_register(codec, TAS5805M_BOOK_00,
		TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_HIZ_STATE);

		status |= tas5805m_set_register(codec, TAS5805M_BOOK_00,
		TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_PLAY_STATE);

		status |= tas5805m_clear_faults(codec);
	}

	if (status)
		pr_err("%s failed to set device state %d sleep %d\n",
			__func__, status, sleep);

	return status;
}

static void tas5805m_pwr_down(int enable)
{
	if (!gpio_is_valid(reset_amp_gpio)) {
#ifdef CONFIG_MTK_HIFI4DSP_SUPPORT
		gpio_send_ipi_msg(gpio_dsp_ctrl_pin, SET_MODE_IPI,
			GPIO_MODE_00);
		gpio_send_ipi_msg(gpio_dsp_ctrl_pin, SET_DIR_IPI,
			GPIO_DIR_OUT);

		if (enable) {
			gpio_send_ipi_msg(gpio_dsp_ctrl_pin, SET_DOUT_IPI,
				GPIO_OUT_ONE);
			usleep_range(PDN_HIGH_DELAY_USEC,
				PDN_HIGH_DELAY_USEC + 200);
		} else {
			gpio_send_ipi_msg(gpio_dsp_ctrl_pin, SET_DOUT_IPI,
				GPIO_OUT_ZERO);
			usleep_range(PDN_LOW_DELAY_USEC,
				PDN_LOW_DELAY_USEC + 200);
		}
#endif
	} else {
		if (enable) {
			gpio_set_value_cansleep(reset_amp_gpio, 1);
			usleep_range(PDN_HIGH_DELAY_USEC,
				PDN_HIGH_DELAY_USEC + 200);
		} else {
			gpio_set_value_cansleep(reset_amp_gpio, 0);
			usleep_range(PDN_LOW_DELAY_USEC,
				PDN_LOW_DELAY_USEC + 200);
		}
	}

	pr_info("%s AP Gpio reset successful %d\n",
		__func__, gpio_get_value_cansleep(reset_amp_gpio));
}

int tas5805m_reset_platform(struct snd_soc_codec *codec, int enable)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	bool playback_active[] = {false, false};
	int status = 0;

	if (!tas5805m_data) {
		pr_err("%s tas5805m_priv Null. codec %p\n",
		__func__, codec);
		return -1;
	}

	mutex_lock(&tas5805m_data->lock);

	playback_active[0] = tas5805m_data->pcm_active;

	if (!enable) {
		/* Put device to mute and HiZ mode if playback is active */
		if (playback_active[0] == true) {
			tas5805m_apply_mute(codec, true);
			tas5805m_device_state(codec, true);
		}

		regcache_cache_only(tas5805m_data->regmap, true);
		regcache_mark_dirty(tas5805m_data->regmap);

		if (tas5805m_b) {
			playback_active[1] = tas5805m_b->pcm_active;
			if (playback_active[1]) {
				tas5805m_apply_mute(tas5805m_b->codec, true);
				tas5805m_device_state(tas5805m_b->codec, true);
			}

			regcache_cache_only(tas5805m_b->regmap, true);
			regcache_mark_dirty(tas5805m_b->regmap);
		}

		if (playback_active[0] || playback_active[1])
			usleep_range(6000, 6200);
	}

	tas5805m_pwr_down(enable);

	tas5805m_data->init_done = false;
	tas5805m_data->pcm_active = false;

	if (enable) {
		regcache_cache_only(tas5805m_data->regmap, false);
		status = regcache_sync(tas5805m_data->regmap);
		if (status)
			dev_err(codec->dev, "%s failed to sync registers. codec %d error %d\n",
				__func__, tas5805m_data->dac_num, status);
	}

	/* Update second codec if exist */
	if (tas5805m_b) {
		tas5805m_b->init_done = false;
		tas5805m_b->pcm_active = false;
		if (enable) {
			regcache_cache_only(tas5805m_b->regmap, false);
			status = regcache_sync(tas5805m_b->regmap);
			if (status)
				dev_err(codec->dev, "%s failed to sync registers. codec %d error %d\n",
					__func__, tas5805m_b->dac_num, status);
		}
	}

	mutex_unlock(&tas5805m_data->lock);

	pr_info("%s-: enable %d\n", __func__, enable);

	return status;
}
EXPORT_SYMBOL(tas5805m_reset_platform);

static int tas5805m_reset_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int status;

	status = tas5805m_reset_platform(codec, 0);
	if (status) {
		dev_err(codec->dev, "%s failed to disable amp. error %d\n",
			__func__, status);
		return status;
	}

	status = tas5805m_reset_platform(codec, 1);
	if (status)
		dev_err(codec->dev, "%s failed to enable amp. error %d\n",
			__func__, status);

	return status;
}

static int tas5805m_reset_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Always set 0. Otherwise userspace won't apply kcontrol */
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int tas5805m_force_unmute_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int status = 0;

	mutex_lock(&tas5805m_data->lock);
	pr_info("%s: force_unmute %d active %d\n", __func__,
		tas5805m_data->force_unmute, tas5805m_data->pcm_active);

	if (ucontrol->value.integer.value[0]) {
		tas5805m_data->force_unmute = true;
		/* apply config if init_done = false */
		if (!tas5805m_data->init_done) {
			status = tas5805m_apply_config(codec);
			tas5805m_data->init_done = true;
			pr_info("%s tas5805m_apply_config status %d\n",
			    __func__, status);
		}
		/* Unmute immediately if playback is not active */
		if (tas5805m_data->pcm_active == false) {
			tas5805m_device_state(codec, false);
			tas5805m_apply_mute(codec, false);
		}
	} else {
		tas5805m_data->force_unmute = false;
		/* Mute stream if PCM is not active */
		if (tas5805m_data->pcm_active == false) {
			tas5805m_apply_mute(codec, true);
			tas5805m_device_state(codec, true);
		}
	}
	mutex_unlock(&tas5805m_data->lock);

	return 0;
}

static int tas5805m_force_unmute_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tas5805m_data->lock);
	ucontrol->value.integer.value[0] = tas5805m_data->force_unmute;
	mutex_unlock(&tas5805m_data->lock);

	return 0;
}

static int tas5805m_fault_status_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	struct soc_bytes_ext *bytes_ext =
			(struct soc_bytes_ext *) kcontrol->private_value;

	if (bytes_ext->max != NUM_FAULT_REGS)
		return -EINVAL;

	mutex_lock(&tas5805m_data->lock);
	tas5805m_check_faults(codec);
	/* Byte values from register <0x70 0x71 0x72 0x73> */
	ucontrol->value.bytes.data[0] = tas5805m_data->ch_fault;
	ucontrol->value.bytes.data[1] = tas5805m_data->global_fault;
	ucontrol->value.bytes.data[2] = tas5805m_data->ot_shdn;
	ucontrol->value.bytes.data[3] = tas5805m_data->ot_warn;
	mutex_unlock(&tas5805m_data->lock);

	return 0;
}

static int tas5805m_recover_fault_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	/* Always set 0. Otherwise userspace won't apply kcontrol */
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int tas5805m_recover_fault_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	bool playback_active;

	if (ucontrol->value.enumerated.item[0] >=
		ARRAY_SIZE(fault_types_text)) {
		dev_err(codec->dev, "%s Invalid Fault paran %d\n",
				__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}

	mutex_lock(&tas5805m_data->lock);
	playback_active = tas5805m_data->pcm_active;

	if (ucontrol->value.enumerated.item[0] == ALL_FAULTS ||
		ucontrol->value.enumerated.item[0] == OVERCURRENT_FAULT) {
		/* Set device to HiZ state */
		tas5805m_set_register(codec, TAS5805M_BOOK_00,
			TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_HIZ_STATE);

		/* Delay for 5ms */
		usleep_range(FAULT_CLEAR_DELAY_USEC,
			FAULT_CLEAR_DELAY_USEC+100);

		/* Set back to Playback */
		tas5805m_set_register(codec, TAS5805M_BOOK_00,
			TAS5805M_PAGE_00, TAS5805M_REG_03, TAS5805M_PLAY_STATE);
	}

	/* Clear error register */
	tas5805m_clear_faults(codec);

	if (playback_active || tas5805m_data->force_unmute) {
		/* Unmute if playback is active */
		tas5805m_apply_mute(tas5805m_data->codec, false);
	} else {
		/* Otherwise put to Deep Sleep */
		tas5805m_device_state(tas5805m_data->codec, true);
	}
	mutex_unlock(&tas5805m_data->lock);

	dev_info(codec->dev, "%s Recover Fault %d active %d force %d\n",
			__func__, ucontrol->value.enumerated.item[0],
			playback_active, tas5805m_data->force_unmute);

	return 0;
}

static const struct soc_enum tas5805m_fault_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fault_types_text),
		fault_types_text);

static const struct snd_kcontrol_new tas5805m_controls_a[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "Master TAS5805m Playback Volume A",
		.info  = tas5805m_vol_info,
		.get   = tas5805m_vol_locked_get,
		.put   = tas5805m_vol_locked_put,
	},
	/* Reset codecs */
	SOC_SINGLE_BOOL_EXT("TAS5805m Reset Internal", 0, tas5805m_reset_get,
		tas5805m_reset_set),

	/* Turn on/off filters */
	SOC_SINGLE_BOOL_EXT("TAS5805m EQ Enable", 0, tas5805m_eq_locked_get,
		tas5805m_eq_locked_set),

	/* Force Unmute of AMP */
	SOC_SINGLE_BOOL_EXT("TAS5805m Force Unmute A", 0,
		tas5805m_force_unmute_get, tas5805m_force_unmute_set),

	/* Checks Fault Status registers: 0x70, 0x71, 0x72, 0x73 */
	SND_SOC_BYTES_EXT("TAS5805m Fault Status A", NUM_FAULT_REGS,
		tas5805m_fault_status_get, NULL),

	/* Clear faults */
	SOC_VALUE_ENUM_EXT("TAS5805m Recover Fault A", tas5805m_fault_ctrl_enum,
		tas5805m_recover_fault_get, tas5805m_recover_fault_set),

};

static const struct snd_kcontrol_new tas5805m_controls_b[] = {
	{
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name  = "Master TAS5805m Playback Volume B",
	.info  = tas5805m_vol_info,
	.get   = tas5805m_vol_locked_get,
	.put   = tas5805m_vol_locked_put,
	},

	/* Force Unmute of AMP */
	SOC_SINGLE_BOOL_EXT("TAS5805m Force Unmute B", 0,
		tas5805m_force_unmute_get, tas5805m_force_unmute_set),

	/* Checks Fault Status registers: 0x70, 0x71, 0x72, 0x73 */
	SND_SOC_BYTES_EXT("TAS5805m Fault Status B", NUM_FAULT_REGS,
		tas5805m_fault_status_get, NULL),

	/* Clear faults */
	SOC_VALUE_ENUM_EXT("TAS5805m Recover Fault B", tas5805m_fault_ctrl_enum,
		tas5805m_recover_fault_get, tas5805m_recover_fault_set),
};

static int tas5805m_snd_probe(struct snd_soc_codec *codec)
{
	struct tas5805m_priv *tas5805m = snd_soc_codec_get_drvdata(codec);
	int ret;

	if (tas5805m->dac_num == 0)
		ret = snd_soc_add_codec_controls(codec, tas5805m_controls_a,
				ARRAY_SIZE(tas5805m_controls_a));
	else
		ret = snd_soc_add_codec_controls(codec,
			tas5805m_controls_b, ARRAY_SIZE(tas5805m_controls_b));

	tas5805m->codec = codec;

	return ret;
}

static struct snd_soc_codec_driver soc_codec_tas5805m = {
	.probe = tas5805m_snd_probe,
};

static int tas5805m_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	if (tas5805m_data->force_unmute && mute) {
		pr_info("%s: mute skipped since force_unmute on\n", __func__);
		return 0;
	}
	tas5805m_apply_mute(codec, mute);

	return 0;
}

static int tas5805m_apply_seq(struct snd_soc_codec *codec,
				const struct reg_sequence *tas5805m_seq,
				size_t size_seq)
{
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int i, ret;
	unsigned int delay_usec;

	for (i = 0; i < size_seq; i++) {
		if (tas5805m_seq[i].reg == TAS5805M_DELAY) {
			delay_usec = tas5805m_seq[i].def*1000;
			usleep_range(delay_usec, delay_usec+200);
			dev_info(codec->dev, "Delay for %d usec\n", delay_usec);
		} else {
			/* Program registers */
			if (tas5805m_data->again >= 0  &&
				tas5805m_seq[i].reg == TAS5805M_REG_54) {
				ret = snd_soc_write(codec, tas5805m_seq[i].reg,
							tas5805m_data->again);
			} else {
				ret = snd_soc_write(codec, tas5805m_seq[i].reg,
							tas5805m_seq[i].def);
			}

			if (ret < 0) {
				dev_err(codec->dev, "i2c write failed reg: 0x%x val: 0x%x\n",
					tas5805m_seq[i].reg,
					tas5805m_seq[i].def);
				return ret;
			}
		}
	}

	return 0;
}

static int tas5805m_apply_config(struct snd_soc_codec *codec)
{
	int ret;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	switch (tas5805m_data->config) {
	case MONO:
		ret = tas5805m_apply_seq(codec, tas5805m_init_mono,
			sizeof(tas5805m_init_mono) /
			sizeof(struct reg_sequence));
		break;

	case WOOFER:
		if (tas5805m_data->eq_en == true) {
			ret = tas5805m_apply_seq(codec, tas5805m_init_woofer,
				sizeof(tas5805m_init_woofer) /
				sizeof(struct reg_sequence));
		} else {
			ret = tas5805m_apply_seq(codec,
				tas5805m_init_woofer_bypass,
				sizeof(tas5805m_init_woofer_bypass) /
				sizeof(struct reg_sequence));
		}
		break;

	case TWEETER:
		if (tas5805m_data->eq_en == true) {
			ret = tas5805m_apply_seq(codec, tas5805m_init_tweeter,
				sizeof(tas5805m_init_tweeter) /
				sizeof(struct reg_sequence));
		} else {
			ret = tas5805m_apply_seq(codec,
				tas5805m_init_tweeter_bypass,
				sizeof(tas5805m_init_tweeter_bypass) /
				sizeof(struct reg_sequence));
		}
		break;

	case MONO_MINI:
		ret = tas5805m_apply_seq(codec, tas5805m_init_mono_mini,
			sizeof(tas5805m_init_mono_mini) /
			sizeof(struct reg_sequence));
		break;

	case TWEETER_MINI:
		if (tas5805m_data->eq_en == true) {
			ret = tas5805m_apply_seq(codec, tas5805m_init_tweeter,
				sizeof(tas5805m_init_tweeter) /
				sizeof(struct reg_sequence));
		} else {
		ret = tas5805m_apply_seq(codec, tas5805m_init_tweeter_mini,
			sizeof(tas5805m_init_tweeter_mini) /
			sizeof(struct reg_sequence));
		}
		break;

	case STEREO:
	default:
		ret = tas5805m_apply_seq(codec, tas5805m_init_stereo,
			sizeof(tas5805m_init_stereo) /
			sizeof(struct reg_sequence));
	}
	if (ret < 0) {
		pr_err("%s: failed to set config\n", __func__);
		return ret;
	}
	tas5805m_data->eq_state = tas5805m_data->eq_en;
	return 0;
}

static int tas5805m_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int status = 0;

	mutex_lock(&tas5805m_data->lock);
	pr_info("%s+ codec %d config %d init %d\n", __func__,
		tas5805m_data->dac_num, tas5805m_data->config,
		tas5805m_data->init_done);

	if (!tas5805m_data) {
		mutex_unlock(&tas5805m_data->lock);
		pr_info("%s- NULL CODEC\n", __func__);
		return 0;
	}

	if (tas5805m_data->init_done == false ||
		tas5805m_data->eq_en != tas5805m_data->eq_state) {
		tas5805m_apply_mute(codec, true);
		status = tas5805m_apply_config(codec);
		tas5805m_data->init_done = true;
	} else
	    status = tas5805m_device_state(dai->codec, false);

	mutex_unlock(&tas5805m_data->lock);

	pr_info("%s- status %d\n", __func__, status);

	return status;
}

static int tas5805m_hw_free(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);
	int status = 0;

	mutex_lock(&tas5805m_data->lock);
	status = tas5805m_device_state(dai->codec, true);
	mutex_unlock(&tas5805m_data->lock);

	pr_info("%s status %d\n", __func__, status);

	return status;
}

static int tas5805m_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5805m_priv *tas5805m_data = snd_soc_codec_get_drvdata(codec);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		tas5805m_data->pcm_active = true;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		tas5805m_data->pcm_active = false;
		break;

	default:
		dev_err(codec->dev, "%s: Invalid command = %d\n",
			__func__, cmd);
	}

	pr_info("%s: command=%d active %d\n", __func__,
		cmd, tas5805m_data->pcm_active);

	return 0;
}

static const struct snd_soc_dai_ops tas5805m_dai_ops = {
	.hw_params	  = tas5805m_hw_params,
	.hw_free	  = tas5805m_hw_free,
	.digital_mute = tas5805m_mute,
	.trigger    = tas5805m_trigger,
};

static struct snd_soc_dai_driver tas5805m_dai_a = {
	.name       = "tas5805m-amplifier-a",
	.playback   = {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 4,
		.rates		= TAS5805M_RATES,
		.formats	= TAS5805M_FORMATS,
	},
	.ops = &tas5805m_dai_ops,
};

static struct snd_soc_dai_driver tas5805m_dai_b = {
	.name		= "tas5805m-amplifier-b",
	.playback   = {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 4,
		.rates		= TAS5805M_RATES,
		.formats	= TAS5805M_FORMATS,
	},
	.ops = &tas5805m_dai_ops,
};

static int tas5805m_parse_node(struct tas5805m_priv *tas5805m_data)
{
	int ret =  device_property_read_u32(tas5805m_data->dev, "dac-num",
					&tas5805m_data->dac_num);
	if (ret) {
		dev_err(tas5805m_data->dev, "%s: dac_num DT property missing\n",
			__func__);
		return ret;
	}

	ret = device_property_read_u32(tas5805m_data->dev, "speaker-config",
				&tas5805m_data->config);
	if (ret)
		pr_warn("%s: speaker-config not found. Setting to default\n",
			__func__);

	if (device_property_read_bool(tas5805m_data->dev,
		"swap-channels"))
		tas5805m_data->swap_lr = true;

	ret = device_property_read_u32(tas5805m_data->dev, "ana-gain",
				&tas5805m_data->again);
	if (ret) {
	    /* No AGAIN override needed */
		tas5805m_data->again = -1;
		ret = 0;
	}

	if (tas5805m_data->dac_num == 0) {

		if (device_property_read_bool(tas5805m_data->dev,
			"use-ap-gpio")) {

			reset_amp_gpio = of_get_named_gpio(
			    tas5805m_data->dev->of_node, "reset-gpio", 0);
			if (!gpio_is_valid(reset_amp_gpio)) {
				pr_err("%s: Failed to get reset gpio from dts %d!\n",
					__func__, reset_amp_gpio);
				return -EINVAL;
			}

			ret = devm_gpio_request_one(tas5805m_data->dev,
				reset_amp_gpio, 0, "tas5805m_reset");
			if (ret < 0) {
				pr_err("%s: Failed to request enable gpio! %d\n",
					__func__, ret);
				return -EINVAL;
			}

			ret = gpio_direction_output(reset_amp_gpio, 0);
			if (ret < 0) {
				pr_err("%s: could not set gpio(%d) to 0 (err=%d)\n",
					 __func__, gpio_dsp_ctrl_pin, ret);
				return -EINVAL;
			}
			usleep_range(PDN_LOW_DELAY_USEC,
				PDN_LOW_DELAY_USEC + 200);

			pr_info("%s: AP Reset enabled %d",
			    __func__, reset_amp_gpio);
		} else {
			ret = device_property_read_u32(tas5805m_data->dev,
				"reset-gpio", &gpio_dsp_ctrl_pin);
			if (ret) {
				pr_err("%s fail to get gpio_dsp_ctrl_pin\n",
					__func__);
				return ret;
			}
		}

		/* Turn On Amp*/
		tas5805m_pwr_down(1);
	}

	dev_info(tas5805m_data->dev, "%s:- codec %d reset-gpio %d config %u swap %d again %d\n",
		__func__, tas5805m_data->dac_num, reset_amp_gpio,
		tas5805m_data->config, tas5805m_data->swap_lr,
		tas5805m_data->again);

	return ret;
}

static int tas5805m_i2c_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	struct regmap_config config = tas5805m_regmap;
	struct tas5805m_priv *tas5805m_data;
	int ret;

	dev_info(&i2c->dev, "%s: %s+ codec_type = %d\n", __func__,
		id->name, (int)id->driver_data);

	tas5805m_data = devm_kzalloc(&i2c->dev, sizeof(struct tas5805m_priv),
								GFP_KERNEL);
	if (!tas5805m_data)
		return -ENOMEM;

	tas5805m_data->regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(tas5805m_data->regmap)) {
		ret = PTR_ERR(tas5805m_data->regmap);
		dev_err(&i2c->dev, "Failed to allocate 1st register map: %d\n",
			ret);
		return ret;
	}

	tas5805m_data->vol = 110;         //110, 0dB
	tas5805m_data->dev =  &i2c->dev;
	tas5805m_data->eq_en = false;
	tas5805m_data->eq_state = false;
	tas5805m_data->force_unmute = false;
	tas5805m_data->pcm_active = false;
	tas5805m_data->init_done = false;

	i2c_set_clientdata(i2c, tas5805m_data);

	tas5805m_parse_node(tas5805m_data);

	mutex_init(&tas5805m_data->lock);

	if (tas5805m_data->dac_num == 0)
		ret = snd_soc_register_codec(&i2c->dev,
					&soc_codec_tas5805m,
					&tas5805m_dai_a, 1);
	else {
		ret = snd_soc_register_codec(&i2c->dev,
					&soc_codec_tas5805m,
					&tas5805m_dai_b, 1);
		tas5805m_b = tas5805m_data;
	}

	if (ret)
		dev_err(&i2c->dev, "%s:- Failed to register codec %d. Error %d\n",
			__func__, tas5805m_data->dac_num, ret);
	else
		dev_info(&i2c->dev, "%s:- codec  = %d\n", __func__,
			tas5805m_data->dac_num);

	return ret;
}

static void tas5805m_i2c_shutdown(struct i2c_client *i2c)
{
	struct tas5805m_priv *tas5805m_data;

	dev_info(&i2c->dev, "%s\n", __func__);

	tas5805m_data = dev_get_drvdata(&i2c->dev);
	if (IS_ERR(tas5805m_data)) {
		dev_err(&i2c->dev, "Failed to get tas5805m_data\n");
		return;
	}

	if (tas5805m_data->dac_num == 0) {
		gpio_set_value_cansleep(reset_amp_gpio, 0);
		/* DEE-165952 - Delay recommended by HW Team */
		usleep_range(17000, 17200);
	}

	dev_info(&i2c->dev, "%s codec = %d\n", __func__,
		tas5805m_data->dac_num);
}

static int tas5805m_remove(struct device *dev)
{
	snd_soc_unregister_codec(dev);

	return 0;
}

static int tas5805m_i2c_remove(struct i2c_client *i2c)
{
	tas5805m_remove(&i2c->dev);

	return 0;
}

static const struct i2c_device_id tas5805m_i2c_id[] = {
	{ "tas5805m", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5805m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5805m_of_match[] = {
	{ .compatible = "ti,tas5805m", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver tas5805m_i2c_driver = {
	.probe      = tas5805m_i2c_probe,
	.shutdown   = tas5805m_i2c_shutdown,
	.remove     = tas5805m_i2c_remove,
	.id_table   = tas5805m_i2c_id,
	.driver     = {
		.name   = TAS5805M_DRV_NAME,
		.of_match_table = tas5805m_of_match,
	},
};

module_i2c_driver(tas5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_AUTHOR("C. Omer Rafique <rafiquec@amazon.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
