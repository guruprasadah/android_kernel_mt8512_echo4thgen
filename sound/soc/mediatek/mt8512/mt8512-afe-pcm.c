/*
 * mt8512-afe-pcm.c  --  Mediatek 8512 ALSA SoC AFE platform driver
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/mfd/syscon.h>
#include <linux/atomic.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include "mt8512-afe-common.h"
#include "mt8512-afe-utils.h"
#include "mt8512-reg.h"
#include "mt8512-afe-debug.h"
#include "mt8512-afe-controls.h"
#include "../common/mtk-base-afe.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"


#define MT8512_ETDM1_IN_MCLK_MULTIPLIER 256
#define MT8512_ETDM2_OUT_MCLK_MULTIPLIER 256
#define MT8512_ETDM2_IN_MCLK_MULTIPLIER 256
#define MT8512_ETDM_NORMAL_MAX_BCK_RATE 24576000

#define AFE_BASE_END_OFFSET 8

#define PCM_STREAM_STR(x) \
	(((x) == SNDRV_PCM_STREAM_CAPTURE) ? "capture" : "playback")

// TODO: revise registers to backup
static const unsigned int mt8512_afe_backup_list[] = {
	AUDIO_TOP_CON0,
	AUDIO_TOP_CON2,
	AUDIO_TOP_CON4,
	AUDIO_TOP_CON5,
	AFE_CONN1,
	AFE_CONN2,
	AFE_CONN3,
	AFE_CONN4,
	AFE_CONN5,
	AFE_CONN6,
	AFE_CONN7,
	AFE_CONN8,
	AFE_CONN9,
	AFE_CONN10,
	AFE_CONN11,
	AFE_CONN12,
	AFE_CONN13,
	AFE_CONN14,
	AFE_CONN15,
	AFE_CONN16,
	AFE_CONN17,
	AFE_CONN18,
	AFE_CONN19,
	AFE_CONN20,
	AFE_CONN21,
	AFE_CONN26,
	AFE_CONN27,
	AFE_CONN28,
	AFE_CONN29,
	AFE_CONN30,
	AFE_CONN31,
	AFE_CONN32,
	AFE_CONN33,
	AFE_CONN34,
	AFE_CONN35,
	AFE_CONN36,
	AFE_CONN37,
	AFE_CONN38,
	AFE_CONN39,
	AFE_CONN40,
	AFE_CONN41,
	AFE_CONN42,
	AFE_CONN43,
	AFE_CONN44,
	AFE_CONN45,
	AFE_CONN46,
	AFE_CONN47,
	AFE_CONN48,
	AFE_CONN49,
	AFE_CONN53,
	AFE_CONN54,
	AFE_CONN55,
	AFE_CONN56,
	AFE_CONN57,
	AFE_CONN58,
	AFE_CONN59,
	AFE_CONN60,
	AFE_CONN61,
	AFE_CONN64,
	AFE_CONN0_1,
	AFE_CONN1_1,
	AFE_CONN2_1,
	AFE_CONN3_1,
	AFE_CONN4_1,
	AFE_CONN5_1,
	AFE_CONN6_1,
	AFE_CONN7_1,
	AFE_CONN8_1,
	AFE_CONN9_1,
	AFE_CONN10_1,
	AFE_CONN11_1,
	AFE_CONN12_1,
	AFE_CONN13_1,
	AFE_CONN14_1,
	AFE_CONN15_1,
	AFE_CONN16_1,
	AFE_CONN17_1,
	AFE_CONN18_1,
	AFE_CONN19_1,
	AFE_CONN20_1,
	AFE_CONN21_1,
	AFE_CONN22_1,
	AFE_CONN23_1,
	AFE_CONN24_1,
	AFE_CONN25_1,
	AFE_CONN26_1,
	AFE_CONN27_1,
	AFE_CONN28_1,
	AFE_CONN29_1,
	AFE_CONN30_1,
	AFE_CONN31_1,
	AFE_CONN32_1,
	AFE_CONN33_1,
	AFE_CONN34_1,
	AFE_CONN35_1,
	AFE_CONN36_1,
	AFE_CONN37_1,
	AFE_CONN38_1,
	AFE_CONN39_1,
	AFE_CONN40_1,
	AFE_CONN41_1,
	AFE_CONN42_1,
	AFE_CONN43_1,
	AFE_CONN44_1,
	AFE_CONN45_1,
	AFE_CONN46_1,
	AFE_CONN47_1,
	AFE_CONN48_1,
	AFE_CONN49_1,
	AFE_CONN_RS,
	AFE_CONN_RS_1,
	AFE_CONN_16BIT,
	AFE_CONN_24BIT,
	AFE_CONN_16BIT_1,
	AFE_CONN_24BIT_1,
	AFE_DL2_BASE,
	AFE_DL2_END,
	AFE_DL3_BASE,
	AFE_DL3_END,
	AFE_DL6_BASE,
	AFE_DL6_END,
	AFE_DL10_BASE,
	AFE_DL10_END,
	AFE_UL1_BASE,
	AFE_UL1_END,
	AFE_UL2_BASE,
	AFE_UL2_END,
	AFE_UL3_BASE,
	AFE_UL3_END,
	AFE_UL4_BASE,
	AFE_UL4_END,
	AFE_UL5_BASE,
	AFE_UL5_END,
	AFE_UL8_BASE,
	AFE_UL8_END,
	AFE_UL9_BASE,
	AFE_UL9_END,
	AFE_UL10_BASE,
	AFE_UL10_END,
	AFE_DAC_CON0,
	AFE_IRQ_MASK,
	ETDM_IN2_CON0,
};

static const struct snd_pcm_hardware mt8512_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.buffer_bytes_max = 256 * 1024,
	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mtk_base_afe *g_priv;
#endif

struct mt8512_afe_rate {
	unsigned int rate;
	unsigned int reg_val;
};

static const struct mt8512_afe_rate mt8512_afe_fs_rates[] = {
	{ .rate = 8000, .reg_val = MT8512_FS_8K },
	{ .rate = 12000, .reg_val = MT8512_FS_12K },
	{ .rate = 16000, .reg_val = MT8512_FS_16K },
	{ .rate = 24000, .reg_val = MT8512_FS_24K },
	{ .rate = 32000, .reg_val = MT8512_FS_32K },
	{ .rate = 48000, .reg_val = MT8512_FS_48K },
	{ .rate = 96000, .reg_val = MT8512_FS_96K },
	{ .rate = 192000, .reg_val = MT8512_FS_192K },
	{ .rate = 384000, .reg_val = MT8512_FS_384K },
	{ .rate = 7350, .reg_val = MT8512_FS_7P35K },
	{ .rate = 11025, .reg_val = MT8512_FS_11P025K },
	{ .rate = 14700, .reg_val = MT8512_FS_14P7K },
	{ .rate = 22050, .reg_val = MT8512_FS_22P05K },
	{ .rate = 29400, .reg_val = MT8512_FS_29P4K },
	{ .rate = 44100, .reg_val = MT8512_FS_44P1K },
	{ .rate = 88200, .reg_val = MT8512_FS_88P2K },
	{ .rate = 176400, .reg_val = MT8512_FS_176P4K },
	{ .rate = 352800, .reg_val = MT8512_FS_352P8K },
};

static int mt8512_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8512_afe_fs_rates); i++)
		if (mt8512_afe_fs_rates[i].rate == rate)
			return mt8512_afe_fs_rates[i].reg_val;

	return -EINVAL;
}

static int mt8512_afe_irq_direction_enable(struct mtk_base_afe *afe,
	int irq_id,
	int direction)
{
	struct mtk_base_afe_irq *irq;

	if (irq_id >= MT8512_AFE_IRQ_NUM)
		return -1;

	irq = &afe->irqs[irq_id];

	if (direction == MT8512_AFE_IRQ_DIR_MCU) {
		regmap_update_bits(afe->regmap, ASYS_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       0);
		regmap_update_bits(afe->regmap, AFE_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
	} else if (direction == MT8512_AFE_IRQ_DIR_DSP) {
		regmap_update_bits(afe->regmap, ASYS_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
		regmap_update_bits(afe->regmap, AFE_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       0);
	} else {
		regmap_update_bits(afe->regmap, ASYS_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
		regmap_update_bits(afe->regmap, AFE_IRQ_MASK,
		       (1 << irq->irq_data->irq_clr_shift),
		       (1 << irq->irq_data->irq_clr_shift));
	}
	return 0;
}


bool mt8512_afe_rate_supported(unsigned int rate, unsigned int id)
{
	switch (id) {
	case MT8512_AFE_IO_ETDM1_IN:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_ETDM2_OUT:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_ETDM2_IN:
		if (rate >= 8000 && rate <= 384000)
			return true;
		break;
	case MT8512_AFE_IO_DMIC:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_PCM1:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_INT_ADDA:
		if (rate >= 8000 && rate <= 48000)
			return true;
		break;
	default:
		break;
	}

	return false;
}

bool mt8512_afe_channel_supported(unsigned int channel, unsigned int id)
{
	switch (id) {
	case MT8512_AFE_IO_ETDM1_IN:
		if (channel >= 1 && channel <= 16)
			return true;
		break;
	case MT8512_AFE_IO_ETDM2_OUT:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_ETDM2_IN:
		if (channel >= 1 && channel <= 8)
			return true;
		break;
	case MT8512_AFE_IO_DMIC:
		if (channel >= 1 && channel <= 8)
			return true;
		break;
	case MT8512_AFE_IO_PCM1:
		/* FALLTHROUGH */
	case MT8512_AFE_IO_INT_ADDA:
		if (channel >= 1 && channel <= 2)
			return true;
		break;
	default:
		break;
	}

	return false;
}

static bool is_ul3_in_direct_mode(struct mtk_base_afe *afe)
{
	unsigned int val = 0;

	regmap_read(afe->regmap, ETDM_COWORK_CON1, &val);

	if (val & ETDM_COWORK_CON1_TDM_IN2_BYPASS_INTERCONN)
		return true;
	else
		return false;
}

static int mt8512_afe_gasrc_get_period_val(unsigned int rate,
	bool op_freq_45m, unsigned int cali_cycles);

static void mt8512_afe_inc_etdm_occupy(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int stream)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data = &afe_priv->etdm_data[etdm_set];
	unsigned long flags;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	etdm_data->occupied[stream]++;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);
}

static void mt8512_afe_dec_etdm_occupy(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int stream)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data = &afe_priv->etdm_data[etdm_set];
	unsigned long flags;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	etdm_data->occupied[stream]--;
	if (etdm_data->occupied[stream] < 0)
		etdm_data->occupied[stream] = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);
}

static unsigned int mt8512_afe_tdm_ch_fixup(unsigned int channels,
	unsigned int etdm_set)
{
	if ((channels > 8) && (!etdm_set))
		return 16;
	else if (channels > 4)
		return 8;
	else if (channels > 2)
		return 4;
	else
		return 2;
}

static const struct mt8512_etdm_ctrl_reg etdm_ctrl_reg[MT8512_ETDM_SETS][2] = {
	{
		{
			.con0 = -1,
			.con1 = -1,
			.con2 = -1,
			.con3 = -1,
			.con4 = -1,
		},
		{
			.con0 = ETDM_IN1_CON0,
			.con1 = ETDM_IN1_CON1,
			.con2 = ETDM_IN1_CON2,
			.con3 = ETDM_IN1_CON3,
			.con4 = ETDM_IN1_CON4,
		},
	},
	{
		{
			.con0 = ETDM_OUT2_CON0,
			.con1 = ETDM_OUT2_CON1,
			.con2 = ETDM_OUT2_CON2,
			.con3 = ETDM_OUT2_CON3,
			.con4 = ETDM_OUT2_CON4,
		},
		{
			.con0 = ETDM_IN2_CON0,
			.con1 = ETDM_IN2_CON1,
			.con2 = ETDM_IN2_CON2,
			.con3 = ETDM_IN2_CON3,
			.con4 = ETDM_IN2_CON4,
		},
	},
};

static void mt8512_afe_enable_etdm(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int stream)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int en_reg = etdm_ctrl_reg[etdm_set][stream].con0;
	unsigned long flags;
	bool need_update = false;

	dev_info(afe->dev, "%s, etdm_set %u\n", __func__, etdm_set);

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	etdm_data->active[stream]++;
	if (etdm_data->active[stream] == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, en_reg, 0x1, 0x1);
}

static void mt8512_afe_disable_etdm(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int stream)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	bool slave_mode = etdm_data->slave_mode[stream];
	unsigned int reg;
	unsigned long flags;
	bool need_update = false;

	dev_info(afe->dev, "%s, etdm_set %u\n", __func__, etdm_set);

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	etdm_data->active[stream]--;
	if (etdm_data->active[stream] == 0)
		need_update = true;
	else if (etdm_data->active[stream] < 0)
		etdm_data->active[stream] = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update) {
		reg = etdm_ctrl_reg[etdm_set][stream].con0;
		regmap_update_bits(afe->regmap, reg, 0x1, 0x0);

		if (slave_mode) {
			reg = etdm_ctrl_reg[etdm_set][stream].con4;
			regmap_update_bits(afe->regmap, reg,
					   ETDM_CON4_ASYNC_RESET,
					   ETDM_CON4_ASYNC_RESET);
			regmap_update_bits(afe->regmap, reg,
					   ETDM_CON4_ASYNC_RESET,
					   0);
		}
	}
}

static bool mt8512_afe_is_etdm_low_power(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int stream)
{
	unsigned int lp_mode_reg = etdm_ctrl_reg[etdm_set][stream].con0;
	unsigned int val;
	bool lp_mode;

	regmap_read(afe->regmap, lp_mode_reg, &val);

	lp_mode = (ETDM_CON0_LOW_POWER_MODE&val)?true:false;

	return lp_mode;
}

static bool mt8512_afe_is_int_1x_en_low_power(struct mtk_base_afe *afe)
{
	unsigned int val;
	bool lp_mode;

	regmap_read(afe->regmap, ASYS_TOP_CON, &val);

	lp_mode = (ASYS_TCON_LP_MODE_ON&val)?true:false;

	return lp_mode;
}

static int mt8512_afe_configure_etdm_out(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int rate,
	unsigned int channels, unsigned int bit_width)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int tdm_channels;
	unsigned int stream = SNDRV_PCM_STREAM_PLAYBACK;
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int data_mode = etdm_data->data_mode[stream];
	unsigned int lrck_width = etdm_data->lrck_width[stream];
	bool slave_mode = etdm_data->slave_mode[stream];
	bool lrck_inv = etdm_data->lrck_inv[stream];
	bool bck_inv = etdm_data->bck_inv[stream];
	unsigned int fmt = etdm_data->format[stream];
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	unsigned int val = 0;
	unsigned int bck;

	dev_info(afe->dev, "%s#%u rate:%u ch:%u bits:%u slave:%d data:%u\n",
		__func__, etdm_set + 1, rate, channels, bit_width,
		slave_mode, data_mode);

	tdm_channels = (data_mode == MT8512_ETDM_DATA_ONE_PIN) ?
		mt8512_afe_tdm_ch_fixup(channels, etdm_set) : 2;

	bck = rate * tdm_channels * bit_width;

	val |= ETDM_CON0_BIT_LEN(bit_width);
	val |= ETDM_CON0_WORD_LEN(bit_width);
	val |= ETDM_CON0_FORMAT(fmt);
	val |= ETDM_CON0_CH_NUM(tdm_channels);

	if (clk_mode == MT8512_ETDM_SHARED_CLOCK)
		val |= ETDM_CON0_SYNC_MODE;

	if (slave_mode) {
		val |= ETDM_CON0_SLAVE_MODE;
		if (lrck_inv)
			val |= ETDM_CON0_SLAVE_LRCK_INV;
		if (bck_inv)
			val |= ETDM_CON0_SLAVE_BCK_INV;
	} else {
		if (lrck_inv)
			val |= ETDM_CON0_MASTER_LRCK_INV;
		if (bck_inv)
			val |= ETDM_CON0_MASTER_BCK_INV;
	}

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con0;
	ctrl_mask = ETDM_OUT_CON0_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	val = 0;

	val |= ETDM_CON1_LRCK_MANUAL_MODE;

	if (!slave_mode) {
		val |= ETDM_CON1_MCLK_OUTPUT;

		if (bck > MT8512_ETDM_NORMAL_MAX_BCK_RATE)
			val |= ETDM_CON1_BCK_FROM_DIVIDER;
	}

	if (lrck_width > 0)
		val |= ETDM_OUT_CON1_LRCK_WIDTH(lrck_width);
	else
		val |= ETDM_OUT_CON1_LRCK_WIDTH(bit_width);

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con1;
	ctrl_mask = ETDM_OUT_CON1_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	val = ETDM_OUT_CON4_FS(mt8512_afe_fs_timing(rate));
	ctrl_mask = ETDM_OUT_CON4_CTRL_MASK;

	if (etdm_set == MT8512_ETDM2) {
		if (slave_mode)
			val |= ETDM_OUT_CON4_CONN_FS(MT8512_FS_ETDMOUT2_1X_EN);
		else
			val |= ETDM_OUT_CON4_CONN_FS(
				mt8512_afe_fs_timing(rate));
		ctrl_mask |= ETDM_OUT_CON4_INTERCONN_EN_SEL_MASK;
	}

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con4;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	return 0;
}

static int mt8512_afe_configure_etdm_in(struct mtk_base_afe *afe,
	unsigned int etdm_set, unsigned int rate,
	unsigned int channels, unsigned int bit_width)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int tdm_channels;
	unsigned int stream = SNDRV_PCM_STREAM_CAPTURE;
	unsigned int data_mode = etdm_data->data_mode[stream];
	unsigned int lrck_width = etdm_data->lrck_width[stream];
	bool slave_mode = etdm_data->slave_mode[stream];
	bool lrck_inv = etdm_data->lrck_inv[stream];
	bool bck_inv = etdm_data->bck_inv[stream];
	unsigned int fmt = etdm_data->format[stream];
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	unsigned int val = 0;
	unsigned int bck;

	dev_info(afe->dev, "%s#%u rate:%u ch:%u bits:%u slave:%d data:%u\n",
		__func__, etdm_set + 1, rate, channels, bit_width,
		slave_mode, data_mode);

	tdm_channels = (data_mode == MT8512_ETDM_DATA_ONE_PIN) ?
		mt8512_afe_tdm_ch_fixup(channels, etdm_set) : 2;

	val |= ETDM_CON0_BIT_LEN(bit_width);
	val |= ETDM_CON0_WORD_LEN(bit_width);
	val |= ETDM_CON0_FORMAT(fmt);
	val |= ETDM_CON0_CH_NUM(tdm_channels);

	bck = rate * tdm_channels * bit_width;

	if (slave_mode) {
		val |= ETDM_CON0_SLAVE_MODE;
		if (lrck_inv)
			val |= ETDM_CON0_SLAVE_LRCK_INV;
		if (bck_inv)
			val |= ETDM_CON0_SLAVE_BCK_INV;
	} else {
		if (lrck_inv)
			val |= ETDM_CON0_MASTER_LRCK_INV;
		if (bck_inv)
			val |= ETDM_CON0_MASTER_BCK_INV;
	}

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con0;
	ctrl_mask = ETDM_IN_CON0_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	val = 0;

	val |= ETDM_CON1_LRCK_MANUAL_MODE;

	val |= ETDM_CON1_MCLK_OUTPUT;

	if (bck > MT8512_ETDM_NORMAL_MAX_BCK_RATE)
		val |= ETDM_CON1_BCK_FROM_DIVIDER;

	if (lrck_width > 0)
		val |= ETDM_IN_CON1_LRCK_WIDTH(lrck_width);
	else
		val |= ETDM_IN_CON1_LRCK_WIDTH(bit_width);

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con1;
	ctrl_mask = ETDM_IN_CON1_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	val = ETDM_IN_CON3_FS(mt8512_afe_fs_timing(rate));

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con3;
	ctrl_mask = ETDM_IN_CON3_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	if (slave_mode) {
		if (etdm_set == MT8512_ETDM1)
			val = ETDM_IN_CON4_CONN_FS(MT8512_FS_ETDMIN1_1X_EN);
		else
			val = ETDM_IN_CON4_CONN_FS(MT8512_FS_ETDMIN2_1X_EN);
	} else
		val = ETDM_IN_CON4_CONN_FS(
			mt8512_afe_fs_timing(rate));

	ctrl_mask = ETDM_IN_CON4_CTRL_MASK;

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con4;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	val = 0;
	if (data_mode == MT8512_ETDM_DATA_MULTI_PIN) {
		val |= ETDM_IN_CON2_MULTI_IP_2CH_MODE |
		       ETDM_IN_CON2_MULTI_IP_ONE_DATA |
		       ETDM_IN_CON2_MULTI_IP_CH(tdm_channels);
	}

	val |= ETDM_IN_CON2_UPDATE_POINT_AUTO_DIS |
	       ETDM_IN_CON2_UPDATE_POINT(1);

	ctrl_reg = etdm_ctrl_reg[etdm_set][stream].con2;
	ctrl_mask = ETDM_IN_CON2_CTRL_MASK;
	regmap_update_bits(afe->regmap, ctrl_reg, ctrl_mask, val);

	return 0;
}

static void mt8512_afe_etdm1_in_force_on(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm1 = &priv->etdm_data[MT8512_ETDM1];
	unsigned int mclk, rate;
	bool need_tuner = false;
	bool slave_mode = false;

	mclk = etdm1->mclk_freq[SNDRV_PCM_STREAM_CAPTURE];
	rate = etdm1->force_rate[SNDRV_PCM_STREAM_CAPTURE];
	slave_mode = etdm1->slave_mode[SNDRV_PCM_STREAM_CAPTURE];

	if (rate % 8000)
		mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_FA2SYS]);

	if (mclk == 0)
		mclk = MT8512_ETDM1_IN_MCLK_MULTIPLIER * rate;

	mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_TDMIN_MCK]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_TDM_IN);

	mt8512_afe_configure_etdm_in(afe,
		MT8512_ETDM1,
		rate,
		etdm1->force_channels[SNDRV_PCM_STREAM_CAPTURE],
		etdm1->force_bit_width[SNDRV_PCM_STREAM_CAPTURE]);

	need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
		  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM1,
				SNDRV_PCM_STREAM_CAPTURE))
		 || (!mt8512_afe_is_int_1x_en_low_power(afe)
		   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM1,
				SNDRV_PCM_STREAM_CAPTURE));

	if (need_tuner) {
		if (rate % 8000) {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL1);
		} else {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL2);
		}
	}

	mt8512_afe_set_clk_parent(afe,
		priv->clocks[MT8512_CLK_TDMIN_M_SEL],
		(rate % 8000) ? priv->clocks[MT8512_CLK_AUD1] :
		priv->clocks[MT8512_CLK_AUD2]);
	mt8512_afe_set_clk_rate(afe, priv->clocks[MT8512_CLK_APLL12_DIV1],
		mclk);

	if (!slave_mode)
		mt8512_afe_enable_etdm(afe, MT8512_ETDM1,
			SNDRV_PCM_STREAM_CAPTURE);

	etdm1->force_on_status[SNDRV_PCM_STREAM_CAPTURE] = true;
}

static void mt8512_afe_etdm2_out_force_on(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm2 = &priv->etdm_data[MT8512_ETDM2];
	unsigned int mclk, rate;
	bool need_tuner = false;
	bool slave_mode = false;

	mclk = etdm2->mclk_freq[SNDRV_PCM_STREAM_PLAYBACK];
	rate = etdm2->force_rate[SNDRV_PCM_STREAM_PLAYBACK];
	slave_mode = etdm2->slave_mode[SNDRV_PCM_STREAM_PLAYBACK];

	if (rate % 8000)
		mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_FA2SYS]);

	if (mclk == 0)
		mclk = MT8512_ETDM2_OUT_MCLK_MULTIPLIER * rate;

	mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_I2SOUT_MCK]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_I2S_OUT);

	mt8512_afe_configure_etdm_out(afe,
		MT8512_ETDM2,
		rate,
		etdm2->force_channels[SNDRV_PCM_STREAM_PLAYBACK],
		etdm2->force_bit_width[SNDRV_PCM_STREAM_PLAYBACK]);

	need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
		  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
				SNDRV_PCM_STREAM_PLAYBACK))
		 || (!mt8512_afe_is_int_1x_en_low_power(afe)
		   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
				SNDRV_PCM_STREAM_PLAYBACK));

	if (need_tuner) {
		if (rate % 8000) {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL1);
		} else {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL2);
		}
	}

	mt8512_afe_set_clk_parent(afe,
		priv->clocks[MT8512_CLK_I2SOUT_M_SEL],
		(rate % 8000) ?
		priv->clocks[MT8512_CLK_AUD1] :
		priv->clocks[MT8512_CLK_AUD2]);
	mt8512_afe_set_clk_rate(afe,
		priv->clocks[MT8512_CLK_APLL12_DIV2],
		mclk);

	if (!slave_mode)
		mt8512_afe_enable_etdm(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_PLAYBACK);

	etdm2->force_on_status[SNDRV_PCM_STREAM_PLAYBACK] = true;
}

static void mt8512_afe_etdm2_in_force_on(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm2 = &priv->etdm_data[MT8512_ETDM2];
	unsigned int mclk, rate;
	bool need_tuner = false;
	bool slave_mode = false;

	mclk = etdm2->mclk_freq[SNDRV_PCM_STREAM_CAPTURE];
	rate = etdm2->force_rate[SNDRV_PCM_STREAM_CAPTURE];
	slave_mode = etdm2->slave_mode[SNDRV_PCM_STREAM_CAPTURE];

	if (rate % 8000)
		mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_FA2SYS]);

	if (mclk == 0)
		mclk = MT8512_ETDM2_IN_MCLK_MULTIPLIER * rate;

	mt8512_afe_enable_clk(afe, priv->clocks[MT8512_CLK_I2SIN_MCK]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_I2S_IN);

	mt8512_afe_configure_etdm_in(afe,
		MT8512_ETDM2,
		rate,
		etdm2->force_channels[SNDRV_PCM_STREAM_CAPTURE],
		etdm2->force_bit_width[SNDRV_PCM_STREAM_CAPTURE]);

	need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
		  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_CAPTURE))
		 || (!mt8512_afe_is_int_1x_en_low_power(afe)
		   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_CAPTURE));

	if (need_tuner) {
		if (rate % 8000) {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL1);
		} else {
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL2);
		}
	}

	mt8512_afe_set_clk_parent(afe,
		priv->clocks[MT8512_CLK_I2SIN_M_SEL],
		(rate % 8000) ?
		priv->clocks[MT8512_CLK_AUD1] :
		priv->clocks[MT8512_CLK_AUD2]);
	mt8512_afe_set_clk_rate(afe,
		priv->clocks[MT8512_CLK_APLL12_DIV0],
		mclk);

	if (!slave_mode)
		mt8512_afe_enable_etdm(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_CAPTURE);

	etdm2->force_on_status[SNDRV_PCM_STREAM_CAPTURE] = true;
}

static int mt8512_afe_prepare_etdm_out(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai,
	unsigned int etdm_set)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int rate = dai->rate;
	unsigned int channels = dai->channels;
	unsigned int bit_width = dai->sample_bits;
	unsigned int stream = SNDRV_PCM_STREAM_PLAYBACK;

	dev_info(dai->dev, "%s#%u rate:%u ch:%u bits:%u slave:%d data:%u\n",
		__func__, etdm_set + 1, rate, channels, bit_width,
		etdm_data->slave_mode[stream],
		etdm_data->data_mode[stream]);

	mt8512_afe_configure_etdm_out(afe, etdm_set, rate,
		channels, bit_width);

	return 0;
}

static int mt8512_afe_prepare_etdm_in(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai,
	unsigned int etdm_set)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int rate = dai->rate;
	unsigned int channels = dai->channels;
	unsigned int bit_width = dai->sample_bits;
	unsigned int stream = SNDRV_PCM_STREAM_CAPTURE;

	dev_info(dai->dev, "%s#%u rate:%u ch:%u bits:%u slave:%d data:%u\n",
		__func__, etdm_set + 1, rate, channels, bit_width,
		etdm_data->slave_mode[stream],
		etdm_data->data_mode[stream]);

	mt8512_afe_configure_etdm_in(afe, etdm_set, rate,
		channels, bit_width);

	return 0;
}

static int mt8512_afe_etdm1_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[MT8512_ETDM1];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;

	dev_info(dai->dev, "%s stream %u clk_mode %u\n",
		__func__, stream, clk_mode);

	if (etdm_data->force_on[stream]) {
		bool force_apply_in_change =
			!etdm_data->force_on_status[SNDRV_PCM_STREAM_CAPTURE]
			&&
			(etdm_data->force_on_policy[SNDRV_PCM_STREAM_CAPTURE]
			 == MT8512_ETDM_FORCE_ON_1ST_TRIGGER);

		if (force_apply_in_change) {
			mt8512_afe_enable_main_clk(afe);

			if (force_apply_in_change)
				mt8512_afe_etdm1_in_force_on(afe);
		}

		return 0;
	}

	mt8512_afe_enable_main_clk(afe);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		dev_info(dai->dev, "%s stream %u is not supported!\n",
		__func__, stream);

	if (stream == SNDRV_PCM_STREAM_CAPTURE ||
	    clk_mode == MT8512_ETDM_SHARED_CLOCK) {
		mt8512_afe_enable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TDMIN_MCK]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_TDM_IN);
	}

	return 0;
}

static void mt8512_afe_etdm1_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8512_AFE_BACKEND_BASE];
	unsigned int etdm_set = MT8512_ETDM1;
	struct mt8512_etdm_data *etdm_data = &afe_priv->etdm_data[etdm_set];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;
	bool reset_in_change = (stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	unsigned int rate = substream->runtime->rate;
	bool need_tuner = false;
	bool slave_mode = etdm_data->slave_mode[stream];

	dev_info(dai->dev, "%s stream %u clk_mode %u\n",
		__func__, stream, clk_mode);

	if (reset_in_change) {
		if ((etdm_data->occupied[stream] == 1)
			&& !etdm_data->force_on[stream]) {
			need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
				&& !mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_CAPTURE))
				|| (!mt8512_afe_is_int_1x_en_low_power(afe)
				&& mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_CAPTURE));

			if (need_tuner) {
				if (rate % 8000) {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL1);
				} else {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL2);
				}
			}
		}
		mt8512_afe_dec_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);
	}

	if (etdm_data->force_on[stream] && !slave_mode)
		return;

	if (etdm_data->occupied[stream] == 0) {
		if (reset_in_change)
			mt8512_afe_disable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE);
	}

	if (etdm_data->force_on[stream] && slave_mode)
		return;

	if (reset_in_change) {
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_TDM_IN);
		mt8512_afe_disable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TDMIN_MCK]);
	}

	if (be->prepared[stream]) {
		be->prepared[stream] = false;
		if (rate % 8000)
			mt8512_afe_disable_clk(afe,
					afe_priv->clocks[MT8512_CLK_FA2SYS]);
	}

	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_etdm1_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8512_AFE_BACKEND_BASE];
	unsigned int etdm_set = MT8512_ETDM1;
	struct mt8512_etdm_data *etdm_data = &afe_priv->etdm_data[etdm_set];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;
	bool apply_in_change = (stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	bool need_tuner = false;
	bool slave_mode = etdm_data->slave_mode[stream];
	unsigned int rate = dai->rate;
	unsigned int mclk = afe_priv->etdm_data[etdm_set].mclk_freq[stream];
	int ret;

	dev_info(dai->dev, "%s stream %u clk_mode %u occupied %d\n",
		__func__, stream, clk_mode, etdm_data->occupied[stream]);

	if (etdm_data->occupied[stream] || be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared %d, occupied %d\n",
			__func__, snd_pcm_stream_str(substream),
			be->prepared[stream], etdm_data->occupied[stream]);
		return 0;
	}

	if (apply_in_change)
		mt8512_afe_inc_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);

	if (etdm_data->force_on[stream]) {
		if (apply_in_change && slave_mode)
			mt8512_afe_enable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE);
		return 0;
	}

	if (apply_in_change) {
		ret = mt8512_afe_prepare_etdm_in(afe, substream, dai,
						 etdm_set);
		if (ret)
			return ret;
	}

	if (rate % 8000)
		mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FA2SYS]);

	if (apply_in_change) {
		if (mclk == 0)
			mclk = MT8512_ETDM1_IN_MCLK_MULTIPLIER * rate;

		need_tuner =
			(mt8512_afe_is_int_1x_en_low_power(afe)
			&& !mt8512_afe_is_etdm_low_power(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE))
			|| (!mt8512_afe_is_int_1x_en_low_power(afe)
			&& mt8512_afe_is_etdm_low_power(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL1);
			} else {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8512_CLK_TDMIN_M_SEL],
			(rate % 8000) ?
			afe_priv->clocks[MT8512_CLK_AUD1] :
			afe_priv->clocks[MT8512_CLK_AUD2]);
		mt8512_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8512_CLK_APLL12_DIV1],
			mclk);
		mt8512_afe_enable_etdm(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);
	}

	be->prepared[stream] = true;

	return 0;
}

static int mt8512_afe_etdm2_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[MT8512_ETDM2];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;

	dev_info(dai->dev, "%s stream %u clk_mode %u occupied %d\n",
		__func__, stream, clk_mode, etdm_data->occupied[stream]);

	if (etdm_data->force_on[stream]) {
		bool force_apply_out_change =
			!etdm_data->force_on_status[SNDRV_PCM_STREAM_PLAYBACK]
			&&
			(etdm_data->force_on_policy[SNDRV_PCM_STREAM_PLAYBACK]
			 == MT8512_ETDM_FORCE_ON_1ST_TRIGGER);
		bool force_apply_in_change =
			!etdm_data->force_on_status[SNDRV_PCM_STREAM_CAPTURE]
			&&
			(etdm_data->force_on_policy[SNDRV_PCM_STREAM_CAPTURE]
			 == MT8512_ETDM_FORCE_ON_1ST_TRIGGER);

		if (force_apply_out_change || force_apply_in_change) {
			if (clk_mode == MT8512_ETDM_SHARED_CLOCK) {
				force_apply_out_change = true;
				force_apply_in_change = true;
			}

			mt8512_afe_enable_main_clk(afe);

			if (force_apply_out_change)
				mt8512_afe_etdm2_out_force_on(afe);

			if (force_apply_in_change)
				mt8512_afe_etdm2_in_force_on(afe);
		}

		return 0;
	}

	mt8512_afe_enable_main_clk(afe);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK ||
	    clk_mode == MT8512_ETDM_SHARED_CLOCK) {
		mt8512_afe_enable_clk(afe,
			afe_priv->clocks[MT8512_CLK_I2SOUT_MCK]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_I2S_OUT);
	}

	if (stream == SNDRV_PCM_STREAM_CAPTURE ||
	    clk_mode == MT8512_ETDM_SHARED_CLOCK) {
		mt8512_afe_enable_clk(afe,
			afe_priv->clocks[MT8512_CLK_I2SIN_MCK]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_I2S_IN);
	}

	return 0;
}

static void mt8512_afe_etdm2_shutdown(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8512_AFE_BACKEND_BASE];
	unsigned int etdm_set = MT8512_ETDM2;
	struct mt8512_etdm_data *etdm_data = &afe_priv->etdm_data[etdm_set];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;
	bool reset_out_change = (stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	bool reset_in_change = (stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	bool need_tuner = false;
	bool slave_mode = etdm_data->slave_mode[stream];
	unsigned int rate = substream->runtime->rate;

	dev_info(dai->dev, "%s stream %u clk_mode %u occupied %d\n",
		__func__, stream, clk_mode, etdm_data->occupied[stream]);

	if (reset_out_change) {
		if ((etdm_data->occupied[stream] == 1)
			&& !etdm_data->force_on[stream]) {
			need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
				&& !mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_PLAYBACK))
				|| (!mt8512_afe_is_int_1x_en_low_power(afe)
				&& mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_PLAYBACK));

			if (need_tuner) {
				if (rate % 8000) {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL1);
				} else {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL2);
				}
			}
		}
		mt8512_afe_dec_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_PLAYBACK);
	}

	if (reset_in_change) {
		if ((etdm_data->occupied[stream] == 1)
			&& !etdm_data->force_on[stream]) {
			need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
				&& !mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_CAPTURE))
				|| (!mt8512_afe_is_int_1x_en_low_power(afe)
				&& mt8512_afe_is_etdm_low_power(afe, etdm_set,
					SNDRV_PCM_STREAM_CAPTURE));

			if (need_tuner) {
				if (rate % 8000) {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL1);
				} else {
					mt8512_afe_disable_apll_associated_cfg(
						afe, MT8512_AFE_APLL2);
				}
			}
		}
		mt8512_afe_dec_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);
	}

	if (etdm_data->force_on[stream] && !slave_mode)
		return;

	if (etdm_data->occupied[stream] == 0) {
		if (reset_out_change)
			mt8512_afe_disable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_PLAYBACK);

		if (reset_in_change)
			mt8512_afe_disable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE);
	}

	if (etdm_data->force_on[stream] && slave_mode)
		return;

	if (reset_out_change) {
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_I2S_OUT);
		mt8512_afe_disable_clk(afe,
			afe_priv->clocks[MT8512_CLK_I2SOUT_MCK]);
	}

	if (reset_in_change) {
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_I2S_IN);
		mt8512_afe_disable_clk(afe,
			afe_priv->clocks[MT8512_CLK_I2SIN_MCK]);
	}
	if (be->prepared[stream]) {
		be->prepared[stream] = false;
		if (rate % 8000)
			mt8512_afe_disable_clk(afe,
					afe_priv->clocks[MT8512_CLK_FA2SYS]);
	}
	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_etdm2_prepare(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_be_dai_data *be =
		&afe_priv->be_data[dai->id - MT8512_AFE_BACKEND_BASE];
	unsigned int etdm_set = MT8512_ETDM2;
	struct mt8512_etdm_data *etdm_data =
		&afe_priv->etdm_data[etdm_set];
	unsigned int clk_mode = etdm_data->clock_mode;
	unsigned int stream = substream->stream;
	bool apply_out_change = (stream == SNDRV_PCM_STREAM_PLAYBACK) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	bool apply_in_change = (stream == SNDRV_PCM_STREAM_CAPTURE) ||
		(clk_mode == MT8512_ETDM_SHARED_CLOCK);
	bool need_tuner = false;
	bool slave_mode = etdm_data->slave_mode[stream];
	unsigned int rate = dai->rate;
	unsigned int mclk = afe_priv->etdm_data[etdm_set].mclk_freq[stream];
	int ret;

	dev_info(dai->dev, "%s stream %u clk_mode %u occupied %d\n",
		__func__, stream, clk_mode, etdm_data->occupied[stream]);

	if (etdm_data->occupied[stream] || be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared %d, occupied %d\n",
			__func__, snd_pcm_stream_str(substream),
			be->prepared[stream], etdm_data->occupied[stream]);
		return 0;
	}

	if (apply_out_change)
		mt8512_afe_inc_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_PLAYBACK);

	if (apply_in_change)
		mt8512_afe_inc_etdm_occupy(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);

	if (etdm_data->force_on[stream]) {
		if (apply_out_change && slave_mode)
			mt8512_afe_enable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (apply_in_change && slave_mode)
			mt8512_afe_enable_etdm(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE);
		return 0;
	}

	if (apply_out_change) {
		ret = mt8512_afe_prepare_etdm_out(afe, substream, dai,
						  etdm_set);
		if (ret)
			return ret;
	}

	if (apply_in_change) {
		ret = mt8512_afe_prepare_etdm_in(afe, substream, dai,
						 etdm_set);
		if (ret)
			return ret;
	}

	if (rate % 8000)
		mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FA2SYS]);

	if (apply_out_change) {
		if (mclk == 0)
			mclk = MT8512_ETDM2_OUT_MCLK_MULTIPLIER * rate;

		need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
			  && !mt8512_afe_is_etdm_low_power(afe,
				etdm_set, SNDRV_PCM_STREAM_PLAYBACK))
			  || (!mt8512_afe_is_int_1x_en_low_power(afe)
			    && mt8512_afe_is_etdm_low_power(afe,
				etdm_set, SNDRV_PCM_STREAM_PLAYBACK));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL1);
			} else {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8512_CLK_I2SOUT_M_SEL],
			(rate % 8000) ?
			afe_priv->clocks[MT8512_CLK_AUD1] :
			afe_priv->clocks[MT8512_CLK_AUD2]);
		mt8512_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8512_CLK_APLL12_DIV2],
			mclk);
		mt8512_afe_enable_etdm(afe, etdm_set,
			SNDRV_PCM_STREAM_PLAYBACK);
	}

	if (apply_in_change) {
		if (mclk == 0)
			mclk = MT8512_ETDM2_IN_MCLK_MULTIPLIER * rate;

		need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
			  && !mt8512_afe_is_etdm_low_power(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE))
			 || (!mt8512_afe_is_int_1x_en_low_power(afe)
			   && mt8512_afe_is_etdm_low_power(afe, etdm_set,
				SNDRV_PCM_STREAM_CAPTURE));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL1);
			} else {
				mt8512_afe_enable_apll_associated_cfg(afe,
					MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8512_CLK_I2SIN_M_SEL],
			(rate % 8000) ?
			afe_priv->clocks[MT8512_CLK_AUD1] :
			afe_priv->clocks[MT8512_CLK_AUD2]);
		mt8512_afe_set_clk_rate(afe,
			afe_priv->clocks[MT8512_CLK_APLL12_DIV0],
			mclk);
		mt8512_afe_enable_etdm(afe, etdm_set,
			SNDRV_PCM_STREAM_CAPTURE);
	}
	be->prepared[stream] = true;

	return 0;
}

/* be_clients will get connected after startup callback */
static bool mt8512_match_1st_be_cpu_dai(struct snd_pcm_substream *substream,
	char *dai_name)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	int stream = substream->stream;
	struct snd_soc_dpcm *dpcm;
	int i = 0;

	list_for_each_entry(dpcm, &fe->dpcm[stream].be_clients, list_be) {
		struct snd_soc_pcm_runtime *be = dpcm->be;

		if (i > 0)
			break;

		if (!strcmp(be->cpu_dai->name, dai_name))
			return true;
		i++;
	}

	return false;
}

static int mt8512_memif_fs(struct snd_pcm_substream *substream,
	unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int dai_id = rtd->cpu_dai->id;
	struct mt8512_fe_dai_data *fe_data = &afe_priv->fe_data[dai_id];
	int fs;

	if (dai_id == MT8512_AFE_MEMIF_UL8)
		return MT8512_FS_ETDMIN1_NX_EN;

	if (dai_id == MT8512_AFE_MEMIF_UL3 && is_ul3_in_direct_mode(afe))
		return MT8512_FS_ETDMIN2_NX_EN;

	fs = mt8512_afe_fs_timing(rate);

	if (!fe_data->slave_mode)
		return fs;

	// slave mode
	switch (dai_id) {
	case MT8512_AFE_MEMIF_UL2:
	case MT8512_AFE_MEMIF_UL3:
	case MT8512_AFE_MEMIF_UL4:
	case MT8512_AFE_MEMIF_UL9:
	case MT8512_AFE_MEMIF_UL10:
		if (mt8512_match_1st_be_cpu_dai(substream, "ETDM1_IN"))
			fs = MT8512_FS_ETDMIN1_1X_EN;
		else if (mt8512_match_1st_be_cpu_dai(substream, "ETDM2_IN"))
			fs = MT8512_FS_ETDMIN2_1X_EN;
		break;
	case MT8512_AFE_MEMIF_DLM:
	case MT8512_AFE_MEMIF_DL2:
	case MT8512_AFE_MEMIF_DL3:
		if (mt8512_match_1st_be_cpu_dai(substream, "ETDM2_OUT"))
			fs = MT8512_FS_ETDMOUT2_1X_EN;
		break;
	default:
		break;
	}

	return fs;
}

static int mt8512_irq_fs(struct snd_pcm_substream *substream,
	unsigned int rate)
{
	int irq_fs = mt8512_memif_fs(substream, rate);

	if (irq_fs == MT8512_FS_ETDMIN1_NX_EN)
		irq_fs = MT8512_FS_ETDMIN1_1X_EN;
	else if (irq_fs == MT8512_FS_ETDMIN2_NX_EN)
		irq_fs = MT8512_FS_ETDMIN2_1X_EN;

	return irq_fs;
}

static int mt8512_alloc_dmabuf(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int dai_id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[dai_id];
	const struct mtk_base_memif_data *data = memif->data;
	struct mt8512_fe_dai_data *fe_data = &afe_priv->fe_data[dai_id];
	const size_t request_size = params_buffer_bytes(params);
	int ret;

	if (request_size > fe_data->sram_size) {
		ret = snd_pcm_lib_malloc_pages(substream, request_size);
		if (ret < 0) {
			dev_info(afe->dev,
				"%s %s malloc pages %zu bytes failed %d\n",
				__func__, data->name, request_size, ret);
			return ret;
		}

		fe_data->use_sram = false;

		mt8512_afe_block_dpidle(afe);
	} else {
		struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->area = (unsigned char *)fe_data->sram_vir_addr;
		dma_buf->addr = fe_data->sram_phy_addr;
		dma_buf->bytes = request_size;
		snd_pcm_set_runtime_buffer(substream, dma_buf);

		fe_data->use_sram = true;
	}

	return 0;
}

static int mt8512_free_dmabuf(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int dai_id = rtd->cpu_dai->id;
	struct mt8512_fe_dai_data *fe_data = &afe_priv->fe_data[dai_id];
	int ret = 0;

	if (fe_data->use_sram) {
		snd_pcm_set_runtime_buffer(substream, NULL);
	} else {
		ret = snd_pcm_lib_free_pages(substream);

		mt8512_afe_unblock_dpidle(afe);
	}

	return ret;
}

int mt8512_afe_fe_startup(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int memif_num = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	dev_dbg(dai->dev, "%s %d\n", __func__, dai->id);

	memif->substream = substream;

	if (memif->data->buffer_bytes_align > 0)
		snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
				   memif->data->buffer_bytes_align);
	else
		snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	mt8512_afe_enable_main_clk(afe);

	/* enable agent */
	regmap_update_bits(afe->regmap, memif->data->agent_disable_reg,
			   1 << memif->data->agent_disable_shift,
			   0 << memif->data->agent_disable_shift);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	mt8512_afe_irq_direction_enable(afe,
		memif->irq_usage,
		MT8512_AFE_IRQ_DIR_MCU);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_info(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	return ret;
}

static void mt8512_afe_fe_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s %d\n", __func__, dai->id);

	mtk_afe_fe_shutdown(substream, dai);

	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_fe_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int dai_id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[dai_id];
	const struct mtk_base_memif_data *data = memif->data;
	struct mt8512_control_data *ctrl_data = &afe_priv->ctrl_data;
	unsigned int channels = params_channels(params);

	dev_dbg(afe->dev, "%s [%s] rate %u ch %u bit %u period %u-%u\n",
		__func__, data->name, params_rate(params), channels,
		params_width(params), params_period_size(params),
		params_periods(params));

	if (data->ch_config_shift >= 0)
		regmap_update_bits(afe->regmap,
			data->ch_config_reg,
			0x1f << data->ch_config_shift,
			channels << data->ch_config_shift);

	if (data->int_odd_shift >= 0) {
		unsigned int odd_en = (channels == 1) ? 1 : 0;

		regmap_update_bits(afe->regmap, data->int_odd_reg,
				   1 << data->int_odd_shift,
				   odd_en << data->int_odd_shift);
	}

	if (dai_id == MT8512_AFE_MEMIF_UL2) {
		unsigned int val = 0;

		if (!ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_O34_O41_1X_EN_MASK,
				   ASYS_TCON_O34_O41_1X_EN_UL2);

			val |= UL_REORDER_START_DATA(8) |
			       UL_REORDER_CHANNEL(channels) |
			       UL_REORDER_NO_BYPASS;
		}

		regmap_update_bits(afe->regmap, AFE_I2S_UL2_REORDER,
				   UL_REORDER_CTRL_MASK, val);
	} else if (dai_id == MT8512_AFE_MEMIF_UL9) {
		unsigned int val = 0;

		if (!ctrl_data->bypass_cm0) {
			regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_O26_O33_1X_EN_MASK,
				   ASYS_TCON_O26_O33_1X_EN_UL9);

			if (channels > 8)
				regmap_update_bits(afe->regmap, ASYS_TOP_CON,
					   ASYS_TCON_O34_O41_1X_EN_MASK,
					   ASYS_TCON_O34_O41_1X_EN_UL9);

			val |= UL_REORDER_START_DATA(0) |
			       UL_REORDER_CHANNEL(channels) |
			       UL_REORDER_NO_BYPASS;
		}

		regmap_update_bits(afe->regmap, AFE_I2S_UL9_REORDER,
				   UL_REORDER_CTRL_MASK, val);
	}

	return mtk_afe_fe_hw_params(substream, params, dai);
}

static int mt8512_afe_fe_prepare(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s %d\n", __func__, dai->id);

	return mtk_afe_fe_prepare(substream, dai);
}

int mt8512_afe_fe_trigger(struct snd_pcm_substream *substream, int cmd,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	const int dai_id = rtd->cpu_dai->id;

	dev_info(dai->dev, "%s %d cmd %d\n", __func__, dai->id, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* enable channel merge */
		if (dai_id == MT8512_AFE_MEMIF_UL2) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL2_REORDER,
				UL_REORDER_EN, UL_REORDER_EN);
		} else if (dai_id == MT8512_AFE_MEMIF_UL9) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL9_REORDER,
				UL_REORDER_EN, UL_REORDER_EN);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable channel merge */
		if (dai_id == MT8512_AFE_MEMIF_UL2) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL2_REORDER,
				UL_REORDER_EN, 0x0);
		} else if (dai_id == MT8512_AFE_MEMIF_UL9) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL9_REORDER,
				UL_REORDER_EN, 0x0);
		}
		break;
	default:
		break;
	}

	return mtk_afe_fe_trigger(substream, cmd, dai);
}

static int mt8512_afe_fe_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_fe_dai_data *fe_data = &afe_priv->fe_data[dai->id];

	dev_dbg(dai->dev, "%s fmt 0x%x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		fe_data->slave_mode = true;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		fe_data->slave_mode = false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8512_afe_etdm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int etdm_set = (dai->id == MT8512_AFE_IO_ETDM1_IN) ?
				 MT8512_ETDM1 : MT8512_ETDM2;
	unsigned int stream = (dai->id == MT8512_AFE_IO_ETDM2_OUT) ?
			       SNDRV_PCM_STREAM_PLAYBACK :
			       SNDRV_PCM_STREAM_CAPTURE;

	if (afe_priv->etdm_data[etdm_set].force_on[stream])
		return 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		afe_priv->etdm_data[etdm_set].format[stream] =
			MT8512_ETDM_FORMAT_I2S;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		afe_priv->etdm_data[etdm_set].format[stream] =
			MT8512_ETDM_FORMAT_DSPA;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		afe_priv->etdm_data[etdm_set].format[stream] =
			MT8512_ETDM_FORMAT_DSPB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		afe_priv->etdm_data[etdm_set].format[stream] =
			MT8512_ETDM_FORMAT_LJ;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		afe_priv->etdm_data[etdm_set].format[stream] =
			MT8512_ETDM_FORMAT_RJ;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		afe_priv->etdm_data[etdm_set].bck_inv[stream] = false;
		afe_priv->etdm_data[etdm_set].lrck_inv[stream] = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		afe_priv->etdm_data[etdm_set].bck_inv[stream] = false;
		afe_priv->etdm_data[etdm_set].lrck_inv[stream] = true;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		afe_priv->etdm_data[etdm_set].bck_inv[stream] = true;
		afe_priv->etdm_data[etdm_set].lrck_inv[stream] = false;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		afe_priv->etdm_data[etdm_set].bck_inv[stream] = true;
		afe_priv->etdm_data[etdm_set].lrck_inv[stream] = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		afe_priv->etdm_data[etdm_set].slave_mode[stream] = true;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		afe_priv->etdm_data[etdm_set].slave_mode[stream] = false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8512_afe_etdm_set_tdm_slot(struct snd_soc_dai *dai,
					unsigned int tx_mask,
					unsigned int rx_mask,
					int slots,
					int slot_width)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int etdm_set = (dai->id == MT8512_AFE_IO_ETDM1_IN) ?
				 MT8512_ETDM1 : MT8512_ETDM2;
	unsigned int stream = (dai->id == MT8512_AFE_IO_ETDM2_OUT) ?
			       SNDRV_PCM_STREAM_PLAYBACK :
			       SNDRV_PCM_STREAM_CAPTURE;

	dev_dbg(dai->dev, "%s %d etdm %u stream %u slot_width %d\n",
		__func__, dai->id, etdm_set, stream, slot_width);

	if (afe_priv->etdm_data[etdm_set].force_on[stream])
		return 0;

	afe_priv->etdm_data[etdm_set].lrck_width[stream] = slot_width;

	return 0;
}

static int mt8512_afe_etdm_set_sysclk(struct snd_soc_dai *dai,
				      int clk_id,
				      unsigned int freq,
				      int dir)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int etdm_set = (dai->id == MT8512_AFE_IO_ETDM1_IN) ?
				 MT8512_ETDM1 : MT8512_ETDM2;
	unsigned int stream = (dai->id == MT8512_AFE_IO_ETDM2_OUT) ?
			       SNDRV_PCM_STREAM_PLAYBACK :
			       SNDRV_PCM_STREAM_CAPTURE;

	dev_dbg(dai->dev, "%s %d etdm %u stream %u freq %u\n",
		__func__, dai->id, etdm_set, stream, freq);

	if (afe_priv->etdm_data[etdm_set].force_on[stream])
		return 0;

	afe_priv->etdm_data[etdm_set].mclk_freq[stream] = freq;

	return 0;
}

static void mt8512_afe_enable_pcm1(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_EN, PCM_INTF_CON1_EN);
}

static void mt8512_afe_disable_pcm1(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_EN, 0x0);

	regmap_update_bits(afe->regmap, AFE_PCM_SRC_FS_CON0,
		PCM_TX_RX_SYNC_ENABLE_MASK, PCM_TX_RX_SYNC_DISABLE);
}

static int mt8512_afe_configure_pcm1(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	bool slave_mode = afe_priv->pcm_intf_data.slave_mode;
	bool lrck_inv = afe_priv->pcm_intf_data.lrck_inv;
	bool bck_inv = afe_priv->pcm_intf_data.bck_inv;
	unsigned int fmt = afe_priv->pcm_intf_data.format;
	unsigned int bit_width = dai->sample_bits;
	unsigned int val = 0;
	int fs;

	regmap_update_bits(afe->regmap, AFE_PCM_SRC_FS_CON0,
			PCM_TX_RX_SYNC_ENABLE_MASK, PCM_TX_RX_SYNC_ENABLE);

	if (!slave_mode) {
		val |= PCM_INTF_CON1_MASTER_MODE |
		       PCM_INTF_CON1_BYPASS_ASRC;

		if (lrck_inv)
			val |= PCM_INTF_CON1_SYNC_OUT_INV;
		if (bck_inv)
			val |= PCM_INTF_CON1_BCLK_OUT_INV;

		fs = mt8512_afe_fs_timing(runtime->rate);
		regmap_update_bits(afe->regmap, AFE_PCM_SRC_FS_CON0,
			PCM_TX_RX_SYNC_SAMPLE_RATE_MASK, PCM_TX_RX_SYNC_FS(fs));
	} else {
		val |= PCM_INTF_CON1_SLAVE_MODE;

		if (lrck_inv)
			val |= PCM_INTF_CON1_SYNC_IN_INV;
		if (bck_inv)
			val |= PCM_INTF_CON1_BCLK_IN_INV;

		/* slave mode and not bypass asrc */
		regmap_update_bits(afe->regmap, AFE_PCM_SRC_FS_CON0,
				PCM_TX_RX_SYNC_SAMPLE_RATE_MASK, EXT_PCM_1X_EN);
		// TODO: add asrc setting
	}

	val |= PCM_INTF_CON1_FORMAT(fmt);

	if (fmt == MT8512_PCM_FORMAT_PCMA ||
	    fmt == MT8512_PCM_FORMAT_PCMB)
		val |= PCM_INTF_CON1_SYNC_LEN(1);
	else
		val |= PCM_INTF_CON1_SYNC_LEN(bit_width);

	switch (runtime->rate) {
	case 48000:
		val |= PCM_INTF_CON1_FS_48K;
		break;
	case 32000:
		val |= PCM_INTF_CON1_FS_32K;
		break;
	case 16000:
		val |= PCM_INTF_CON1_FS_16K;
		break;
	case 8000:
		val |= PCM_INTF_CON1_FS_8K;
		break;
	default:
		return -EINVAL;
	}

	if (bit_width > 16)
		val |= PCM_INTF_CON1_24BIT | PCM_INTF_CON1_64BCK;
	else
		val |= PCM_INTF_CON1_16BIT | PCM_INTF_CON1_32BCK;

	regmap_update_bits(afe->regmap, PCM_INTF_CON1,
			   PCM_INTF_CON1_CONFIG_MASK, val);

	return 0;
}

static int mt8512_afe_pcm1_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s active %u\n", __func__, dai->active);

	if (dai->active)
		return 0;

	mt8512_afe_enable_main_clk(afe);

	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_PCMIF);

	return 0;
}

static void mt8512_afe_pcm1_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s active %u\n", __func__, dai->active);

	if (dai->active)
		return;

	mt8512_afe_disable_pcm1(afe);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_PCMIF);
}

static int mt8512_afe_pcm1_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	dev_dbg(dai->dev, "%s\n", __func__);

	if ((dai->playback_active + dai->capture_active) > 1) {
		dev_info(afe->dev, "%s '%s' active(%u-%u) already\n",
			 __func__, snd_pcm_stream_str(substream),
			 dai->playback_active,
			 dai->capture_active);
		return 0;
	}

	ret = mt8512_afe_configure_pcm1(substream, dai);
	if (ret)
		return ret;

	mt8512_afe_enable_pcm1(afe);

	return 0;
}

static int mt8512_afe_pcm1_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dai->dev, "%s fmt 0x%x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		afe_priv->pcm_intf_data.format = MT8512_PCM_FORMAT_I2S;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		afe_priv->pcm_intf_data.bck_inv = false;
		afe_priv->pcm_intf_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		afe_priv->pcm_intf_data.bck_inv = false;
		afe_priv->pcm_intf_data.lrck_inv = true;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		afe_priv->pcm_intf_data.bck_inv = true;
		afe_priv->pcm_intf_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		afe_priv->pcm_intf_data.bck_inv = true;
		afe_priv->pcm_intf_data.lrck_inv = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		afe_priv->pcm_intf_data.slave_mode = true;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		afe_priv->pcm_intf_data.slave_mode = false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void mt8512_afe_enable_dmic(struct mtk_base_afe *afe,
					struct snd_soc_dai *dai)
{
	unsigned int channels = dai->channels;
	unsigned int val = 0;
	unsigned int msk = 0;

	msk |= DMIC_UL_CON0_MODE_3P25M_CH1_CTL;
	msk |= DMIC_UL_CON0_MODE_3P25M_CH2_CTL;
	msk |= DMIC_UL_CON0_SRC_ON_TMP_CTL;
	val = msk;

	if (channels > 6)
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			msk, val);

	if (channels > 4)
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			msk, val);

	if (channels > 2)
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			msk, val);

	if (channels > 0) {
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			msk, val);
	}

#if 0
	/*DE said 8512 dmic no need set this,8168 need */
	/* adda afe on */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
					ADDA_UL_DL_CON0_ADDA_INTF_ON,
					ADDA_UL_DL_CON0_ADDA_INTF_ON);

	/* dmic clkdiv on */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
					ADDA_UL_DL_CON0_DMIC_CLKDIV_ON,
					ADDA_UL_DL_CON0_DMIC_CLKDIV_ON);
#endif

	/* dmic ck div on */
	regmap_update_bits(afe->regmap, PWR2_TOP_CON1,
					PWR2_TOP_CON1_DMIC_PDM_INTF_ON,
					PWR2_TOP_CON1_DMIC_PDM_INTF_ON);
}

static void mt8512_afe_disable_dmic(struct mtk_base_afe *afe)
{
	unsigned int msk = 0;

	msk |= DMIC_UL_CON0_MODE_3P25M_CH1_CTL;
	msk |= DMIC_UL_CON0_MODE_3P25M_CH2_CTL;
	msk |= DMIC_UL_CON0_SRC_ON_TMP_CTL;

	regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
		msk, 0x0);
	regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
		msk, 0x0);
	regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
		msk, 0x0);
	regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
		msk, 0x0);

#if 0
	/*DE said 8512 dmic no need set this,8168 need */
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
			ADDA_UL_DL_CON0_ADDA_INTF_ON, 0x0);

	regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0,
			ADDA_UL_DL_CON0_DMIC_CLKDIV_ON, 0x0);
#endif

	regmap_update_bits(afe->regmap, PWR2_TOP_CON1,
		PWR2_TOP_CON1_DMIC_PDM_INTF_ON, 0x0);
}

static const struct reg_sequence mt8512_afe_dmic_iir_coeff_reg_defaults[] = {
	{ AFE_DMIC0_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC0_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC0_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC1_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC1_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC1_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC2_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC2_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC2_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC3_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC3_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC3_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_10_09, 0x0000C048 },
};

static int mt8512_afe_load_dmic_iir_coeff_table(struct mtk_base_afe *afe)
{
	return regmap_multi_reg_write(afe->regmap,
			mt8512_afe_dmic_iir_coeff_reg_defaults,
			ARRAY_SIZE(mt8512_afe_dmic_iir_coeff_reg_defaults));
}

static int mt8512_afe_configure_dmic_array(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_dmic_data *dmic_data = &afe_priv->dmic_data;
	unsigned int *dmic_src_sel = dmic_data->dmic_src_sel;
	unsigned int mask =
			PWR2_TOP_CON_DMIC8_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC7_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC6_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC5_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC4_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC3_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC2_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC1_SRC_SEL_MASK;
	unsigned int val =
			PWR2_TOP_CON_DMIC8_SRC_SEL_VAL(dmic_src_sel[7]) |
			PWR2_TOP_CON_DMIC7_SRC_SEL_VAL(dmic_src_sel[6]) |
			PWR2_TOP_CON_DMIC6_SRC_SEL_VAL(dmic_src_sel[5]) |
			PWR2_TOP_CON_DMIC5_SRC_SEL_VAL(dmic_src_sel[4]) |
			PWR2_TOP_CON_DMIC4_SRC_SEL_VAL(dmic_src_sel[3]) |
			PWR2_TOP_CON_DMIC3_SRC_SEL_VAL(dmic_src_sel[2]) |
			PWR2_TOP_CON_DMIC2_SRC_SEL_VAL(dmic_src_sel[1]) |
			PWR2_TOP_CON_DMIC1_SRC_SEL_VAL(dmic_src_sel[0]);

	regmap_update_bits(afe->regmap, PWR2_TOP_CON0, mask, val);

	return 0;
}

static int mt8512_afe_configure_dmic(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_dmic_data *dmic_data = &afe_priv->dmic_data;
	unsigned int rate = dai->rate;
	unsigned int channels = dai->channels;
	bool two_wire_mode = dmic_data->two_wire_mode;
	unsigned int clk_phase_sel_ch1 = dmic_data->clk_phase_sel_ch1;
	unsigned int clk_phase_sel_ch2 = dmic_data->clk_phase_sel_ch2;
	bool iir_on = dmic_data->iir_on;
	unsigned int dmic_ul_mode = dmic_data->ul_mode;
	unsigned int val = 0;

	val |= DMIC_UL_CON0_SDM_3_LEVEL_CTL;

	if (two_wire_mode) {
		val |= DMIC_UL_CON0_TWO_WIRE_MODE_CTL;
		val |= DMIC_UL_CON0_PHASE_SEL_CH1(clk_phase_sel_ch1);
		val |= DMIC_UL_CON0_PHASE_SEL_CH2(clk_phase_sel_ch2);
	} else {
		val |= DMIC_UL_CON0_PHASE_SEL_CH1(clk_phase_sel_ch1);
		val |= DMIC_UL_CON0_PHASE_SEL_CH2((clk_phase_sel_ch1 + 4)
						  & 0x7);
	}

	mt8512_afe_configure_dmic_array(afe);

	switch (rate) {
	case 48000: /* 3.072M */
		val |= DMIC_UL_CON0_VOCIE_MODE_48K;
		break;
	case 32000: /* 2.048M */
		val |= DMIC_UL_CON0_VOCIE_MODE_32K;
		break;
	case 16000: /* 2.048M */
		val |= DMIC_UL_CON0_VOCIE_MODE_16K;
		break;
	case 8000: /* 2.048M */
		val |= DMIC_UL_CON0_VOCIE_MODE_8K;
		break;
	default:
		return -EINVAL;
	}

	val |= DMIC_UL_CON0_3P25M_1P625M_SEL(dmic_ul_mode & 0x1);
	val |= DMIC_UL_CON0_LOW_POWER_MODE_SEL(dmic_ul_mode);

	if (iir_on) {
		mt8512_afe_load_dmic_iir_coeff_table(afe);
		val |= DMIC_UL_CON0_IIR_MODE_SEL(0); /* SW mode */
		val |= DMIC_UL_CON0_IIR_ON_TMP_CTL;
	}

	if (channels > 6)
		regmap_update_bits(afe->regmap, AFE_DMIC3_UL_SRC_CON0,
			DMIC_UL_CON0_CONFIG_MASK, val);

	if (channels > 4)
		regmap_update_bits(afe->regmap, AFE_DMIC2_UL_SRC_CON0,
			DMIC_UL_CON0_CONFIG_MASK, val);

	if (channels > 2)
		regmap_update_bits(afe->regmap, AFE_DMIC1_UL_SRC_CON0,
			DMIC_UL_CON0_CONFIG_MASK, val);

	if (channels > 0) {
		regmap_update_bits(afe->regmap, AFE_DMIC0_UL_SRC_CON0,
			DMIC_UL_CON0_CONFIG_MASK, val);
	}

	return 0;
}

static int mt8512_afe_dmic_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_enable_main_clk(afe);

	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DMIC0);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DMIC1);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DMIC2);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DMIC3);

	return 0;
}

static void mt8512_afe_dmic_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_disable_dmic(afe);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DMIC3);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DMIC2);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DMIC1);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DMIC0);

	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_dmic_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_configure_dmic(afe, substream, dai);
	mt8512_afe_enable_dmic(afe, dai);

	return 0;
}

static int mt8512_afe_int_adda_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int stream = substream->stream;

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_enable_main_clk(afe);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DAC);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DAC_PREDIS);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DL_ASRC);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_ADC);
	}

	return 0;
}

static void mt8512_afe_int_adda_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	unsigned int stream = substream->stream;
	unsigned int rate = substream->runtime->rate;

	dev_dbg(dai->dev, "%s, rate:%u\n", __func__, rate);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DL_ASRC);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DAC_PREDIS);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DAC);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_ADC);
	}

	if (!mt8512_afe_is_int_1x_en_low_power(afe)) {
		if (rate % 8000)
			mt8512_afe_disable_apll_associated_cfg(afe,
				MT8512_AFE_APLL1);
		else
			mt8512_afe_disable_apll_associated_cfg(afe,
				MT8512_AFE_APLL2);
	}

	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_int_adda_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int stream = substream->stream;
	unsigned int rate = dai->rate;

	dev_info(dai->dev, "%s stream %d, rate = %d\n", __func__, stream, rate);

	if (!mt8512_afe_is_int_1x_en_low_power(afe)) {
		if (rate % 8000)
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL1);
		else
			mt8512_afe_enable_apll_associated_cfg(afe,
				MT8512_AFE_APLL2);
	}

	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	regmap_update_bits(afe->regmap, ASMO_TIMING_CON0,
		ASMO_TIMING_CON0_ASMO0_MODE_MASK,
		ASMO_TIMING_CON0_ASMO0_MODE_VAL(mt8512_afe_fs_timing(rate)));

	return 0;
}

static const struct mt8512_gasrc_ctrl_reg gasrc_ctrl_reg[MT8512_GASRC_NUM] = {
	[MT8512_GASRC0] = {
		.con0 = AFE_GASRC0_NEW_CON0,
		.con1 = AFE_GASRC0_NEW_CON1,
		.con2 = AFE_GASRC0_NEW_CON2,
		.con3 = AFE_GASRC0_NEW_CON3,
		.con4 = AFE_GASRC0_NEW_CON4,
		.con6 = AFE_GASRC0_NEW_CON6,
		.con7 = AFE_GASRC0_NEW_CON7,
		.con10 = AFE_GASRC0_NEW_CON10,
		.con11 = AFE_GASRC0_NEW_CON11,
		.con13 = AFE_GASRC0_NEW_CON13,
		.con14 = AFE_GASRC0_NEW_CON14,
	},
	[MT8512_GASRC1] = {
		.con0 = AFE_GASRC1_NEW_CON0,
		.con1 = AFE_GASRC1_NEW_CON1,
		.con2 = AFE_GASRC1_NEW_CON2,
		.con3 = AFE_GASRC1_NEW_CON3,
		.con4 = AFE_GASRC1_NEW_CON4,
		.con6 = AFE_GASRC1_NEW_CON6,
		.con7 = AFE_GASRC1_NEW_CON7,
		.con10 = AFE_GASRC1_NEW_CON10,
		.con11 = AFE_GASRC1_NEW_CON11,
		.con13 = AFE_GASRC1_NEW_CON13,
		.con14 = AFE_GASRC1_NEW_CON14,
	},
	[MT8512_GASRC2] = {
		.con0 = AFE_GASRC2_NEW_CON0,
		.con1 = AFE_GASRC2_NEW_CON1,
		.con2 = AFE_GASRC2_NEW_CON2,
		.con3 = AFE_GASRC2_NEW_CON3,
		.con4 = AFE_GASRC2_NEW_CON4,
		.con6 = AFE_GASRC2_NEW_CON6,
		.con7 = AFE_GASRC2_NEW_CON7,
		.con10 = AFE_GASRC2_NEW_CON10,
		.con11 = AFE_GASRC2_NEW_CON11,
		.con13 = AFE_GASRC2_NEW_CON13,
		.con14 = AFE_GASRC2_NEW_CON14,
	},
	[MT8512_GASRC3] = {
		.con0 = AFE_GASRC3_NEW_CON0,
		.con1 = AFE_GASRC3_NEW_CON1,
		.con2 = AFE_GASRC3_NEW_CON2,
		.con3 = AFE_GASRC3_NEW_CON3,
		.con4 = AFE_GASRC3_NEW_CON4,
		.con6 = AFE_GASRC3_NEW_CON6,
		.con7 = AFE_GASRC3_NEW_CON7,
		.con10 = AFE_GASRC3_NEW_CON10,
		.con11 = AFE_GASRC3_NEW_CON11,
		.con13 = AFE_GASRC3_NEW_CON13,
		.con14 = AFE_GASRC3_NEW_CON14,
	},
};

static const struct mt8512_afe_gasrc_mux_map gasrc_mux_maps[] = {
	{ MT8512_GASRC0, 0, MUX_GASRC_8CH, },
	{ MT8512_GASRC0, 1, MUX_GASRC_6CH, },
	{ MT8512_GASRC0, 2, MUX_GASRC_4CH, },
	{ MT8512_GASRC0, 3, MUX_GASRC_2CH, },
	{ MT8512_GASRC1, 0, MUX_GASRC_8CH, },
	{ MT8512_GASRC1, 1, MUX_GASRC_6CH, },
	{ MT8512_GASRC1, 2, MUX_GASRC_4CH, },
	{ MT8512_GASRC1, 3, MUX_GASRC_2CH, },
	{ MT8512_GASRC2, 0, MUX_GASRC_8CH, },
	{ MT8512_GASRC2, 1, MUX_GASRC_6CH, },
	{ MT8512_GASRC2, 2, MUX_GASRC_2CH, },
	{ MT8512_GASRC3, 0, MUX_GASRC_8CH, },
	{ MT8512_GASRC3, 1, MUX_GASRC_2CH, },
};

static int mt8512_afe_query_gasrc_mux(int gasrc_id, int idx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gasrc_mux_maps); i++)
		if ((gasrc_mux_maps[i].gasrc_id == gasrc_id) &&
			(gasrc_mux_maps[i].idx == idx))
			return gasrc_mux_maps[i].mux;

	return -EINVAL;
}

static int mt8512_afe_get_gasrc_mux_config(struct mtk_base_afe *afe,
	int gasrc_id, unsigned int stream)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int input_mux;
	int output_mux;

	if (gasrc_id < 0)
		return -EINVAL;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		input_mux = mt8512_afe_query_gasrc_mux(gasrc_id,
			afe_priv->gasrc_data[gasrc_id].input_mux);
		if (input_mux < 0)
			return -EINVAL;
		else
			return input_mux;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		output_mux = mt8512_afe_query_gasrc_mux(gasrc_id,
			afe_priv->gasrc_data[gasrc_id].output_mux);
		if (output_mux < 0)
			return -EINVAL;
		else
			return output_mux;
	}

	return -EINVAL;
}

static int mt8512_dai_num_to_gasrc(int num)
{
	int val = num - MT8512_AFE_IO_GASRC0;

	if (val < 0 || val >= MT8512_GASRC_NUM)
		return -EINVAL;

	return val;
}

static void mt8512_afe_reset_gasrc(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int val = 0;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		val = GASRC_CFG0_GASRC0_SOFT_RST;
		break;
	case MT8512_GASRC1:
		val = GASRC_CFG0_GASRC1_SOFT_RST;
		break;
	case MT8512_GASRC2:
		val = GASRC_CFG0_GASRC2_SOFT_RST;
		break;
	case MT8512_GASRC3:
		val = GASRC_CFG0_GASRC3_SOFT_RST;
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, GASRC_CFG0, val, val);
	regmap_update_bits(afe->regmap, GASRC_CFG0, val, 0);

}

static void mt8512_afe_clear_gasrc(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int ctrl_reg = 0;

	if (gasrc_id < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET_STR_CLR, GASRC_NEW_CON0_CHSET_STR_CLR);
}

static struct snd_soc_dai *mt8512_get_be_cpu_dai(
	struct snd_pcm_substream *be_substream,
	char *dai_name)
{
	struct snd_soc_dpcm *dpcm_be;
	struct snd_soc_dpcm *dpcm_fe;
	struct snd_soc_pcm_runtime *be = be_substream->private_data;
	struct snd_soc_pcm_runtime *fe;
	int stream = be_substream->stream;

	list_for_each_entry(dpcm_fe, &be->dpcm[stream].fe_clients, list_fe) {
		fe = dpcm_fe->fe;

		list_for_each_entry(dpcm_be,
			&fe->dpcm[stream].be_clients, list_be) {
			if (!strcmp(dpcm_be->be->cpu_dai->name, dai_name))
				return dpcm_be->be->cpu_dai;
		}
	}

	return NULL;
}

static bool mt8512_be_cpu_dai_matched(struct snd_soc_dapm_widget *w,
	int dai_id, int stream)
{
	struct snd_soc_dapm_path *path;
	struct snd_soc_dai *dai;
	bool ret = false;
	bool playback = (stream == SNDRV_PCM_STREAM_PLAYBACK) ? true : false;

	if (!w)
		return false;

	if (w->is_ep)
		return false;

	if (playback) {
		snd_soc_dapm_widget_for_each_source_path(w, path) {
			if (path && path->source) {
				if (!path->connect)
					continue;

				dai = path->source->priv;
				if (dai && dai_id == dai->id)
					return true;

				ret = mt8512_be_cpu_dai_matched(path->source,
						dai_id, stream);
				if (ret)
					return ret;
			}
		}
	} else {
		snd_soc_dapm_widget_for_each_sink_path(w, path) {
			if (path && path->sink) {
				if (!path->connect)
					continue;

				dai = path->sink->priv;
				if (dai && dai_id == dai->id)
					return true;

				ret = mt8512_be_cpu_dai_matched(path->sink,
						dai_id, stream);
				if (ret)
					return ret;
			}
		}
	}

	return false;
}

static bool mt8512_be_cpu_dai_connected(
	struct snd_pcm_substream *be_substream,
	char *dai_name, int stream, int dai_id)
{
	struct snd_soc_dai *dai =
		mt8512_get_be_cpu_dai(be_substream, dai_name);
	struct snd_soc_dapm_widget *w = NULL;

	if (dai == NULL)
		return false;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		w = dai->playback_widget;
	else if (stream == SNDRV_PCM_STREAM_CAPTURE)
		w = dai->capture_widget;

	return mt8512_be_cpu_dai_matched(w, dai_id, stream);
}

static bool mt8512_get_be_cpu_dai_rate(struct snd_pcm_substream *be_substream,
	char *dai_name, int stream, int dai_id, unsigned int *rate)
{
	struct snd_soc_dai *dai = mt8512_get_be_cpu_dai(be_substream, dai_name);
	bool ret = false;

	if (!rate)
		return ret;

	ret = mt8512_be_cpu_dai_connected(be_substream,
			dai_name, stream, dai_id);
	if (!ret)
		return false;

	*rate = dai->rate;

	return ret;
}

static int mt8512_afe_gasrc_get_input_rate(
	struct snd_pcm_substream *substream, int dai_id, unsigned int rate)
{
	unsigned int input_rate;

	if (mt8512_get_be_cpu_dai_rate(substream, "INT ADDA",
			SNDRV_PCM_STREAM_CAPTURE, dai_id, &input_rate))
		return input_rate;
	else if (mt8512_get_be_cpu_dai_rate(substream, "ETDM1_IN",
			SNDRV_PCM_STREAM_CAPTURE, dai_id, &input_rate))
		return input_rate;
	else if (mt8512_get_be_cpu_dai_rate(substream, "ETDM2_IN",
			SNDRV_PCM_STREAM_CAPTURE, dai_id, &input_rate))
		return input_rate;
	else if (mt8512_get_be_cpu_dai_rate(substream, "DMIC",
			SNDRV_PCM_STREAM_CAPTURE, dai_id, &input_rate))
		return input_rate;

	return rate;
}

static int mt8512_afe_gasrc_get_output_rate(
	struct snd_pcm_substream *substream, int dai_id, unsigned int rate)
{
	unsigned int output_rate;

	if (mt8512_get_be_cpu_dai_rate(substream, "ETDM2_OUT",
			SNDRV_PCM_STREAM_PLAYBACK, dai_id, &output_rate))
		return output_rate;

	if (mt8512_get_be_cpu_dai_rate(substream, "INT ADDA",
			SNDRV_PCM_STREAM_PLAYBACK, dai_id, &output_rate))
		return output_rate;

	return rate;
}

static int mt8512_afe_gasrc_get_input_fs(struct snd_pcm_substream *substream,
	int dai_id, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int fs;
	struct mt8512_etdm_data *etdm_data;
	int be_rate;

	fs = mt8512_afe_fs_timing(rate);

	if (mt8512_be_cpu_dai_connected(substream, "INT ADDA",
			SNDRV_PCM_STREAM_CAPTURE, dai_id))
		fs = MT8512_FS_AMIC_1X_EN_ASYNC;

	if (mt8512_be_cpu_dai_connected(substream, "ETDM1_IN",
			SNDRV_PCM_STREAM_CAPTURE, dai_id)) {
		etdm_data = &afe_priv->etdm_data[MT8512_ETDM1];
		if (etdm_data->slave_mode[substream->stream]) {
			fs = MT8512_FS_ETDMIN1_1X_EN;
		} else {
			be_rate = mt8512_afe_gasrc_get_input_rate(substream,
				dai_id, rate);
			fs = mt8512_afe_fs_timing(be_rate);
		}
	}

	if (mt8512_be_cpu_dai_connected(substream, "ETDM2_IN",
			SNDRV_PCM_STREAM_CAPTURE, dai_id)) {
		etdm_data = &afe_priv->etdm_data[MT8512_ETDM2];
		if (etdm_data->slave_mode[substream->stream]) {
			fs = MT8512_FS_ETDMIN2_1X_EN;
		} else {
			be_rate = mt8512_afe_gasrc_get_input_rate(substream,
				dai_id, rate);
			fs = mt8512_afe_fs_timing(be_rate);
		}
	}

	if (mt8512_be_cpu_dai_connected(substream, "DMIC",
			SNDRV_PCM_STREAM_CAPTURE, dai_id)) {
		be_rate = mt8512_afe_gasrc_get_input_rate(substream,
			dai_id, rate);
		fs = mt8512_afe_fs_timing(be_rate);
	}

	return fs;
}

static int mt8512_afe_gasrc_get_output_fs(struct snd_pcm_substream *substream,
	int dai_id, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	int fs;
	struct mt8512_etdm_data *etdm_data;
	int be_rate;

	fs = mt8512_afe_fs_timing(rate);

	if (mt8512_be_cpu_dai_connected(substream, "ETDM2_OUT",
		SNDRV_PCM_STREAM_PLAYBACK, dai_id)) {
		etdm_data = &afe_priv->etdm_data[MT8512_ETDM2];
		if (etdm_data->slave_mode[substream->stream]) {
			fs = MT8512_FS_ETDMOUT2_1X_EN;
		} else {
			be_rate = mt8512_afe_gasrc_get_output_rate(substream,
				dai_id, rate);
			fs = mt8512_afe_fs_timing(be_rate);
		}
	}

	if (mt8512_be_cpu_dai_connected(substream, "INT ADDA",
		SNDRV_PCM_STREAM_PLAYBACK, dai_id)) {
		be_rate = mt8512_afe_gasrc_get_output_rate(substream,
			dai_id, rate);
		fs = mt8512_afe_fs_timing(be_rate);
	}

	return fs;
}

static void mt8512_afe_gasrc_set_input_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mask = GASRC_TIMING_CON0_GASRC0_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC0_IN_MODE(fs_timing);
		break;
	case MT8512_GASRC1:
		mask = GASRC_TIMING_CON0_GASRC1_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC1_IN_MODE(fs_timing);
		break;
	case MT8512_GASRC2:
		mask = GASRC_TIMING_CON0_GASRC2_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC2_IN_MODE(fs_timing);
		break;
	case MT8512_GASRC3:
		mask = GASRC_TIMING_CON0_GASRC3_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC3_IN_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, GASRC_TIMING_CON0, mask, val);
}

static void mt8512_afe_gasrc_set_output_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mask = GASRC_TIMING_CON1_GASRC0_OUT_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC0_OUT_MODE(fs_timing);
		break;
	case MT8512_GASRC1:
		mask = GASRC_TIMING_CON1_GASRC1_OUT_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC1_OUT_MODE(fs_timing);
		break;
	case MT8512_GASRC2:
		mask = GASRC_TIMING_CON1_GASRC2_OUT_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC2_OUT_MODE(fs_timing);
		break;
	case MT8512_GASRC3:
		mask = GASRC_TIMING_CON1_GASRC3_OUT_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC3_OUT_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, GASRC_TIMING_CON1, mask, val);
}

struct mt8512_afe_gasrc_freq {
	unsigned int rate;
	unsigned int freq_val;
};

static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_freq_palette_49m_45m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x050000 },
	{ .rate = 12000, .freq_val = 0x078000 },
	{ .rate = 16000, .freq_val = 0x0A0000 },
	{ .rate = 24000, .freq_val = 0x0F0000 },
	{ .rate = 32000, .freq_val = 0x140000 },
	{ .rate = 48000, .freq_val = 0x1E0000 },
	{ .rate = 96000, .freq_val = 0x3C0000 },
	{ .rate = 192000, .freq_val = 0x780000 },
	{ .rate = 384000, .freq_val = 0xF00000 },
	{ .rate = 7350, .freq_val = 0x049800 },
	{ .rate = 11025, .freq_val = 0x06E400 },
	{ .rate = 14700, .freq_val = 0x093000 },
	{ .rate = 22050, .freq_val = 0x0DC800 },
	{ .rate = 29400, .freq_val = 0x126000 },
	{ .rate = 44100, .freq_val = 0x1B9000 },
	{ .rate = 88200, .freq_val = 0x372000 },
	{ .rate = 176400, .freq_val = 0x6E4000 },
	{ .rate = 352800, .freq_val = 0xDC8000 },
};

static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_period_palette_49m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x060000 },
	{ .rate = 12000, .freq_val = 0x040000 },
	{ .rate = 16000, .freq_val = 0x030000 },
	{ .rate = 24000, .freq_val = 0x020000 },
	{ .rate = 32000, .freq_val = 0x018000 },
	{ .rate = 48000, .freq_val = 0x010000 },
	{ .rate = 96000, .freq_val = 0x008000 },
	{ .rate = 192000, .freq_val = 0x004000 },
	{ .rate = 384000, .freq_val = 0x002000 },
	{ .rate = 7350, .freq_val = 0x0687D8 },
	{ .rate = 11025, .freq_val = 0x045A90 },
	{ .rate = 14700, .freq_val = 0x0343EC },
	{ .rate = 22050, .freq_val = 0x022D48 },
	{ .rate = 29400, .freq_val = 0x01A1F6 },
	{ .rate = 44100, .freq_val = 0x0116A4 },
	{ .rate = 88200, .freq_val = 0x008B52 },
	{ .rate = 176400, .freq_val = 0x0045A9 },
	{ .rate = 352800, .freq_val = 0x0022D4 },
};

static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_period_palette_45m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x058332 },
	{ .rate = 12000, .freq_val = 0x03ACCC },
	{ .rate = 16000, .freq_val = 0x02C199 },
	{ .rate = 24000, .freq_val = 0x01D666 },
	{ .rate = 32000, .freq_val = 0x0160CC },
	{ .rate = 48000, .freq_val = 0x00EB33 },
	{ .rate = 96000, .freq_val = 0x007599 },
	{ .rate = 192000, .freq_val = 0x003ACD },
	{ .rate = 384000, .freq_val = 0x001D66 },
	{ .rate = 7350, .freq_val = 0x060000 },
	{ .rate = 11025, .freq_val = 0x040000 },
	{ .rate = 14700, .freq_val = 0x030000 },
	{ .rate = 22050, .freq_val = 0x020000 },
	{ .rate = 29400, .freq_val = 0x018000 },
	{ .rate = 44100, .freq_val = 0x010000 },
	{ .rate = 88200, .freq_val = 0x008000 },
	{ .rate = 176400, .freq_val = 0x004000 },
	{ .rate = 352800, .freq_val = 0x002000 },
};

/* INT_ADDA ADC (RX Tracking of 26m) */
static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_freq_palette_49m_45m_48_cycles[] = {
	{ .rate = 8000, .freq_val = 0x06AAAA },
	{ .rate = 12000, .freq_val = 0x0A0000 },
	{ .rate = 16000, .freq_val = 0x0D5555 },
	{ .rate = 24000, .freq_val = 0x140000 },
	{ .rate = 32000, .freq_val = 0x1AAAAA },
	{ .rate = 48000, .freq_val = 0x280000 },
	{ .rate = 96000, .freq_val = 0x500000 },
	{ .rate = 192000, .freq_val = 0xA00000 },
	{ .rate = 384000, .freq_val = 0x1400000 },
	{ .rate = 11025, .freq_val = 0x093000 },
	{ .rate = 22050, .freq_val = 0x126000 },
	{ .rate = 44100, .freq_val = 0x24C000 },
	{ .rate = 88200, .freq_val = 0x498000 },
	{ .rate = 176400, .freq_val = 0x930000 },
	{ .rate = 352800, .freq_val = 0x1260000 },
};

/* INT_ADDA DAC (TX Tracking of 26m) */
static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_period_palette_49m_48_cycles[] = {
	{ .rate = 8000, .freq_val = 0x048000 },
	{ .rate = 12000, .freq_val = 0x030000 },
	{ .rate = 16000, .freq_val = 0x024000 },
	{ .rate = 24000, .freq_val = 0x018000 },
	{ .rate = 32000, .freq_val = 0x012000 },
	{ .rate = 48000, .freq_val = 0x00C000 },
	{ .rate = 96000, .freq_val = 0x006000 },
	{ .rate = 192000, .freq_val = 0x003000 },
	{ .rate = 384000, .freq_val = 0x001800 },
	{ .rate = 11025, .freq_val = 0x0343EB },
	{ .rate = 22050, .freq_val = 0x01A1F6 },
	{ .rate = 44100, .freq_val = 0x00D0FB },
	{ .rate = 88200, .freq_val = 0x00687D },
	{ .rate = 176400, .freq_val = 0x00343F },
	{ .rate = 352800, .freq_val = 0x001A1F },
};

/* INT_ADDA DAC (TX Tracking of 26m) */
static const
	struct mt8512_afe_gasrc_freq
		mt8512_afe_gasrc_period_palette_45m_441_cycles[] = {
	{ .rate = 8000, .freq_val = 0x25FC0D },
	{ .rate = 12000, .freq_val = 0x1952B3 },
	{ .rate = 16000, .freq_val = 0x12FE06 },
	{ .rate = 24000, .freq_val = 0x0CA95A },
	{ .rate = 32000, .freq_val = 0x097F03 },
	{ .rate = 48000, .freq_val = 0x0654AD },
	{ .rate = 96000, .freq_val = 0x032A56 },
	{ .rate = 192000, .freq_val = 0x01952B },
	{ .rate = 384000, .freq_val = 0x00CA96 },
	{ .rate = 11025, .freq_val = 0x1B9000 },
	{ .rate = 22050, .freq_val = 0x0DC800 },
	{ .rate = 44100, .freq_val = 0x06E400 },
	{ .rate = 88200, .freq_val = 0x037200 },
	{ .rate = 176400, .freq_val = 0x01B900 },
	{ .rate = 352800, .freq_val = 0x00DC80 },
};

static int mt8512_afe_gasrc_get_freq_val(unsigned int rate,
	unsigned int cali_cycles)
{
	int i;
	const struct mt8512_afe_gasrc_freq *freq_palette = NULL;
	int tbl_size = 0;

	if (cali_cycles == 48) {
		freq_palette =
			mt8512_afe_gasrc_freq_palette_49m_45m_48_cycles;
		tbl_size = ARRAY_SIZE(
			mt8512_afe_gasrc_freq_palette_49m_45m_48_cycles);
	} else {
		freq_palette =
			mt8512_afe_gasrc_freq_palette_49m_45m_64_cycles;
		tbl_size = ARRAY_SIZE(
			mt8512_afe_gasrc_freq_palette_49m_45m_64_cycles);
	}

	for (i = 0; i < tbl_size; i++)
		if (freq_palette[i].rate == rate)
			return freq_palette[i].freq_val;

	return -EINVAL;
}

static int mt8512_afe_gasrc_get_period_val(unsigned int rate,
	bool op_freq_45m, unsigned int cali_cycles)
{
	int i;
	const struct mt8512_afe_gasrc_freq *period_palette = NULL;
	int tbl_size = 0;

	if (cali_cycles == 48) {
		period_palette =
			mt8512_afe_gasrc_period_palette_49m_48_cycles;
		tbl_size = ARRAY_SIZE(
			mt8512_afe_gasrc_period_palette_49m_48_cycles);
	} else if (cali_cycles == 441) {
		period_palette =
			mt8512_afe_gasrc_period_palette_45m_441_cycles;
		tbl_size = ARRAY_SIZE(
			mt8512_afe_gasrc_period_palette_45m_441_cycles);
	} else {
		if (op_freq_45m) {
			period_palette =
				mt8512_afe_gasrc_period_palette_45m_64_cycles;
			tbl_size = ARRAY_SIZE(
				mt8512_afe_gasrc_period_palette_45m_64_cycles);
		} else {
			period_palette =
				mt8512_afe_gasrc_period_palette_49m_64_cycles;
			tbl_size = ARRAY_SIZE(
				mt8512_afe_gasrc_period_palette_49m_64_cycles);
		}
	}

	for (i = 0; i < tbl_size; i++) {
		if (period_palette[i].rate == rate)
			return period_palette[i].freq_val;
	}

	return -EINVAL;
}

static void mt8512_afe_gasrc_set_rx_mode_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int input_rate, int output_rate)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int cali_cycles = 0;
	unsigned int ctrl_reg = 0;
	int val = 0;

	if (gasrc_id < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_OFS_SEL_MASK,
		GASRC_NEW_CON0_CHSET0_OFS_SEL_RX);
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_IFS_SEL_MASK,
		GASRC_NEW_CON0_CHSET0_IFS_SEL_RX);

	cali_cycles = afe_priv->gasrc_data[gasrc_id].cali_cycles;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con2;
	val = mt8512_afe_gasrc_get_freq_val(output_rate, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con3;
	val = mt8512_afe_gasrc_get_freq_val(input_rate, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

static void mt8512_afe_gasrc_set_tx_mode_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int input_rate, int output_rate)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	bool gasrc_op_freq_45m;
	unsigned int cali_cycles;
	unsigned int ctrl_reg = 0;
	int val = 0;

	if (gasrc_id < 0)
		return;

	gasrc_op_freq_45m = afe_priv->gasrc_data[gasrc_id].op_freq_45m;
	cali_cycles = afe_priv->gasrc_data[gasrc_id].cali_cycles;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_OFS_SEL_MASK,
		GASRC_NEW_CON0_CHSET0_OFS_SEL_TX);
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_IFS_SEL_MASK,
		GASRC_NEW_CON0_CHSET0_IFS_SEL_TX);

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con4;
	val = mt8512_afe_gasrc_get_period_val(output_rate,
		gasrc_op_freq_45m, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con1;
	val = mt8512_afe_gasrc_get_period_val(input_rate,
		gasrc_op_freq_45m, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

static void mt8512_afe_gasrc_use_sel(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, bool no_bypass)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int mask = 0;
	unsigned int val = 0;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mask = GASRC_CFG0_GASRC0_USE_SEL_MASK;
		val = GASRC_CFG0_GASRC0_USE_SEL(no_bypass);
		break;
	case MT8512_GASRC1:
		mask = GASRC_CFG0_GASRC1_USE_SEL_MASK;
		val = GASRC_CFG0_GASRC1_USE_SEL(no_bypass);
		break;
	case MT8512_GASRC2:
		mask = GASRC_CFG0_GASRC2_USE_SEL_MASK;
		val = GASRC_CFG0_GASRC2_USE_SEL(no_bypass);
		break;
	case MT8512_GASRC3:
		mask = GASRC_CFG0_GASRC3_USE_SEL_MASK;
		val = GASRC_CFG0_GASRC3_USE_SEL(no_bypass);
		break;
	default:
		break;
	}

	regmap_update_bits(afe->regmap, GASRC_CFG0, mask, val);
}

struct mt8512_afe_gasrc_lrck_sel {
	int fs_timing;
	unsigned int lrck_sel_val;
};

static const
	struct mt8512_afe_gasrc_lrck_sel mt8512_afe_gasrc_lrck_sels[] = {
	{
		.fs_timing = MT8512_FS_ETDMIN2_1X_EN,
		.lrck_sel_val = MT8512_AFE_GASRC_LRCK_SEL_ETDM_IN2,
	},
	{
		.fs_timing = MT8512_FS_ETDMIN1_1X_EN,
		.lrck_sel_val = MT8512_AFE_GASRC_LRCK_SEL_ETDM_IN1,
	},
	{
		.fs_timing = MT8512_FS_ETDMOUT2_1X_EN,
		.lrck_sel_val = MT8512_AFE_GASRC_LRCK_SEL_ETDM_OUT2,
	},
	{
		.fs_timing = MT8512_FS_AMIC_1X_EN_ASYNC,
		.lrck_sel_val = MT8512_AFE_GASRC_LRCK_SEL_UL_VIRTUAL,
	},
};

static int mt8512_afe_gasrc_get_lrck_sel_val(int fs_timing)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8512_afe_gasrc_lrck_sels); i++)
		if (mt8512_afe_gasrc_lrck_sels[i].fs_timing == fs_timing)
			return mt8512_afe_gasrc_lrck_sels[i].lrck_sel_val;

	return -EINVAL;
}

static void mt8512_afe_gasrc_sel_lrck(
	struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int mask = 0;
	unsigned int val = 0;

	val = mt8512_afe_gasrc_get_lrck_sel_val(fs_timing);
	if (val < 0)
		return;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mask = GASRC_CFG0_GASRC0_LRCK_SEL_MASK;
		val = GASRC_CFG0_GASRC0_LRCK_SEL(val);
		break;
	case MT8512_GASRC1:
		mask = GASRC_CFG0_GASRC1_LRCK_SEL_MASK;
		val = GASRC_CFG0_GASRC1_LRCK_SEL(val);
		break;
	case MT8512_GASRC2:
		mask = GASRC_CFG0_GASRC2_LRCK_SEL_MASK;
		val = GASRC_CFG0_GASRC2_LRCK_SEL(val);
		break;
	case MT8512_GASRC3:
		mask = GASRC_CFG0_GASRC3_LRCK_SEL_MASK;
		val = GASRC_CFG0_GASRC3_LRCK_SEL(val);
		break;
	default:
		break;
	}

	regmap_update_bits(afe->regmap, GASRC_CFG0, mask, val);
}

static void mt8512_afe_gasrc_sel_cali_clk(
	struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, bool gasrc_op_freq_45m)
{
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int mask = 0;
	unsigned int val = 0;

	val = gasrc_op_freq_45m;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mask = PWR1_ASM_CON1_GASRC0_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC0_CALI_CK_SEL(val);
		break;
	case MT8512_GASRC1:
		mask = PWR1_ASM_CON1_GASRC1_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC1_CALI_CK_SEL(val);
		break;
	case MT8512_GASRC2:
		mask = PWR1_ASM_CON1_GASRC2_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC2_CALI_CK_SEL(val);
		break;
	case MT8512_GASRC3:
		mask = PWR1_ASM_CON1_GASRC3_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC3_CALI_CK_SEL(val);
		break;
	default:
		break;
	}

	regmap_update_bits(afe->regmap, PWR1_ASM_CON1, mask, val);
}

static bool mt8512_afe_gasrc_is_tx_tracking(int fs_timing)
{
	if (fs_timing == MT8512_FS_ETDMOUT2_1X_EN)
		return true;
	else
		return false;
}

static bool mt8512_afe_gasrc_is_rx_tracking(int fs_timing)
{
	if ((fs_timing == MT8512_FS_ETDMIN1_1X_EN) ||
		(fs_timing == MT8512_FS_ETDMIN2_1X_EN) ||
		(fs_timing == MT8512_FS_AMIC_1X_EN_ASYNC))
		return true;
	else
		return false;
}

static void mt8512_afe_gasrc_enable_iir(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	if (gasrc_id < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_IIR_STAGE_MASK,
		GASRC_NEW_CON0_CHSET0_IIR_STAGE(8));

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_CLR_IIR_HISTORY,
		GASRC_NEW_CON0_CHSET0_CLR_IIR_HISTORY);

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_IIR_EN,
		GASRC_NEW_CON0_CHSET0_IIR_EN);
}

static void mt8512_afe_gasrc_disable_iir(struct mtk_base_afe *afe,
	int gasrc_id)
{
	unsigned int ctrl_reg = 0;

	if (gasrc_id < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_CHSET0_IIR_EN, 0);
}

enum afe_gasrc_iir_coeff_table_id {
	MT8512_AFE_GASRC_IIR_COEFF_384_to_352 = 0,
	MT8512_AFE_GASRC_IIR_COEFF_256_to_192,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_256,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	MT8512_AFE_GASRC_IIR_COEFF_256_to_96,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_128,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	MT8512_AFE_GASRC_IIR_COEFF_256_to_48,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_64,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_48,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_44,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_32,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	MT8512_AFE_GASRC_IIR_COEFF_352_to_24,
	MT8512_AFE_GASRC_IIR_COEFF_384_to_24,
	MT8512_AFE_GASRC_IIR_TABLES,
};

struct mt8512_afe_gasrc_iir_coeff_table_id {
	int input_rate;
	int output_rate;
	int table_id;
};

static const struct mt8512_afe_gasrc_iir_coeff_table_id
	mt8512_afe_gasrc_iir_coeff_table_ids[] = {
	{
		.input_rate = 8000,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 12000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 12000,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 16000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 16000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 16000,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 16000,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 24000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 24000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 24000,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 24000,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 24000,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 32000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 32000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_96,
	},
	{
		.input_rate = 32000,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 32000,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 32000,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 32000,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 32000,
		.output_rate = 29400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 48000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 48000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 48000,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 48000,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 48000,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 48000,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 48000,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 48000,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 96000,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 96000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 96000,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 96000,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 96000,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 96000,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 96000,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 96000,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 96000,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 96000,
		.output_rate = 88200,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 192000,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 192000,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 192000,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 192000,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 192000,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 192000,
		.output_rate = 96000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 192000,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 192000,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 192000,
		.output_rate = 88200,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 192000,
		.output_rate = 176400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 384000,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 384000,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 384000,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 384000,
		.output_rate = 96000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 384000,
		.output_rate = 192000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 384000,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 384000,
		.output_rate = 88200,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 384000,
		.output_rate = 176400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 384000,
		.output_rate = 352800,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 11025,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 11025,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 14700,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 14700,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 14700,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 22050,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 22050,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 22050,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 29400,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 29400,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 29400,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 44100,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 44100,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 44100,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 44100,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 44100,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 44100,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 44100,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 29400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 88200,
		.output_rate = 8000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 88200,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 88200,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 88200,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 88200,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 88200,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 88200,
		.output_rate = 7350,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 88200,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 88200,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 88200,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 88200,
		.output_rate = 29400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 88200,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 176400,
		.output_rate = 12000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_24,
	},
	{
		.input_rate = 176400,
		.output_rate = 16000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 176400,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 176400,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 176400,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 176400,
		.output_rate = 96000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 176400,
		.output_rate = 11025,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 176400,
		.output_rate = 14700,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 176400,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 176400,
		.output_rate = 29400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 176400,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 176400,
		.output_rate = 88200,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 352800,
		.output_rate = 24000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_24,
	},
	{
		.input_rate = 352800,
		.output_rate = 32000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 352800,
		.output_rate = 48000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 352800,
		.output_rate = 96000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 352800,
		.output_rate = 192000,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 352800,
		.output_rate = 22050,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 352800,
		.output_rate = 29400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 352800,
		.output_rate = 44100,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 352800,
		.output_rate = 88200,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 352800,
		.output_rate = 176400,
		.table_id = MT8512_AFE_GASRC_IIR_COEFF_384_to_192,
	},
};

#define IIR_NUMS (48)

static const unsigned int
	mt8512_afe_gasrc_iir_coeffs[MT8512_AFE_GASRC_IIR_TABLES][IIR_NUMS] = {
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_352] = {
		0x10bea3af, 0x2007e9be, 0x10bea3af,
		0xe2821372, 0xf0848d58, 0x00000003,
		0x08f9d435, 0x113d1a1f, 0x08f9d435,
		0xe31a73c5, 0xf1030af1, 0x00000003,
		0x09dd37b9, 0x13106967, 0x09dd37b9,
		0xe41398e1, 0xf1c98ae5, 0x00000003,
		0x0b55c74b, 0x16182d46, 0x0b55c74b,
		0xe5bce8cb, 0xf316f594, 0x00000003,
		0x0e02cb05, 0x1b950f07, 0x0e02cb05,
		0xf44d829a, 0xfaa9876b, 0x00000004,
		0x13e0e18e, 0x277f6d77, 0x13e0e18e,
		0xf695efae, 0xfc700da4, 0x00000004,
		0x0db3df0d, 0x1b6240b3, 0x0db3df0d,
		0xf201ce8e, 0xfca24567, 0x00000003,
		0x06b31e0f, 0x0cca96d1, 0x06b31e0f,
		0xc43a9021, 0xe051c370, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_256_to_192] = {
		0x0de3c667, 0x137bf0e3, 0x0de3c667,
		0xd9575388, 0xe0d4770d, 0x00000002,
		0x0e54ed46, 0x1474090f, 0x0e54ed46,
		0xdb1c8213, 0xe2a7b6b7, 0x00000002,
		0x0d58713b, 0x13bde364, 0x05d8713b,
		0xde0a3770, 0xe5183cde, 0x00000002,
		0x0bdcfce3, 0x128ef355, 0x0bdcfce3,
		0xe2be28af, 0xe8affd19, 0x00000002,
		0x139091b3, 0x20f20a8e, 0x139091b3,
		0xe9ed58af, 0xedff795d, 0x00000002,
		0x0e68e9cd, 0x1a4cb00b, 0x0e68e9cd,
		0xf3ba2b24, 0xf5275137, 0x00000002,
		0x13079595, 0x251713f9, 0x13079595,
		0xf78c204d, 0xf227616a, 0x00000000,
		0x00000000, 0x2111eb8f, 0x2111eb8f,
		0x0014ac5b, 0x00000000, 0x00000006,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_256] = {
		0x0db45c84, 0x1113e68a, 0x0db45c84,
		0xdf58fbd3, 0xe0e51ba2, 0x00000002,
		0x0e0c4d8f, 0x11eaf5ef, 0x0e0c4d8f,
		0xe11e9264, 0xe2da4b80, 0x00000002,
		0x0cf2558c, 0x1154c11a, 0x0cf2558c,
		0xe41c6288, 0xe570c517, 0x00000002,
		0x0b5132d7, 0x10545ecd, 0x0b5132d7,
		0xe8e2e944, 0xe92f8dc6, 0x00000002,
		0x1234ffbb, 0x1cfba5c7, 0x1234ffbb,
		0xf00653e0, 0xee9406e3, 0x00000002,
		0x0cfd073a, 0x170277ad, 0x0cfd073a,
		0xf96e16e7, 0xf59562f9, 0x00000002,
		0x08506c2b, 0x1011cd72, 0x08506c2b,
		0x164a9eae, 0xe4203311, 0xffffffff,
		0x00000000, 0x3d58af1e, 0x3d58af1e,
		0x001bee13, 0x00000000, 0x00000007,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_256] = {
		0x0eca2fa9, 0x0f2b0cd3, 0x0eca2fa9,
		0xf50313ef, 0xf15857a7, 0x00000003,
		0x0ee239a9, 0x1045115c, 0x0ee239a9,
		0xec9f2976, 0xe5090807, 0x00000002,
		0x0ec57a45, 0x11d000f7, 0x0ec57a45,
		0xf0bb67bb, 0xe84c86de, 0x00000002,
		0x0e85ba7e, 0x13ee7e9a, 0x0e85ba7e,
		0xf6c74ebb, 0xecdba82c, 0x00000002,
		0x1cba1ac9, 0x2da90ada, 0x1cba1ac9,
		0xfecba589, 0xf2c756e1, 0x00000002,
		0x0f79dec4, 0x1c27f5e0, 0x0f79dec4,
		0x03c44399, 0xfc96c6aa, 0x00000003,
		0x1104a702, 0x21a72c89, 0x1104a702,
		0x1b6a6fb8, 0xfb5ee0f2, 0x00000001,
		0x0622fc30, 0x061a0c67, 0x0622fc30,
		0xe88911f2, 0xe0da327a, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_192] = {
		0x1c012b9a, 0x09302bd9, 0x1c012b9a,
		0x0056c6d0, 0xe2b7f35c, 0x00000002,
		0x1b60cee5, 0x0b59639b, 0x1b60cee5,
		0x045dc965, 0xca2264a0, 0x00000001,
		0x19ec96ad, 0x0eb20aa9, 0x19ec96ad,
		0x0a6789cd, 0xd08944ba, 0x00000001,
		0x17c243aa, 0x1347e7fc, 0x17c243aa,
		0x131e03a8, 0xd9241dd4, 0x00000001,
		0x1563b168, 0x1904032f, 0x1563b168,
		0x0f0d206b, 0xf1d7f8e1, 0x00000002,
		0x14cd0206, 0x2169e2af, 0x14cd0206,
		0x14a5d991, 0xf7279caf, 0x00000002,
		0x0aac4c7f, 0x14cb084b, 0x0aac4c7f,
		0x30bc41c6, 0xf5565720, 0x00000001,
		0x0cea20d5, 0x03bc5f00, 0x0cea20d5,
		0xfeec800a, 0xc1b99664, 0x00000001,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_192] = {
		0x1bd356f3, 0x012e014f, 0x1bd356f3,
		0x081be0a6, 0xe28e2407, 0x00000002,
		0x0d7d8ee8, 0x01b9274d, 0x0d7d8ee8,
		0x09857a7b, 0xe4cae309, 0x00000002,
		0x0c999cbe, 0x038e89c5, 0x0c999cbe,
		0x0beae5bc, 0xe7ded2a4, 0x00000002,
		0x0b4b6e2c, 0x061cd206, 0x0b4b6e2c,
		0x0f6a2551, 0xec069422, 0x00000002,
		0x13ad5974, 0x129397e7, 0x13ad5974,
		0x13d3c166, 0xf11cacb8, 0x00000002,
		0x126195d4, 0x1b259a6c, 0x126195d4,
		0x184cdd94, 0xf634a151, 0x00000002,
		0x092aa1ea, 0x11add077, 0x092aa1ea,
		0x3682199e, 0xf31b28fc, 0x00000001,
		0x0e09b91b, 0x0010b76f, 0x0e09b91b,
		0x0f0e2575, 0xc19d364a, 0x00000001,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_176] = {
		0x1b4feb25, 0xfa1874df, 0x1b4feb25,
		0x0fc84364, 0xe27e7427, 0x00000002,
		0x0d22ad1f, 0xfe465ea8, 0x0d22ad1f,
		0x10d89ab2, 0xe4aa760e, 0x00000002,
		0x0c17b497, 0x004c9a14, 0x0c17b497,
		0x12ba36ef, 0xe7a11513, 0x00000002,
		0x0a968b87, 0x031b65c2, 0x0a968b87,
		0x157c39d1, 0xeb9561ce, 0x00000002,
		0x11cea26a, 0x0d025bcc, 0x11cea26a,
		0x18ef4a32, 0xf05a2342, 0x00000002,
		0x0fe5d188, 0x156af55c, 0x0fe5d188,
		0x1c6234df, 0xf50cd288, 0x00000002,
		0x07a1ea25, 0x0e900dd7, 0x07a1ea25,
		0x3d441ae6, 0xf0314c15, 0x00000001,
		0x0dd3517a, 0xfc7f1621, 0x0dd3517a,
		0x1ee4972a, 0xc193ad77, 0x00000001,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_256_to_96] = {
		0x0bad1c6d, 0xf7125e39, 0x0bad1c6d,
		0x200d2195, 0xe0e69a20, 0x00000002,
		0x0b7cc85d, 0xf7b2aa2b, 0x0b7cc85d,
		0x1fd4a137, 0xe2d2e8fc, 0x00000002,
		0x09ad4898, 0xf9f3edb1, 0x09ad4898,
		0x202ffee3, 0xe533035b, 0x00000002,
		0x073ebe31, 0xfcd552f2, 0x073ebe31,
		0x2110eb62, 0xe84975f6, 0x00000002,
		0x092af7cc, 0xff2b1fc9, 0x092af7cc,
		0x2262052a, 0xec1ceb75, 0x00000002,
		0x09655d3e, 0x04f0939d, 0x09655d3e,
		0x47cf219d, 0xe075904a, 0x00000001,
		0x021b3ca5, 0x03057f44, 0x021b3ca5,
		0x4a5c8f68, 0xe72b7f7b, 0x00000001,
		0x00000000, 0x389ecf53, 0x358ecf53,
		0x04b60049, 0x00000000, 0x00000004,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_128] = {
		0x0c4deacd, 0xf5b3be35, 0x0c4deacd,
		0x20349d1f, 0xe0b9a80d, 0x00000002,
		0x0c5dbbaa, 0xf6157998, 0x0c5dbbaa,
		0x200c143d, 0xe25209ea, 0x00000002,
		0x0a9de1bd, 0xf85ee460, 0x0a9de1bd,
		0x206099de, 0xe46a166c, 0x00000002,
		0x081f9a34, 0xfb7ffe47, 0x081f9a34,
		0x212dd0f7, 0xe753c9ab, 0x00000002,
		0x0a6f9ddb, 0xfd863e9e, 0x0a6f9ddb,
		0x226bd8a2, 0xeb2ead0b, 0x00000002,
		0x05497d0e, 0x01ebd7f0, 0x05497d0e,
		0x23eba2f6, 0xef958aff, 0x00000002,
		0x008e7c5f, 0x00be6aad, 0x008e7c5f,
		0x4a74b30a, 0xe6b0319a, 0x00000001,
		0x00000000, 0x38f3c5aa, 0x38f3c5aa,
		0x012e1306, 0x00000000, 0x00000006,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_128] = {
		0x0cf188aa, 0xf37845cc, 0x0cf188aa,
		0x126b5cbc, 0xf10e5785, 0x00000003,
		0x0c32c481, 0xf503c49b, 0x0c32c481,
		0x24e5a686, 0xe3edcb35, 0x00000002,
		0x0accda0f, 0xf7ad602d, 0x0accda0f,
		0x2547ad4f, 0xe65c4390, 0x00000002,
		0x08d6d7fb, 0xfb56b002, 0x08d6d7fb,
		0x25f3f39f, 0xe9860165, 0x00000002,
		0x0d4b1ceb, 0xff189a5d, 0x0d4b1ceb,
		0x26d3a3a5, 0xed391db5, 0x00000002,
		0x0a060fcf, 0x07a2d23a, 0x0a060fcf,
		0x27b2168e, 0xf0c10173, 0x00000002,
		0x040b6e8c, 0x0742638c, 0x040b6e8c,
		0x5082165c, 0xe5f8f032, 0x00000001,
		0x067a1ae1, 0xf98acf04, 0x067a1ae1,
		0x2526b255, 0xe0ab23e6, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_96] = {
		0x0ba3aaf1, 0xf0c12941, 0x0ba3aaf1,
		0x2d8fe4ae, 0xe097f1ad, 0x00000002,
		0x0be92064, 0xf0b1f1a9, 0x0be92064,
		0x2d119d04, 0xe1e5fe1b, 0x00000002,
		0x0a1220de, 0xf3a9aff8, 0x0a1220de,
		0x2ccb18cb, 0xe39903cf, 0x00000002,
		0x07794a30, 0xf7c2c155, 0x07794a30,
		0x2ca647c8, 0xe5ef0ccd, 0x00000002,
		0x0910b1c4, 0xf84c9886, 0x0910b1c4,
		0x2c963877, 0xe8fbcb7a, 0x00000002,
		0x041d6154, 0xfec82c8a, 0x041d6154,
		0x2c926893, 0xec6aa839, 0x00000002,
		0x005b2676, 0x0050bb1f, 0x005b2676,
		0x5927e9f4, 0xde9fd5bc, 0x00000001,
		0x00000000, 0x2b1e5dc1, 0x2b1e5dc1,
		0x0164aa09, 0x00000000, 0x00000006,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_96] = {
		0x0481f41d, 0xf9c1b194, 0x0481f41d,
		0x31c66864, 0xe0581a1d, 0x00000002,
		0x0a3e5a4c, 0xf216665d, 0x0a3e5a4c,
		0x31c3de69, 0xe115ebae, 0x00000002,
		0x0855f15c, 0xf5369aef, 0x0855f15c,
		0x323c17ad, 0xe1feed04, 0x00000002,
		0x05caeeeb, 0xf940c54b, 0x05caeeeb,
		0x33295d2b, 0xe3295c94, 0x00000002,
		0x0651a46a, 0xfa4d6542, 0x0651a46a,
		0x3479d138, 0xe49580b2, 0x00000002,
		0x025e0ccb, 0xff36a412, 0x025e0ccb,
		0x35f517d7, 0xe6182a82, 0x00000002,
		0x0085eff3, 0x0074e0ca, 0x0085eff3,
		0x372ef0de, 0xe7504e71, 0x00000002,
		0x00000000, 0x29b76685, 0x29b76685,
		0x0deab1c3, 0x00000000, 0x00000003,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_88] = {
		0x0c95e01f, 0xed56f8fc, 0x0c95e01f,
		0x191b8467, 0xf0c99b0e, 0x00000003,
		0x0bbee41a, 0xef0e8160, 0x0bbee41a,
		0x31c02b41, 0xe2ef4cd9, 0x00000002,
		0x0a2d258f, 0xf2225b96, 0x0a2d258f,
		0x314c8bd2, 0xe4c10e08, 0x00000002,
		0x07f9e42a, 0xf668315f, 0x07f9e42a,
		0x30cf47d4, 0xe71e3418, 0x00000002,
		0x0afd6fa9, 0xf68f867d, 0x0afd6fa9,
		0x3049674d, 0xe9e0cf4b, 0x00000002,
		0x06ebc830, 0xffaa9acd, 0x06ebc830,
		0x2fcee1bf, 0xec81ee52, 0x00000002,
		0x010de038, 0x01a27806, 0x010de038,
		0x2f82d453, 0xee2ade9b, 0x00000002,
		0x064f0462, 0xf68a0d30, 0x064f0462,
		0x32c81742, 0xe07f3a37, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_256_to_48] = {
		0x02b72fb4, 0xfb7c5152, 0x02b72fb4,
		0x374ab8ef, 0xe039095c, 0x00000002,
		0x05ca62de, 0xf673171b, 0x05ca62de,
		0x1b94186a, 0xf05c2de7, 0x00000003,
		0x09a9656a, 0xf05ffe29, 0x09a9656a,
		0x37394e81, 0xe1611f87, 0x00000002,
		0x06e86c29, 0xf54bf713, 0x06e86c29,
		0x37797f41, 0xe24ce1f6, 0x00000002,
		0x07a6b7c2, 0xf5491ea7, 0x07a6b7c2,
		0x37e40444, 0xe3856d91, 0x00000002,
		0x02bf8a3e, 0xfd2f5fa6, 0x02bf8a3e,
		0x38673190, 0xe4ea5a4d, 0x00000002,
		0x007e1bd5, 0x000e76ca, 0x007e1bd5,
		0x38da5414, 0xe61afd77, 0x00000002,
		0x00000000, 0x2038247b, 0x2038247b,
		0x07212644, 0x00000000, 0x00000004,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_64] = {
		0x05c89f29, 0xf6443184, 0x05c89f29,
		0x1bbe0f00, 0xf034bf19, 0x00000003,
		0x05e47be3, 0xf6284bfe, 0x05e47be3,
		0x1b73d610, 0xf0a9a268, 0x00000003,
		0x09eb6c29, 0xefbc8df5, 0x09eb6c29,
		0x365264ff, 0xe286ce76, 0x00000002,
		0x0741f28e, 0xf492d155, 0x0741f28e,
		0x35a08621, 0xe4320cfe, 0x00000002,
		0x087cdc22, 0xf3daa1c7, 0x087cdc22,
		0x34c55ef0, 0xe6664705, 0x00000002,
		0x038022af, 0xfc43da62, 0x038022af,
		0x33d2b188, 0xe8e92eb8, 0x00000002,
		0x001de8ed, 0x0001bd74, 0x001de8ed,
		0x33061aa8, 0xeb0d6ae7, 0x00000002,
		0x00000000, 0x3abd8743, 0x3abd8743,
		0x032b3f7f, 0x00000000, 0x00000005,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_64] = {
		0x05690759, 0xf69bdff3, 0x05690759,
		0x392fbdf5, 0xe032c3cc, 0x00000002,
		0x05c3ff7a, 0xf60d6b05, 0x05c3ff7a,
		0x1c831a72, 0xf052119a, 0x00000003,
		0x0999efb9, 0xefae71b0, 0x0999efb9,
		0x3900fd02, 0xe13a60b9, 0x00000002,
		0x06d5aa46, 0xf4c1d0ea, 0x06d5aa46,
		0x39199f34, 0xe20c15e1, 0x00000002,
		0x077f7d1d, 0xf49411e4, 0x077f7d1d,
		0x394b3591, 0xe321be50, 0x00000002,
		0x02a14b6b, 0xfcd3c8a5, 0x02a14b6b,
		0x398b4c12, 0xe45e5473, 0x00000002,
		0x00702155, 0xffef326c, 0x00702155,
		0x39c46c90, 0xe56c1e59, 0x00000002,
		0x00000000, 0x1c69d66c, 0x1c69d66c,
		0x0e76f270, 0x00000000, 0x00000003,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_48] = {
		0x05be8a21, 0xf589fb98, 0x05be8a21,
		0x1d8de063, 0xf026c3d8, 0x00000003,
		0x05ee4f4f, 0xf53df2e5, 0x05ee4f4f,
		0x1d4d87e2, 0xf07d5518, 0x00000003,
		0x0a015402, 0xee079bc7, 0x0a015402,
		0x3a0a0c2b, 0xe1e16c40, 0x00000002,
		0x07512c6a, 0xf322f651, 0x07512c6a,
		0x394e82c2, 0xe326def2, 0x00000002,
		0x087a5316, 0xf1d3ba1f, 0x087a5316,
		0x385bbd4a, 0xe4dbe26b, 0x00000002,
		0x035bd161, 0xfb2b7588, 0x035bd161,
		0x37464782, 0xe6d6a034, 0x00000002,
		0x00186dd8, 0xfff28830, 0x00186dd8,
		0x365746b9, 0xe88d9a4a, 0x00000002,
		0x00000000, 0x2cd02ed1, 0x2cd02ed1,
		0x035f6308, 0x00000000, 0x00000005,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_48] = {
		0x0c68c88c, 0xe9266466, 0x0c68c88c,
		0x1db3d4c3, 0xf0739c07, 0x00000003,
		0x05c69407, 0xf571a70a, 0x05c69407,
		0x1d6f1d3b, 0xf0d89718, 0x00000003,
		0x09e8d133, 0xee2a68df, 0x09e8d133,
		0x3a32d61b, 0xe2c2246a, 0x00000002,
		0x079233b7, 0xf2d17252, 0x079233b7,
		0x3959a2c3, 0xe4295381, 0x00000002,
		0x09c2822e, 0xf0613d7b, 0x09c2822e,
		0x385c3c48, 0xe5d3476b, 0x00000002,
		0x050e0b2c, 0xfa200d5d, 0x050e0b2c,
		0x37688f21, 0xe76fc030, 0x00000002,
		0x006ddb6e, 0x00523f01, 0x006ddb6e,
		0x36cd234d, 0xe8779510, 0x00000002,
		0x0635039f, 0xf488f773, 0x0635039f,
		0x3be42508, 0xe0488e99, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_44] = {
		0x0c670696, 0xe8dc1ef2, 0x0c670696,
		0x1e05c266, 0xf06a9f0d, 0x00000003,
		0x05c60160, 0xf54b9f4a, 0x05c60160,
		0x1dc3811d, 0xf0c7e4db, 0x00000003,
		0x09e74455, 0xeddfc92a, 0x09e74455,
		0x3adfddda, 0xe28c4ae3, 0x00000002,
		0x078ea9ae, 0xf28c3ba7, 0x078ea9ae,
		0x3a0a98e8, 0xe3d93541, 0x00000002,
		0x09b32647, 0xefe954c5, 0x09b32647,
		0x3910a244, 0xe564f781, 0x00000002,
		0x04f0e9e4, 0xf9b7e8d5, 0x04f0e9e4,
		0x381f6928, 0xe6e5316c, 0x00000002,
		0x006303ee, 0x003ae836, 0x006303ee,
		0x37852c0e, 0xe7db78c1, 0x00000002,
		0x06337ac0, 0xf46665c5, 0x06337ac0,
		0x3c818406, 0xe042df81, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_32] = {
		0x07d25973, 0xf0fd68ae, 0x07d25973,
		0x3dd9d640, 0xe02aaf11, 0x00000002,
		0x05a0521d, 0xf5390cc4, 0x05a0521d,
		0x1ec7dff7, 0xf044be0d, 0x00000003,
		0x04a961e1, 0xf71c730b, 0x04a961e1,
		0x1e9edeee, 0xf082b378, 0x00000003,
		0x06974728, 0xf38e3bf1, 0x06974728,
		0x3cd69b60, 0xe1afd01c, 0x00000002,
		0x072d4553, 0xf2c1e0e2, 0x072d4553,
		0x3c54fdc3, 0xe28e96b6, 0x00000002,
		0x02802de3, 0xfbb07dd5, 0x02802de3,
		0x3bc4f40f, 0xe38a3256, 0x00000002,
		0x000ce31b, 0xfff0d7a8, 0x000ce31b,
		0x3b4bbb40, 0xe45f55d6, 0x00000002,
		0x00000000, 0x1ea1b887, 0x1ea1b887,
		0x03b1b27d, 0x00000000, 0x00000005,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_32] = {
		0x0c5074a7, 0xe83ee090, 0x0c5074a7,
		0x1edf8fe7, 0xf04ec5d0, 0x00000003,
		0x05bbb01f, 0xf4fa20a7, 0x05bbb01f,
		0x1ea87e16, 0xf093b881, 0x00000003,
		0x04e8e57f, 0xf69fc31d, 0x04e8e57f,
		0x1e614210, 0xf0f1139e, 0x00000003,
		0x07756686, 0xf1f67c0b, 0x07756686,
		0x3c0a3b55, 0xe2d8c5a6, 0x00000002,
		0x097212e8, 0xeede0608, 0x097212e8,
		0x3b305555, 0xe3ff02e3, 0x00000002,
		0x0495d6c0, 0xf8bf1399, 0x0495d6c0,
		0x3a5c93a1, 0xe51e0d14, 0x00000002,
		0x00458b2d, 0xfffdc761, 0x00458b2d,
		0x39d4793b, 0xe5d6d407, 0x00000002,
		0x0609587b, 0xf456ed0f, 0x0609587b,
		0x3e1d20e1, 0xe0315c96, 0x00000002,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_352_to_24] = {
		0x062002ee, 0xf4075ac9, 0x062002ee,
		0x1f577599, 0xf0166280, 0x00000003,
		0x05cdb68c, 0xf4ab2e81, 0x05cdb68c,
		0x1f2a7a17, 0xf0484eb7, 0x00000003,
		0x04e3078b, 0xf67b954a, 0x04e3078b,
		0x1ef25b71, 0xf08a5bcf, 0x00000003,
		0x071fc81e, 0xf23391f6, 0x071fc81e,
		0x3d4bc51b, 0xe1cdf67e, 0x00000002,
		0x08359c1c, 0xf04d3910, 0x08359c1c,
		0x3c80bf1e, 0xe2c6cf99, 0x00000002,
		0x0331888d, 0xfa1ebde6, 0x0331888d,
		0x3b94c153, 0xe3e96fad, 0x00000002,
		0x00143063, 0xffe1d1af, 0x00143063,
		0x3ac672e3, 0xe4e7f96f, 0x00000002,
		0x00000000, 0x2d7cf831, 0x2d7cf831,
		0x074e3a4f, 0x00000000, 0x00000004,
	},
	[MT8512_AFE_GASRC_IIR_COEFF_384_to_24] = {
		0x0c513993, 0xe7dbde26, 0x0c513993,
		0x1f4e3b98, 0xf03b6bee, 0x00000003,
		0x05bd9980, 0xf4c4fb19, 0x05bd9980,
		0x1f21aa2b, 0xf06fa0e5, 0x00000003,
		0x04eb9c21, 0xf6692328, 0x04eb9c21,
		0x1ee6fb2f, 0xf0b6982c, 0x00000003,
		0x07795c9e, 0xf18d56cf, 0x07795c9e,
		0x3d345c1a, 0xe229a2a1, 0x00000002,
		0x096d3d11, 0xee265518, 0x096d3d11,
		0x3c7d096a, 0xe30bee74, 0x00000002,
		0x0478f0db, 0xf8270d5a, 0x0478f0db,
		0x3bc96998, 0xe3ea3cf8, 0x00000002,
		0x0037d4b8, 0xffdedcf0, 0x0037d4b8,
		0x3b553ec9, 0xe47a2910, 0x00000002,
		0x0607e296, 0xf42bc1d7, 0x0607e296,
		0x3ee67cb9, 0xe0252e31, 0x00000002,
	},
};

static bool mt8512_afe_gasrc_found_iir_coeff_table_id(int input_rate,
	int output_rate, int *table_id)
{
	int i;
	const struct mt8512_afe_gasrc_iir_coeff_table_id *table =
		mt8512_afe_gasrc_iir_coeff_table_ids;

	if (!table_id)
		return false;

	/* no need to apply iir for up-sample */
	if (input_rate <= output_rate)
		return false;

	for (i = 0; i < ARRAY_SIZE(mt8512_afe_gasrc_iir_coeff_table_ids); i++) {
		if ((table[i].input_rate == input_rate) &&
			(table[i].output_rate == output_rate)) {
			*table_id = table[i].table_id;
			return true;
		}
	}

	return false;
}

static bool mt8512_afe_gasrc_fill_iir_coeff_table(struct mtk_base_afe *afe,
	int gasrc_id, int table_id)
{
	const unsigned int *table;
	unsigned int ctrl_reg;
	int i;

	if ((table_id < 0) ||
		(table_id >= MT8512_AFE_GASRC_IIR_TABLES))
		return false;

	if (gasrc_id < 0)
		return false;

	dev_dbg(afe->dev, "%s [%d] table_id %d\n",
		__func__, gasrc_id, table_id);

	table = &mt8512_afe_gasrc_iir_coeffs[table_id][0];

	/* enable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_COEFF_SRAM_CTRL,
		GASRC_NEW_CON0_COEFF_SRAM_CTRL);

	/* fill coeffs from addr 0 */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con11;
	regmap_write(afe->regmap, ctrl_reg, 0);

	/* fill all coeffs */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con10;
	for (i = 0; i < IIR_NUMS; i++)
		regmap_write(afe->regmap, ctrl_reg, table[i]);

	/* disable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		GASRC_NEW_CON0_COEFF_SRAM_CTRL, 0);

	return true;
}

static bool mt8512_afe_load_gasrc_iir_coeff_table(struct mtk_base_afe *afe,
	int gasrc_id, int input_rate, int output_rate)
{
	int table_id;

	if (mt8512_afe_gasrc_found_iir_coeff_table_id(input_rate,
			output_rate, &table_id)) {
		return mt8512_afe_gasrc_fill_iir_coeff_table(afe,
			gasrc_id, table_id);
	}

	return false;
}

static void mt8512_afe_adjust_gasrc_cali_cycles(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing, unsigned int rate)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	unsigned int *cali_cycles;

	if (gasrc_id < 0)
		return;

	cali_cycles = &(afe_priv->gasrc_data[gasrc_id].cali_cycles);

	if (fs_timing == MT8512_FS_AMIC_1X_EN_ASYNC) {
		switch (rate) {
		case 8000:
			/* FALLTHROUGH */
		case 16000:
			/* FALLTHROUGH */
		case 32000:
			*cali_cycles = 64;
			break;
		case 48000:
			*cali_cycles = 48;
			break;
		default:
			*cali_cycles = 64;
			break;
		}
	}

	dev_dbg(afe->dev, "%s [%d] cali_cycles %u\n",
		__func__, gasrc_id, *cali_cycles);
}

static int mt8512_afe_configure_gasrc(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int stream = substream->stream;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	struct mt8512_gasrc_data *gasrc_data;
	int mux;
	int input_fs = 0, output_fs = 0;
	int input_rate = 0, output_rate = 0;
	int fix_input_rate = 0, fix_output_rate = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct snd_soc_pcm_runtime *be = substream->private_data;
	struct snd_pcm_substream *paired_substream = NULL;
	bool duplex;
	bool *gasrc_op_freq_45m;
	unsigned int *cali_cycles;

	if (gasrc_id < 0)
		return -EINVAL;

	gasrc_data = &afe_priv->gasrc_data[gasrc_id];

	mux = mt8512_afe_get_gasrc_mux_config(afe, gasrc_id, stream);
	duplex = gasrc_data->duplex;
	gasrc_op_freq_45m = &(gasrc_data->op_freq_45m);
	cali_cycles = &(gasrc_data->cali_cycles);
	fix_input_rate = gasrc_data->fix_input_rate;
	fix_output_rate = gasrc_data->fix_output_rate;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (fix_input_rate) {
			input_fs = mt8512_afe_fs_timing(fix_input_rate);
			input_rate = fix_input_rate;
		} else {
			input_fs = mt8512_afe_fs_timing(runtime->rate);
			input_rate = runtime->rate;
		}
		*gasrc_op_freq_45m = (input_rate % 8000);

		if (fix_output_rate) {
			output_fs = mt8512_afe_fs_timing(fix_output_rate);
			output_rate = fix_output_rate;
		} else {
			if (duplex) {
				paired_substream =
					snd_soc_dpcm_get_substream(be,
					SNDRV_PCM_STREAM_CAPTURE);
				output_fs =
					mt8512_afe_gasrc_get_output_fs(
					paired_substream, dai->id,
					paired_substream->runtime->rate);
				output_rate =
					mt8512_afe_gasrc_get_output_rate(
					paired_substream, dai->id,
					paired_substream->runtime->rate);
			} else {
				output_fs =
					mt8512_afe_gasrc_get_output_fs(
					substream, dai->id, runtime->rate);
				output_rate =
					mt8512_afe_gasrc_get_output_rate(
					substream, dai->id, runtime->rate);
			}
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (fix_output_rate) {
			output_fs = mt8512_afe_fs_timing(fix_output_rate);
			output_rate = fix_output_rate;
		} else {
			output_fs = mt8512_afe_fs_timing(runtime->rate);
			output_rate = runtime->rate;
		}
		*gasrc_op_freq_45m = (output_rate % 8000);

		if (fix_input_rate) {
			input_fs = mt8512_afe_fs_timing(fix_input_rate);
			input_rate = fix_input_rate;
		} else {
			if (duplex) {
				paired_substream =
					snd_soc_dpcm_get_substream(be,
					SNDRV_PCM_STREAM_PLAYBACK);
				input_fs =
					mt8512_afe_gasrc_get_input_fs(
					paired_substream, dai->id,
					paired_substream->runtime->rate);
				input_rate =
					mt8512_afe_gasrc_get_input_rate(
					paired_substream, dai->id,
					paired_substream->runtime->rate);
			} else {
				input_fs =
					mt8512_afe_gasrc_get_input_fs(
					substream, dai->id, runtime->rate);
				input_rate =
					mt8512_afe_gasrc_get_input_rate(
					substream, dai->id, runtime->rate);
			}
		}
	}

	dev_dbg(dai->dev, "%s [%d] %s input_fs 0x%x, output_fs 0x%x\n",
		__func__, gasrc_id, PCM_STREAM_STR(substream->stream),
		input_fs, output_fs);
	dev_dbg(dai->dev, "%s [%d] %s input_rate %d, output_rate %d\n",
		__func__, gasrc_id, PCM_STREAM_STR(substream->stream),
		input_rate, output_rate);
	dev_dbg(dai->dev, "%s [%d] %s gasrc_op_freq_45m %d\n",
		__func__, gasrc_id, PCM_STREAM_STR(substream->stream),
		*gasrc_op_freq_45m);

	if (mt8512_afe_load_gasrc_iir_coeff_table(afe, gasrc_id,
			input_rate, output_rate)) {
		mt8512_afe_gasrc_enable_iir(afe, gasrc_id);
		gasrc_data->iir_on = true;
	} else {
		mt8512_afe_gasrc_disable_iir(afe, gasrc_id);
		gasrc_data->iir_on = false;
	}

	/* INT_ADDA ADC (RX Tracking of 26m) */
	if (stream == SNDRV_PCM_STREAM_CAPTURE)
		mt8512_afe_adjust_gasrc_cali_cycles(afe, dai,
			input_fs, output_rate);

	mt8512_afe_gasrc_set_input_fs(afe, dai, input_fs);
	mt8512_afe_gasrc_set_output_fs(afe, dai, output_fs);

	gasrc_data->cali_tx = false;
	gasrc_data->cali_rx = false;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mt8512_afe_gasrc_is_tx_tracking(output_fs))
			gasrc_data->cali_tx = true;
		else
			gasrc_data->cali_tx = false;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (mt8512_afe_gasrc_is_rx_tracking(input_fs))
			gasrc_data->cali_rx = true;
		else
			gasrc_data->cali_rx = false;

		gasrc_data->cali_tx = false;
	}

	dev_dbg(dai->dev, "%s [%d] %s cali_tx %d, cali_rx %d\n",
		__func__, gasrc_id, PCM_STREAM_STR(substream->stream),
		gasrc_data->cali_tx, gasrc_data->cali_rx);

	switch (mux) {
	case MUX_GASRC_8CH:
	case MUX_GASRC_6CH:
	case MUX_GASRC_4CH:
		gasrc_data->one_heart = true;
		break;
	case MUX_GASRC_2CH:
		gasrc_data->one_heart = false;
		break;
	default:
		gasrc_data->one_heart = false;
		break;
	}

	if (gasrc_data->one_heart && (gasrc_id != MT8512_GASRC0))
		regmap_update_bits(afe->regmap, gasrc_ctrl_reg[gasrc_id].con0,
			GASRC_NEW_CON0_ONE_HEART, GASRC_NEW_CON0_ONE_HEART);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8512_afe_gasrc_set_tx_mode_fs(afe,
			dai, input_rate, output_rate);
		if (gasrc_data->cali_tx) {
			mt8512_afe_gasrc_sel_cali_clk(afe,
				dai, *gasrc_op_freq_45m);
			mt8512_afe_gasrc_sel_lrck(afe, dai, output_fs);
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8512_afe_gasrc_set_rx_mode_fs(afe,
			dai, input_rate, output_rate);
		if (gasrc_data->cali_rx) {
			mt8512_afe_gasrc_sel_cali_clk(afe,
				dai, *gasrc_op_freq_45m);
			mt8512_afe_gasrc_sel_lrck(afe, dai, input_fs);
		}
	}

	if (gasrc_data->cali_tx || gasrc_data->cali_rx) {

		val = (*gasrc_op_freq_45m) ?
			GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_45M :
			GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_49M;
		mask = GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_MASK;
		regmap_update_bits(afe->regmap, gasrc_ctrl_reg[gasrc_id].con7,
			mask, val);

		val = GASRC_NEW_CON6_FREQ_CALI_CYCLE(*cali_cycles) |
				GASRC_NEW_CON6_COMP_FREQ_RES_EN |
				GASRC_NEW_CON6_FREQ_CALI_BP_DGL |
				GASRC_NEW_CON6_CALI_USE_FREQ_OUT |
				GASRC_NEW_CON6_FREQ_CALI_AUTO_RESTART;
		mask = GASRC_NEW_CON6_FREQ_CALI_CYCLE_MASK |
				GASRC_NEW_CON6_COMP_FREQ_RES_EN |
				GASRC_NEW_CON6_FREQ_CALI_BP_DGL |
				GASRC_NEW_CON6_CALI_USE_FREQ_OUT |
				GASRC_NEW_CON6_FREQ_CALI_AUTO_RESTART;

		regmap_update_bits(afe->regmap, gasrc_ctrl_reg[gasrc_id].con6,
			mask, val);
	}

	return 0;
}

static int mt8512_afe_enable_gasrc(struct snd_soc_dai *dai, int stream)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	struct mt8512_gasrc_data *gasrc_data;
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	int ret = 0, counter;
	bool re_enable = false;

	if (gasrc_id < 0)
		return -EINVAL;

	gasrc_data = &afe_priv->gasrc_data[gasrc_id];

	if (gasrc_data->re_enable[stream]) {
		re_enable = true;
		gasrc_data->re_enable[stream] = false;
	}

	counter = atomic_add_return(1, &gasrc_data->ref_cnt);
	if (counter != 1 && !re_enable)
		return 0;

	dev_dbg(dai->dev, "%s [%d] one_heart %d re_enable %d\n",
		__func__, gasrc_id, gasrc_data->one_heart, re_enable);

	if (gasrc_data->cali_tx || gasrc_data->cali_rx) {
		if (gasrc_data->one_heart)
			ctrl_reg = gasrc_ctrl_reg[MT8512_GASRC0].con6;
		else
			ctrl_reg = gasrc_ctrl_reg[gasrc_id].con6;

		val = GASRC_NEW_CON6_CALI_EN;
		regmap_update_bits(afe->regmap, ctrl_reg, val, val);

		val = GASRC_NEW_CON6_AUTO_TUNE_FREQ2 |
				GASRC_NEW_CON6_AUTO_TUNE_FREQ3;
		regmap_update_bits(afe->regmap, ctrl_reg, val, val);
	}

	if (gasrc_data->one_heart)
		ctrl_reg = gasrc_ctrl_reg[MT8512_GASRC0].con0;
	else
		ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	val = GASRC_NEW_CON0_ASM_ON;
	regmap_update_bits(afe->regmap, ctrl_reg, val, val);

	return ret;
}

static int mt8512_afe_disable_gasrc(struct snd_soc_dai *dai, bool directly)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	struct mt8512_gasrc_data *gasrc_data;
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	int ret = 0, counter;

	if (gasrc_id < 0)
		return -EINVAL;

	gasrc_data = &afe_priv->gasrc_data[gasrc_id];

	if (!directly) {
		counter = atomic_sub_return(1, &gasrc_data->ref_cnt);
		if (counter < 0) {
			atomic_add(1, &gasrc_data->ref_cnt);
			return 0;
		} else if (counter > 0)
			return 0;
	}

	dev_dbg(dai->dev, "%s [%d] one_heart %d directly %d\n",
		__func__, gasrc_id, gasrc_data->one_heart, directly);

	if (gasrc_data->one_heart)
		ctrl_reg = gasrc_ctrl_reg[MT8512_GASRC0].con0;
	else
		ctrl_reg = gasrc_ctrl_reg[gasrc_id].con0;

	val = GASRC_NEW_CON0_ASM_ON;
	regmap_update_bits(afe->regmap, ctrl_reg, val, 0);

	if (gasrc_data->cali_tx || gasrc_data->cali_rx) {
		if (gasrc_data->one_heart)
			ctrl_reg = gasrc_ctrl_reg[MT8512_GASRC0].con6;
		else
			ctrl_reg = gasrc_ctrl_reg[gasrc_id].con6;

		val = GASRC_NEW_CON6_CALI_EN;
		regmap_update_bits(afe->regmap, ctrl_reg, val, 0);
	}

	if (gasrc_data->iir_on)
		mt8512_afe_gasrc_disable_iir(afe, gasrc_id);

	return ret;
}

static int mt8512_afe_gasrc_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);

	dev_info(dai->dev, "%s [%d] %s\n", __func__,
		gasrc_id, PCM_STREAM_STR(substream->stream));

	mt8512_afe_enable_main_clk(afe);

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_L]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_M]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_H]);

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_GASRC0);
		break;
	case MT8512_GASRC1:
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_GASRC1);
		break;
	case MT8512_GASRC2:
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_GASRC2);
		break;
	case MT8512_GASRC3:
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_GASRC3);
		break;
	default:
		break;
	}

	return 0;
}

static void mt8512_afe_gasrc_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	struct mt8512_gasrc_data *gasrc_data;

	if (gasrc_id < 0)
		return;

	gasrc_data = &afe_priv->gasrc_data[gasrc_id];

	dev_info(dai->dev, "%s [%d] %s\n", __func__,
		gasrc_id, PCM_STREAM_STR(substream->stream));

	gasrc_data->re_enable[substream->stream] = false;

	switch (gasrc_id) {
	case MT8512_GASRC0:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_GASRC0);
		break;
	case MT8512_GASRC1:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_GASRC1);
		break;
	case MT8512_GASRC2:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_GASRC2);
		break;
	case MT8512_GASRC3:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_GASRC3);
		break;
	default:
		break;
	}

	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_H]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_M]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_FASM_L]);

	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_gasrc_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const int gasrc_id = mt8512_dai_num_to_gasrc(dai->id);
	struct mt8512_gasrc_data *gasrc_data;
	int counter;

	if (gasrc_id < 0)
		return -EINVAL;

	gasrc_data = &afe_priv->gasrc_data[gasrc_id];

	gasrc_data->duplex = false;
	gasrc_data->op_freq_45m = false;
	gasrc_data->cali_cycles = 64;
	gasrc_data->re_enable[substream->stream] = false;

	counter = atomic_read(&gasrc_data->ref_cnt);

	if (dai->capture_active && dai->playback_active)
		gasrc_data->duplex = true;

	dev_info(dai->dev, "%s [%d] %s duplex %d\n", __func__,
		gasrc_id, PCM_STREAM_STR(substream->stream),
		gasrc_data->duplex);

	if (gasrc_data->duplex && counter > 0) {
		mt8512_afe_disable_gasrc(dai, true);
		gasrc_data->re_enable[substream->stream] = true;
	}

	mt8512_afe_reset_gasrc(afe, dai);
	mt8512_afe_clear_gasrc(afe, dai);
	mt8512_afe_gasrc_use_sel(afe, dai, true);
	mt8512_afe_configure_gasrc(afe, substream, dai);

	return 0;
}

static int mt8512_afe_gasrc_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;

	dev_dbg(dai->dev, "%s [%d] %s cmd %d\n", __func__,
		mt8512_dai_num_to_gasrc(dai->id),
		PCM_STREAM_STR(substream->stream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mt8512_afe_enable_gasrc(dai, substream->stream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mt8512_afe_disable_gasrc(dai, false);
		break;
	default:
		break;
	}

	return ret;
}

static int mt8512_afe_configure_multi_in(struct mtk_base_afe *afe,
	unsigned int channels,
	unsigned int bit_width,
	unsigned int period_size,
	bool spdif_input)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_multi_in_data *multi_in = &afe_priv->multi_in_data;
	unsigned int val;
	unsigned int period_bytes;

	val = AFE_MPHONE_MULTI_CON0_SDATA0_SEL(0) |
	      AFE_MPHONE_MULTI_CON0_SDATA1_SEL(1) |
	      AFE_MPHONE_MULTI_CON0_SDATA2_SEL(2) |
	      AFE_MPHONE_MULTI_CON0_SDATA3_SEL(3);

	if (spdif_input) {
		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;

			period_bytes = period_size * 4 * channels;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;

			period_bytes = period_size * 3 * channels;
		} else {
			val |= AFE_MPHONE_MULTI_CON0_16BIT_DATA |
			       AFE_MPHONE_MULTI_CON0_16BIT_SWAP;

			period_bytes = period_size * 4 * channels;
		}
	} else {
		if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON0_24BIT_DATA;

			period_bytes = period_size * 3 * channels;
		} else {
			val |= AFE_MPHONE_MULTI_CON0_16BIT_DATA;

			period_bytes = period_size * 4 * channels;
		}
	}

	if (period_bytes >= 1024) {
		val |= AFE_MPHONE_MULTI_CON0_256DWORD_PERIOD;

		multi_in->period_update_bytes = 1024;
	} else if (period_bytes >= 512) {
		val |= AFE_MPHONE_MULTI_CON0_128DWORD_PERIOD;

		multi_in->period_update_bytes = 512;
	} else if (period_bytes >= 256) {
		val |= AFE_MPHONE_MULTI_CON0_64DWORD_PERIOD;

		multi_in->period_update_bytes = 256;
	} else {
		val |= AFE_MPHONE_MULTI_CON0_32DWORD_PERIOD;

		multi_in->period_update_bytes = 128;
	}

	multi_in->notify_irq_count = period_bytes /
		multi_in->period_update_bytes;

	regmap_update_bits(afe->regmap, AFE_MPHONE_MULTI_CON0,
			   AFE_MPHONE_MULTI_CON0_SET_MASK, val);

	val = AFE_MPHONE_MULTI_CON1_CH_NUM(channels) |
	      AFE_MPHONE_MULTI_CON1_BIT_NUM(bit_width) |
	      AFE_MPHONE_MULTI_CON1_SYNC_ON;

	if (spdif_input) {
		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
			       AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON1_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
			       AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		}
	} else {
		val |= AFE_MPHONE_MULTI_CON1_HBR_MODE;

		if (bit_width == 32) {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else if (bit_width == 24) {
			val |= AFE_MPHONE_MULTI_CON1_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_24BIT_SWAP_BYPASS |
			       AFE_MPHONE_MULTI_CON1_LRCK_32_CYCLE;
		} else {
			val |= AFE_MPHONE_MULTI_CON1_NON_COMPACT_MODE |
			       AFE_MPHONE_MULTI_CON1_LRCK_16_CYCLE;
		}

		if (multi_in->format == MT8512_MULTI_IN_FORMAT_I2S) {
			val |= AFE_MPHONE_MULTI_CON1_DELAY_DATA |
			       AFE_MPHONE_MULTI_CON1_LEFT_ALIGN;
		} else if (multi_in->format == MT8512_MULTI_IN_FORMAT_LJ) {
			val |= AFE_MPHONE_MULTI_CON1_LEFT_ALIGN |
			       AFE_MPHONE_MULTI_CON1_LRCK_INV;
		} else if (multi_in->format == MT8512_MULTI_IN_FORMAT_RJ) {
			val |= AFE_MPHONE_MULTI_CON1_LRCK_INV;
		}

		if (multi_in->bck_inv)
			val ^= AFE_MPHONE_MULTI_CON1_BCK_INV;

		if (multi_in->lrck_inv)
			val ^= AFE_MPHONE_MULTI_CON1_LRCK_INV;
	}

	regmap_update_bits(afe->regmap, AFE_MPHONE_MULTI_CON1,
			   AFE_MPHONE_MULTI_CON1_SET_MASK, val);

	multi_in->current_irq_count = 0;

	return 0;
}

static int mt8512_afe_enable_multi_in(struct mtk_base_afe *afe)
{
	regmap_update_bits(afe->regmap, AFE_MPHONE_MULTI_CON0,
			   AFE_MPHONE_MULTI_CON0_EN,
			   AFE_MPHONE_MULTI_CON0_EN);

	return 0;
}

static int mt8512_afe_disable_multi_in(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_MPHONE_MULTI_CON0,
			   AFE_MPHONE_MULTI_CON0_EN, 0x0);

	afe_priv->multi_in_data.current_irq_count = 0;

	return 0;
}

static int mt8512_afe_spdif_in_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_enable_main_clk(afe);

	return 0;
}

static void mt8512_afe_spdif_in_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_spdif_in_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	dev_dbg(dai->dev, "%s ch:%d bits:%d period_size:%lu\n",
		__func__, dai->channels, dai->sample_bits,
		runtime->period_size);

	ret = mt8512_afe_configure_multi_in(afe,
					    dai->channels,
					    dai->sample_bits,
					    runtime->period_size,
					    true);

	return ret;
}

static int mt8512_afe_spdif_in_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s cmd %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8512_afe_enable_multi_in(afe);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8512_afe_disable_multi_in(afe);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8512_afe_multi_in_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_enable_main_clk(afe);

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 128);

	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_MULTI_IN);

	return 0;
}

static void mt8512_afe_multi_in_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s\n", __func__);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_MULTI_IN);
	mt8512_afe_disable_main_clk(afe);
}

static int mt8512_afe_multi_in_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int ret = 0;

	dev_dbg(dai->dev, "%s ch:%d bits:%d period_size:%lu\n",
		__func__, dai->channels, dai->sample_bits,
		runtime->period_size);

	ret = mt8512_afe_configure_multi_in(afe,
					    dai->channels,
					    dai->sample_bits,
					    runtime->period_size,
					    false);

	return ret;
}

static int mt8512_afe_multi_in_set_fmt(struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dai->dev, "%s fmt 0x%x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		afe_priv->multi_in_data.format = MT8512_MULTI_IN_FORMAT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		afe_priv->multi_in_data.format = MT8512_MULTI_IN_FORMAT_LJ;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		afe_priv->multi_in_data.format = MT8512_MULTI_IN_FORMAT_RJ;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		afe_priv->multi_in_data.bck_inv = false;
		afe_priv->multi_in_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		afe_priv->multi_in_data.bck_inv = false;
		afe_priv->multi_in_data.lrck_inv = true;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		afe_priv->multi_in_data.bck_inv = true;
		afe_priv->multi_in_data.lrck_inv = false;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		afe_priv->multi_in_data.bck_inv = true;
		afe_priv->multi_in_data.lrck_inv = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8512_afe_multi_in_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s cmd %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8512_afe_enable_multi_in(afe);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8512_afe_disable_multi_in(afe);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mt8512_afe_handle_etdm_force_on(struct mtk_base_afe *afe,
	bool probe);

static int mt8512_afe_handle_etdm_force_off(struct mtk_base_afe *afe);

static int mt8512_afe_dai_suspend(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	struct mt8512_adsp_data *adsp_data = &(afe_priv->adsp_data);
#endif

	dev_dbg(afe->dev, "%s, dai %d suspend %d >>\n", __func__,
		dai->id, afe->suspended);

	if (afe->suspended)
		return 0;

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	/* if ADSP HOSTLESS is active, not do register coverage */
	if (adsp_data->adsp_on && adsp_data->hostless_active()) {
		mt8512_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS],
			afe_priv->clocks[MT8512_CLK_CLK26M]);
		afe->suspended = true;
		return 0;
	}
#endif

	mt8512_afe_handle_etdm_force_off(afe);

	mt8512_afe_enable_main_clk(afe);

	/* do afe suspend */
	mtk_afe_dai_suspend(dai);

	mt8512_afe_disable_main_clk(afe);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);

	dev_dbg(afe->dev, "%s dai %d <<\n", __func__, dai->id);

	return 0;
}

static int mt8512_afe_dai_resume(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	struct mt8512_adsp_data *adsp_data = &(afe_priv->adsp_data);
#endif

	dev_dbg(afe->dev, "%s dai %d suspend %d >>\n", __func__,
		dai->id, afe->suspended);

	if (!afe->suspended)
		return 0;

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	/* if ADSP HOSTLESS is active, not do register coverage */
	if (adsp_data->adsp_on && adsp_data->hostless_active()) {
		mt8512_afe_set_clk_parent(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS],
			afe_priv->clocks[MT8512_CLK_AUD_SYSPLL1_D4]);
		afe->suspended = false;
		return 0;
	}
#endif

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);

	mt8512_afe_enable_main_clk(afe);

	/* do afe suspend */
	mtk_afe_dai_resume(dai);

	mt8512_afe_disable_main_clk(afe);

	mt8512_afe_handle_etdm_force_on(afe, false);

	dev_dbg(afe->dev, "%s dai %d<<\n", __func__, dai->id);

	return 0;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt8512_afe_fe_dai_ops = {
	.startup	= mt8512_afe_fe_startup,
	.shutdown	= mt8512_afe_fe_shutdown,
	.hw_params	= mt8512_afe_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
	.prepare	= mt8512_afe_fe_prepare,
	.trigger	= mt8512_afe_fe_trigger,
	.set_fmt	= mt8512_afe_fe_set_fmt,
};

/* BE DAIs */
static const struct snd_soc_dai_ops mt8512_afe_etdm1_ops = {
	.startup	= mt8512_afe_etdm1_startup,
	.shutdown	= mt8512_afe_etdm1_shutdown,
	.prepare	= mt8512_afe_etdm1_prepare,
	.set_fmt	= mt8512_afe_etdm_set_fmt,
	.set_tdm_slot	= mt8512_afe_etdm_set_tdm_slot,
	.set_sysclk	= mt8512_afe_etdm_set_sysclk,
};

static const struct snd_soc_dai_ops mt8512_afe_etdm2_ops = {
	.startup	= mt8512_afe_etdm2_startup,
	.shutdown	= mt8512_afe_etdm2_shutdown,
	.prepare	= mt8512_afe_etdm2_prepare,
	.set_fmt	= mt8512_afe_etdm_set_fmt,
	.set_tdm_slot	= mt8512_afe_etdm_set_tdm_slot,
	.set_sysclk	= mt8512_afe_etdm_set_sysclk,
};

static const struct snd_soc_dai_ops mt8512_afe_pcm1_ops = {
	.startup	= mt8512_afe_pcm1_startup,
	.shutdown	= mt8512_afe_pcm1_shutdown,
	.prepare	= mt8512_afe_pcm1_prepare,
	.set_fmt	= mt8512_afe_pcm1_set_fmt,
};

static const struct snd_soc_dai_ops mt8512_afe_dmic_ops = {
	.startup	= mt8512_afe_dmic_startup,
	.shutdown	= mt8512_afe_dmic_shutdown,
	.prepare	= mt8512_afe_dmic_prepare,
};

static const struct snd_soc_dai_ops mt8512_afe_int_adda_ops = {
	.startup	= mt8512_afe_int_adda_startup,
	.shutdown	= mt8512_afe_int_adda_shutdown,
	.prepare	= mt8512_afe_int_adda_prepare,
};

static const struct snd_soc_dai_ops mt8512_afe_gasrc_ops = {
	.startup	= mt8512_afe_gasrc_startup,
	.shutdown	= mt8512_afe_gasrc_shutdown,
	.prepare	= mt8512_afe_gasrc_prepare,
	.trigger	= mt8512_afe_gasrc_trigger,
};

static const struct snd_soc_dai_ops mt8512_afe_spdif_in_ops = {
	.startup	= mt8512_afe_spdif_in_startup,
	.shutdown	= mt8512_afe_spdif_in_shutdown,
	.prepare	= mt8512_afe_spdif_in_prepare,
	.trigger	= mt8512_afe_spdif_in_trigger,
};

static const struct snd_soc_dai_ops mt8512_afe_multi_in_ops = {
	.startup	= mt8512_afe_multi_in_startup,
	.shutdown	= mt8512_afe_multi_in_shutdown,
	.prepare	= mt8512_afe_multi_in_prepare,
	.set_fmt	= mt8512_afe_multi_in_set_fmt,
	.trigger	= mt8512_afe_multi_in_trigger,
};

static struct snd_soc_dai_driver mt8512_afe_pcm_dais[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DLM",
		.id = MT8512_AFE_MEMIF_DLM,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.playback = {
			.stream_name = "DLM",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "DL2",
		.id = MT8512_AFE_MEMIF_DL2,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "DL3",
		.id = MT8512_AFE_MEMIF_DL3,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "DL6",
		.id = MT8512_AFE_MEMIF_DL6,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL1",
		.id = MT8512_AFE_MEMIF_UL1,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL2",
		.id = MT8512_AFE_MEMIF_UL2,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL3",
		.id = MT8512_AFE_MEMIF_UL3,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL4",
		.id = MT8512_AFE_MEMIF_UL4,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL5",
		.id = MT8512_AFE_MEMIF_UL5,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL8",
		.id = MT8512_AFE_MEMIF_UL8,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL9",
		.id = MT8512_AFE_MEMIF_UL9,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL9",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
		.name = "UL10",
		.id = MT8512_AFE_MEMIF_UL10,
		.suspend = mt8512_afe_dai_suspend,
		.resume = mt8512_afe_dai_resume,
		.capture = {
			.stream_name = "UL10",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_fe_dai_ops,
	}, {
	/* BE DAIs */
		.name = "ETDM1_IN",
		.id = MT8512_AFE_IO_ETDM1_IN,
		.capture = {
			.stream_name = "ETDM1 Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_etdm1_ops,
	}, {
	/* BE DAIs */
		.name = "ETDM2_OUT",
		.id = MT8512_AFE_IO_ETDM2_OUT,
		.playback = {
			.stream_name = "ETDM2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_etdm2_ops,
	}, {
	/* BE DAIs */
		.name = "ETDM2_IN",
		.id = MT8512_AFE_IO_ETDM2_IN,
		.capture = {
			.stream_name = "ETDM2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_etdm2_ops,
	}, {
	/* BE DAIs */
		.name = "PCM1",
		.id = MT8512_AFE_IO_PCM1,
		.playback = {
			.stream_name = "PCM1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "PCM1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_pcm1_ops,
		.symmetric_rates = 1,
		.symmetric_samplebits = 1,
	}, {
	/* BE DAIs */
		.name = "VIRTUAL_DL_SRC",
		.id = MT8512_AFE_IO_VIRTUAL_DL_SRC,
		.capture = {
			.stream_name = "VIRTUAL_DL_SRC",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
	}, {
	/* BE DAIs */
		.name = "DMIC",
		.id = MT8512_AFE_IO_DMIC,
		.capture = {
			.stream_name = "DMIC Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_dmic_ops,
	}, {
	/* BE DAIs */
		.name = "INT ADDA",
		.id = MT8512_AFE_IO_INT_ADDA,
		.playback = {
			.stream_name = "INT ADDA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "INT ADDA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_int_adda_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC0",
		.id = MT8512_AFE_IO_GASRC0,
		.playback = {
			.stream_name = "GASRC0 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC0 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC1",
		.id = MT8512_AFE_IO_GASRC1,
		.playback = {
			.stream_name = "GASRC1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC2",
		.id = MT8512_AFE_IO_GASRC2,
		.playback = {
			.stream_name = "GASRC2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "GASRC3",
		.id = MT8512_AFE_IO_GASRC3,
		.playback = {
			.stream_name = "GASRC3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "GASRC3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_gasrc_ops,
	}, {
	/* BE DAIs */
		.name = "SPDIF_IN",
		.id = MT8512_AFE_IO_SPDIF_IN,
		.capture = {
			.stream_name = "SPDIF Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_88200 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_176400 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_spdif_in_ops,
	}, {
	/* BE DAIs */
		.name = "MULTI_IN",
		.id = MT8512_AFE_IO_MULTI_IN,
		.capture = {
			.stream_name = "MULTI Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_3LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_afe_multi_in_ops,
	},
};

static int mt8512_snd_soc_dapm_get_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	int ret;

	mt8512_afe_enable_reg_rw_clk(afe);

	ret = snd_soc_dapm_get_enum_double(kcontrol, ucontrol);

	mt8512_afe_disable_reg_rw_clk(afe);

	return ret;
}

static int mt8512_snd_soc_dapm_put_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	int ret;

	mt8512_afe_enable_reg_rw_clk(afe);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

	mt8512_afe_disable_reg_rw_clk(afe);

	return ret;
}

static const struct snd_kcontrol_new mt8512_afe_o00_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN0, 0, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o01_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN1, 1, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o02_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I02 Switch", AFE_CONN2, 2, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o03_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN3, 3, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o04_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN4, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN4, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN4, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN4, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN4, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN4_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN4_1, 8, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o05_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN5, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN5, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN5, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN5, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN5, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN5_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN5_1, 9, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o06_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN6, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I34 Switch", AFE_CONN6_1, 2, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o07_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN7, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I35 Switch", AFE_CONN7_1, 3, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o08_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN8, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I36 Switch", AFE_CONN8_1, 4, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o09_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN9, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I37 Switch", AFE_CONN9_1, 5, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN10, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN10_1, 6, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN11, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN11_1, 7, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o12_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN12, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN12, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN12, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN12_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I60 Switch", AFE_CONN12_1, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o13_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN13, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN13, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN13, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN13_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I61 Switch", AFE_CONN13_1, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN14, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN14, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN14_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN14_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I60 Switch", AFE_CONN14_1, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN15, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN15, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN15_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN15_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I61 Switch", AFE_CONN15_1, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN16, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN16, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN16_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN16_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I60 Switch", AFE_CONN16_1, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o17_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN17, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN17, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN17_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN17_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I61 Switch", AFE_CONN17_1, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o18_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN18, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN18, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN18_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I60 Switch", AFE_CONN18_1, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o19_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN19, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN19, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN19_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I61 Switch", AFE_CONN19_1, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o20_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN20, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN20, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN20, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN20, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN20, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN20, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN20, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN20_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN20_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN20_1, 9, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o21_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN21, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN21, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN21, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN21, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN21, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN21, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN21, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN21_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN21_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN21_1, 9, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o26_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN26, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN26, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN26_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN26_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I60 Switch", AFE_CONN26_1, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o27_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN27, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN27, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN27_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN27_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I61 Switch", AFE_CONN27_1, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o28_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN28, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN28, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN28, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN28_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I34 Switch", AFE_CONN28_1, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN28_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I44 Switch", AFE_CONN28_1, 12, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o29_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN29, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN29, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN29, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN29_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I35 Switch", AFE_CONN29_1, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN29_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I45 Switch", AFE_CONN29_1, 13, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o30_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN30, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN30, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN30, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN30_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I36 Switch", AFE_CONN30_1, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN30_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN30_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I46 Switch", AFE_CONN30_1, 14, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o31_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN31, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN31, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN31, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN31_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I37 Switch", AFE_CONN31_1, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN31_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN31_1, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I47 Switch", AFE_CONN31_1, 15, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o32_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN32, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN32, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN32, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN32, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN32_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN32_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN32_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I48 Switch", AFE_CONN32_1, 16, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o33_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN33, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN33, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN33, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN33, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN33_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN33_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN33_1, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I49 Switch", AFE_CONN33_1, 17, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o34_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN34, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN34, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN34, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN34, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN34_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN34_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I50 Switch", AFE_CONN34_1, 18, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o35_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN35, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN35, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN35, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN35, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I33 Switch", AFE_CONN35_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN35_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I51 Switch", AFE_CONN35_1, 19, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o36_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN36, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN36, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I34 Switch", AFE_CONN36_1, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN36_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I52 Switch", AFE_CONN36_1, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN36, 24, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o37_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN37, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN37, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I35 Switch", AFE_CONN37_1, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN37_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I53 Switch", AFE_CONN37_1, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN37, 25, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o38_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN38, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN38, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I36 Switch", AFE_CONN38_1, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I54 Switch", AFE_CONN38_1, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN38, 26, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o39_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN39, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN39, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I37 Switch", AFE_CONN39_1, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I55 Switch", AFE_CONN39_1, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN39, 27, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o40_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN40, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN40, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I38 Switch", AFE_CONN40_1, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I56 Switch", AFE_CONN40_1, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN40, 28, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o41_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN41, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN41, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I39 Switch", AFE_CONN41_1, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I57 Switch", AFE_CONN41_1, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN41, 29, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o42_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN42, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN42, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN42, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN42, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN42, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN42, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN42_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN42_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I45 Switch", AFE_CONN42_1, 13, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o43_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN43, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN43, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN43, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN43, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN43, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN43, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN43_1, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN43_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I44 Switch", AFE_CONN43_1, 12, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o44_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN44, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN44, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN44, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN44, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I24 Switch", AFE_CONN44, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I43 Switch", AFE_CONN44_1, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I44 Switch", AFE_CONN44_1, 12, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o45_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN45, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN45, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN45, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN45, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I25 Switch", AFE_CONN45, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I42 Switch", AFE_CONN45_1, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I45 Switch", AFE_CONN45_1, 13, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o46_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN46, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN46, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN46, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN46, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN46, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I46 Switch", AFE_CONN46_1, 14, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o47_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN47, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN47, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN47, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN47, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I27 Switch", AFE_CONN47, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I47 Switch", AFE_CONN47_1, 15, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o48_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I10 Switch", AFE_CONN48, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN48, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN48, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN48, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I28 Switch", AFE_CONN48, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I40 Switch", AFE_CONN48_1, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I48 Switch", AFE_CONN48_1, 16, 1, 0),
};

static const struct snd_kcontrol_new mt8512_afe_o49_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I11 Switch", AFE_CONN49, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN49, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN49, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN49, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I29 Switch", AFE_CONN49, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I41 Switch", AFE_CONN49_1, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I49 Switch", AFE_CONN49_1, 17, 1, 0),
};

static const char * const ul3_mux_text[] = {
	"Interconn", "Direct",
};

static SOC_ENUM_SINGLE_DECL(ul3_mux_enum,
	ETDM_COWORK_CON1, 24, ul3_mux_text);

static const struct snd_kcontrol_new ul3_mux =
	SOC_DAPM_ENUM_EXT("UL3 Source", ul3_mux_enum,
		mt8512_snd_soc_dapm_get_enum_double,
		mt8512_snd_soc_dapm_put_enum_double);

static const char * const ul1_mux_text[] = {
	"Spdif_In", "Multi_In",
};

static SOC_ENUM_SINGLE_VIRT_DECL(ul1_mux_enum, ul1_mux_text);

static const struct snd_kcontrol_new ul1_mux =
	SOC_DAPM_ENUM("UL1 Source", ul1_mux_enum);

static const char * const mt8512_afe_gasrc_text_1[] = {
	"Gasrc_8ch", "Gasrc_6ch", "Gasrc_4ch", "Gasrc_2ch",
};

static const char * const mt8512_afe_gasrc_text_2[] = {
	"Gasrc_8ch", "Gasrc_6ch", "Gasrc_2ch",
};

static const char * const mt8512_afe_gasrc_text_3[] = {
	"Gasrc_8ch", "Gasrc_2ch",
};

static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc0_input_enum,
	mt8512_afe_gasrc_text_1);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc1_input_enum,
	mt8512_afe_gasrc_text_1);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc2_input_enum,
	mt8512_afe_gasrc_text_2);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc3_input_enum,
	mt8512_afe_gasrc_text_3);

static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc0_output_enum,
	mt8512_afe_gasrc_text_1);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc1_output_enum,
	mt8512_afe_gasrc_text_1);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc2_output_enum,
	mt8512_afe_gasrc_text_2);
static SOC_ENUM_SINGLE_VIRT_DECL(mt8512_afe_gasrc3_output_enum,
	mt8512_afe_gasrc_text_3);

static int mt8512_afe_gasrc_string_to_id(const char *name)
{
	int gasrc_id = -1;

	if (strstr(name, "GASRC0"))
		gasrc_id = MT8512_GASRC0;
	else if (strstr(name, "GASRC1"))
		gasrc_id = MT8512_GASRC1;
	else if (strstr(name, "GASRC2"))
		gasrc_id = MT8512_GASRC2;
	else if (strstr(name, "GASRC3"))
		gasrc_id = MT8512_GASRC3;

	return gasrc_id;
}

static int mt8512_afe_gasrc_source_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const char *name = kcontrol->id.name;
	int gasrc_id = mt8512_afe_gasrc_string_to_id(name);

	if (gasrc_id < 0)
		return -EINVAL;

	ucontrol->value.enumerated.item[0] =
		afe_priv->gasrc_data[gasrc_id].input_mux;

	return 0;
}

static int mt8512_afe_gasrc_source_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	const char *name = kcontrol->id.name;
	int gasrc_id = mt8512_afe_gasrc_string_to_id(name);

	if (gasrc_id < 0)
		return -EINVAL;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	afe_priv->gasrc_data[gasrc_id].input_mux =
		ucontrol->value.enumerated.item[0];

	return 0;
}

static int mt8512_afe_gasrc_sink_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	const char *name = kcontrol->id.name;
	int gasrc_id = mt8512_afe_gasrc_string_to_id(name);

	if (gasrc_id < 0)
		return -EINVAL;

	ucontrol->value.enumerated.item[0] =
		afe_priv->gasrc_data[gasrc_id].output_mux;

	return 0;
}

static int mt8512_afe_gasrc_sink_mux_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *comp = snd_soc_dapm_to_component(dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(comp);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	const char *name = kcontrol->id.name;
	int gasrc_id = mt8512_afe_gasrc_string_to_id(name);

	if (gasrc_id < 0)
		return -EINVAL;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	afe_priv->gasrc_data[gasrc_id].output_mux =
		ucontrol->value.enumerated.item[0];

	return 0;
}

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
static void mt8512_adsp_get_afe_memif_sram(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int *paddr,
				       unsigned int *size)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_fe_dai_data *fe_data = &afe_priv->fe_data[memif_id];
	*paddr = fe_data->sram_phy_addr;
	*size = fe_data->sram_size;
}

static int mt8512_adsp_set_afe_memif(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int channels,
				       snd_pcm_format_t format)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	struct mt8512_control_data *ctrl_data = &afe_priv->ctrl_data;
	int fs, hd_audio = 0;

	if (memif_id == MT8512_AFE_MEMIF_UL2) {
		unsigned int val = 0;

		if (!ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_O34_O41_1X_EN_MASK,
				   ASYS_TCON_O34_O41_1X_EN_UL2);

			val |= UL_REORDER_START_DATA(8) |
			       UL_REORDER_CHANNEL(channels) |
			       UL_REORDER_NO_BYPASS;
		}

		regmap_update_bits(afe->regmap, AFE_I2S_UL2_REORDER,
				   UL_REORDER_CTRL_MASK, val);
	} else if (memif_id == MT8512_AFE_MEMIF_UL9) {
		unsigned int val = 0;

		if (!ctrl_data->bypass_cm0) {
			regmap_update_bits(afe->regmap, ASYS_TOP_CON,
				   ASYS_TCON_O26_O33_1X_EN_MASK,
				   ASYS_TCON_O26_O33_1X_EN_UL9);

			if (channels > 8)
				regmap_update_bits(afe->regmap, ASYS_TOP_CON,
					   ASYS_TCON_O34_O41_1X_EN_MASK,
					   ASYS_TCON_O34_O41_1X_EN_UL9);

			val |= UL_REORDER_START_DATA(0) |
			       UL_REORDER_CHANNEL(channels) |
			       UL_REORDER_NO_BYPASS;
		}

		regmap_update_bits(afe->regmap, AFE_I2S_UL9_REORDER,
				   UL_REORDER_CTRL_MASK, val);
	}

	/* start */
	regmap_write(afe->regmap, memif->data->reg_ofs_base,
			 memif->phys_buf_addr);
	/* end */
	regmap_write(afe->regmap,
			 memif->data->reg_ofs_base + AFE_BASE_END_OFFSET,
			 memif->phys_buf_addr + memif->buffer_size - 1);

	/* set MSB to 33-bit, fix 33-bit to 0 for adsp */
	regmap_update_bits(afe->regmap, memif->data->msb_reg,
			       1 << memif->data->msb_shift,
			       0 << memif->data->msb_shift);

	/* set channel */
	if (memif->data->ch_config_shift >= 0)
		regmap_update_bits(afe->regmap,
			memif->data->ch_config_reg,
			0x1f << memif->data->ch_config_shift,
			channels << memif->data->ch_config_shift);

	if (memif->data->mono_shift >= 0) {
		unsigned int mono = (channels == 1) ? 1 : 0;

		regmap_update_bits(afe->regmap, memif->data->mono_reg,
				       1 << memif->data->mono_shift,
				       mono << memif->data->mono_shift);
	}

	/* set rate */
	if (memif->data->fs_shift < 0)
		return 0;

	fs = mt8512_afe_fs_timing(rate);

	if (fs < 0)
		return -EINVAL;

	if (memif->data->id == MT8512_AFE_MEMIF_UL8)
		fs = MT8512_FS_ETDMIN1_NX_EN;

	regmap_update_bits(afe->regmap, memif->data->fs_reg,
			       memif->data->fs_maskbit << memif->data->fs_shift,
			       fs << memif->data->fs_shift);

	/* set hd mode */
	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		hd_audio = 0;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		hd_audio = 1;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		hd_audio = 1;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		hd_audio = 1;
		break;
	default:
		dev_info(afe->dev, "%s() error: unsupported format %d\n",
			__func__, format);
		break;
	}

	if (memif->data->hd_reg >= 0)
		regmap_update_bits(afe->regmap, memif->data->hd_reg,
				       1 << memif->data->hd_shift,
				       hd_audio << memif->data->hd_shift);

	mt8512_afe_irq_direction_enable(afe,
		memif->irq_usage,
		MT8512_AFE_IRQ_DIR_DSP);

	return 0;
}

static int mt8512_adsp_set_afe_memif_enable(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int period_size,
				       int enable)
{
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_control_data *ctrl_data = &afe_priv->ctrl_data;
	unsigned int counter = period_size;
	int fs;

	dev_info(afe->dev, "%s memif %d %s\n", __func__, memif_id,
		enable?"enable":"disable");

	/* TODO IRQ which side? */
	if (enable) {
		if (memif->data->agent_disable_shift >= 0)
			regmap_update_bits(afe->regmap,
				memif->data->agent_disable_reg,
				1 << memif->data->agent_disable_shift,
				0 << memif->data->agent_disable_shift);

		/* enable channel merge */
		if (memif_id == MT8512_AFE_MEMIF_UL2 &&
		    !ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL2_REORDER,
				UL_REORDER_EN, UL_REORDER_EN);
		} else if (memif_id == MT8512_AFE_MEMIF_UL9 &&
			   !ctrl_data->bypass_cm0) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL9_REORDER,
				UL_REORDER_EN, UL_REORDER_EN);
		}

		/* set irq counter */
		if (irq_data->irq_cnt_reg >= 0)
			regmap_update_bits(afe->regmap,
				irq_data->irq_cnt_reg,
				irq_data->irq_cnt_maskbit
				<< irq_data->irq_cnt_shift,
				counter << irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = mt8512_afe_fs_timing(rate);

		if (fs < 0)
			return -EINVAL;

		if (irq_data->irq_fs_reg >= 0)
			regmap_update_bits(afe->regmap,
				irq_data->irq_fs_reg,
				irq_data->irq_fs_maskbit
				<< irq_data->irq_fs_shift,
				fs << irq_data->irq_fs_shift);
	} else {
		if (memif_id == MT8512_AFE_MEMIF_UL2 &&
		    !ctrl_data->bypass_cm1) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL2_REORDER,
				UL_REORDER_EN, 0x0);
		} else if (memif_id == MT8512_AFE_MEMIF_UL9 &&
			   !ctrl_data->bypass_cm0) {
			regmap_update_bits(afe->regmap,
				AFE_I2S_UL9_REORDER,
				UL_REORDER_EN, 0x0);
		}

		if (memif->data->agent_disable_shift >= 0)
			regmap_update_bits(afe->regmap,
				memif->data->agent_disable_reg,
				1 << memif->data->agent_disable_shift,
				1 << memif->data->agent_disable_shift);
	}

	return 0;
}

static void mt8512_adsp_set_afe_init(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s\n", __func__);

	/* enable audio power always on */
	device_init_wakeup(afe->dev, true);

	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_enable_main_clk(afe);
}

static void mt8512_adsp_set_afe_uninit(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s\n", __func__);

	mt8512_afe_disable_main_clk(afe);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);

	/* disable audio power always on */
	device_init_wakeup(afe->dev, false);
}

static void mt8512_adsp_set_afe_interconn(struct mtk_base_afe *afe, int enable)
{
	dev_info(afe->dev, "%s\n", __func__);

#if 0
	if (enable) {
		regmap_update_bits(afe->regmap,
			AFE_CONN4_1,
			0x100,
			0x100);
		regmap_update_bits(afe->regmap,
			AFE_CONN5_1,
			0x100,
			0x100);
		regmap_update_bits(afe->regmap,
			AFE_CONN32_1,
			0x100,
			0x100);
		regmap_update_bits(afe->regmap,
			AFE_CONN33_1,
			0x100,
			0x100);
	} else {
		regmap_update_bits(afe->regmap,
			AFE_CONN4_1,
			0x100,
			0x0);
		regmap_update_bits(afe->regmap,
			AFE_CONN5_1,
			0x100,
			0x0);
		regmap_update_bits(afe->regmap,
			AFE_CONN32_1,
			0x100,
			0x0);
		regmap_update_bits(afe->regmap,
			AFE_CONN33_1,
			0x100,
			0x0);
	}
#endif
}
#endif

static const struct snd_kcontrol_new mt8512_afe_gasrc0_input_mux =
	SOC_DAPM_ENUM_EXT("GASRC0 Source", mt8512_afe_gasrc0_input_enum,
		mt8512_afe_gasrc_source_mux_get,
		mt8512_afe_gasrc_source_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc1_input_mux =
	SOC_DAPM_ENUM_EXT("GASRC1 Source", mt8512_afe_gasrc1_input_enum,
		mt8512_afe_gasrc_source_mux_get,
		mt8512_afe_gasrc_source_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc2_input_mux =
	SOC_DAPM_ENUM_EXT("GASRC2 Source", mt8512_afe_gasrc2_input_enum,
		mt8512_afe_gasrc_source_mux_get,
		mt8512_afe_gasrc_source_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc3_input_mux =
	SOC_DAPM_ENUM_EXT("GASRC3 Source", mt8512_afe_gasrc3_input_enum,
		mt8512_afe_gasrc_source_mux_get,
		mt8512_afe_gasrc_source_mux_put);

static const struct snd_kcontrol_new mt8512_afe_gasrc0_output_mux =
	SOC_DAPM_ENUM_EXT("GASRC0 Sink", mt8512_afe_gasrc0_output_enum,
		mt8512_afe_gasrc_sink_mux_get,
		mt8512_afe_gasrc_sink_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc1_output_mux =
	SOC_DAPM_ENUM_EXT("GASRC1 Sink", mt8512_afe_gasrc1_output_enum,
		mt8512_afe_gasrc_sink_mux_get,
		mt8512_afe_gasrc_sink_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc2_output_mux =
	SOC_DAPM_ENUM_EXT("GASRC2 Sink", mt8512_afe_gasrc2_output_enum,
		mt8512_afe_gasrc_sink_mux_get,
		mt8512_afe_gasrc_sink_mux_put);
static const struct snd_kcontrol_new mt8512_afe_gasrc3_output_mux =
	SOC_DAPM_ENUM_EXT("GASRC3 Sink", mt8512_afe_gasrc3_output_enum,
		mt8512_afe_gasrc_sink_mux_get,
		mt8512_afe_gasrc_sink_mux_put);

/* AFE_DAC_CLK */
static int mt8512_afe_dac_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8512_afe_enable_main_clk(afe);
		mt8512_afe_enable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DAC);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_DAC_PREDIS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DAC_PREDIS);
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_DAC);
		mt8512_afe_disable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
		mt8512_afe_disable_main_clk(afe);
		break;
	default:
		break;
	}

	return 0;
}

/* AFE_ADC_CLK */
static int mt8512_afe_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s, event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt8512_afe_enable_main_clk(afe);
		mt8512_afe_enable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
		mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_ADC);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_ADC);
		mt8512_afe_disable_clk(afe,
			afe_priv->clocks[MT8512_CLK_TOP_AUD_26M]);
		mt8512_afe_disable_main_clk(afe);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget mt8512_afe_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("I00", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I01", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I02", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I03", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I04", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I06", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I07", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I09", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I10", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I11", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I12", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I13", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I14", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I15", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I16", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I17", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I18", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I19", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I20", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I21", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I22", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I23", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I24", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I25", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I26", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I27", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I28", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I29", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I32", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I33", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I34", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I35", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I36", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I37", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I38", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I39", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I40", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I41", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I42", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I43", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I44", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I45", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I46", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I47", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I48", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I49", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I50", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I51", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I52", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I53", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I54", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I55", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I56", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I57", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I58", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I59", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I60", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I61", SND_SOC_NOPM, 0, 0, NULL, 0),

	// virtual mixer
	SND_SOC_DAPM_MIXER("I22V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I23V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I24V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I25V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I26V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I27V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I28V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I29V", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O14_O15", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O00", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o00_mix, ARRAY_SIZE(mt8512_afe_o00_mix)),
	SND_SOC_DAPM_MIXER("O01", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o01_mix, ARRAY_SIZE(mt8512_afe_o01_mix)),
	SND_SOC_DAPM_MIXER("O02", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o02_mix, ARRAY_SIZE(mt8512_afe_o02_mix)),
	SND_SOC_DAPM_MIXER("O03", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o03_mix, ARRAY_SIZE(mt8512_afe_o03_mix)),
	SND_SOC_DAPM_MIXER("O04", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o04_mix, ARRAY_SIZE(mt8512_afe_o04_mix)),
	SND_SOC_DAPM_MIXER("O05", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o05_mix, ARRAY_SIZE(mt8512_afe_o05_mix)),
	SND_SOC_DAPM_MIXER("O06", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o06_mix, ARRAY_SIZE(mt8512_afe_o06_mix)),
	SND_SOC_DAPM_MIXER("O07", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o07_mix, ARRAY_SIZE(mt8512_afe_o07_mix)),
	SND_SOC_DAPM_MIXER("O08", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o08_mix, ARRAY_SIZE(mt8512_afe_o08_mix)),
	SND_SOC_DAPM_MIXER("O09", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o09_mix, ARRAY_SIZE(mt8512_afe_o09_mix)),
	SND_SOC_DAPM_MIXER("O10", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o10_mix, ARRAY_SIZE(mt8512_afe_o10_mix)),
	SND_SOC_DAPM_MIXER("O11", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o11_mix, ARRAY_SIZE(mt8512_afe_o11_mix)),
	SND_SOC_DAPM_MIXER("O12", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o12_mix, ARRAY_SIZE(mt8512_afe_o12_mix)),
	SND_SOC_DAPM_MIXER("O13", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o13_mix, ARRAY_SIZE(mt8512_afe_o13_mix)),
	SND_SOC_DAPM_MIXER("O14", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o14_mix, ARRAY_SIZE(mt8512_afe_o14_mix)),
	SND_SOC_DAPM_MIXER("O15", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o15_mix, ARRAY_SIZE(mt8512_afe_o15_mix)),
	SND_SOC_DAPM_MIXER("O16", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o16_mix, ARRAY_SIZE(mt8512_afe_o16_mix)),
	SND_SOC_DAPM_MIXER("O17", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o17_mix, ARRAY_SIZE(mt8512_afe_o17_mix)),
	SND_SOC_DAPM_MIXER("O18", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o18_mix, ARRAY_SIZE(mt8512_afe_o18_mix)),
	SND_SOC_DAPM_MIXER("O19", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o19_mix, ARRAY_SIZE(mt8512_afe_o19_mix)),
	SND_SOC_DAPM_MIXER("O20", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o20_mix, ARRAY_SIZE(mt8512_afe_o20_mix)),
	SND_SOC_DAPM_MIXER("O21", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o21_mix, ARRAY_SIZE(mt8512_afe_o21_mix)),
	SND_SOC_DAPM_MIXER("O26", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o26_mix, ARRAY_SIZE(mt8512_afe_o26_mix)),
	SND_SOC_DAPM_MIXER("O27", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o27_mix, ARRAY_SIZE(mt8512_afe_o27_mix)),
	SND_SOC_DAPM_MIXER("O28", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o28_mix, ARRAY_SIZE(mt8512_afe_o28_mix)),
	SND_SOC_DAPM_MIXER("O29", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o29_mix, ARRAY_SIZE(mt8512_afe_o29_mix)),
	SND_SOC_DAPM_MIXER("O30", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o30_mix, ARRAY_SIZE(mt8512_afe_o30_mix)),
	SND_SOC_DAPM_MIXER("O31", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o31_mix, ARRAY_SIZE(mt8512_afe_o31_mix)),
	SND_SOC_DAPM_MIXER("O32", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o32_mix, ARRAY_SIZE(mt8512_afe_o32_mix)),
	SND_SOC_DAPM_MIXER("O33", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o33_mix, ARRAY_SIZE(mt8512_afe_o33_mix)),
	SND_SOC_DAPM_MIXER("O34", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o34_mix, ARRAY_SIZE(mt8512_afe_o34_mix)),
	SND_SOC_DAPM_MIXER("O35", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o35_mix, ARRAY_SIZE(mt8512_afe_o35_mix)),
	SND_SOC_DAPM_MIXER("O36", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o36_mix, ARRAY_SIZE(mt8512_afe_o36_mix)),
	SND_SOC_DAPM_MIXER("O37", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o37_mix, ARRAY_SIZE(mt8512_afe_o37_mix)),
	SND_SOC_DAPM_MIXER("O38", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o38_mix, ARRAY_SIZE(mt8512_afe_o38_mix)),
	SND_SOC_DAPM_MIXER("O39", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o39_mix, ARRAY_SIZE(mt8512_afe_o39_mix)),
	SND_SOC_DAPM_MIXER("O40", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o40_mix, ARRAY_SIZE(mt8512_afe_o40_mix)),
	SND_SOC_DAPM_MIXER("O41", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o41_mix, ARRAY_SIZE(mt8512_afe_o41_mix)),
	SND_SOC_DAPM_MIXER("O42", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o42_mix, ARRAY_SIZE(mt8512_afe_o42_mix)),
	SND_SOC_DAPM_MIXER("O43", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o43_mix, ARRAY_SIZE(mt8512_afe_o43_mix)),
	SND_SOC_DAPM_MIXER("O44", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o44_mix, ARRAY_SIZE(mt8512_afe_o44_mix)),
	SND_SOC_DAPM_MIXER("O45", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o45_mix, ARRAY_SIZE(mt8512_afe_o45_mix)),
	SND_SOC_DAPM_MIXER("O46", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o46_mix, ARRAY_SIZE(mt8512_afe_o46_mix)),
	SND_SOC_DAPM_MIXER("O47", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o47_mix, ARRAY_SIZE(mt8512_afe_o47_mix)),
	SND_SOC_DAPM_MIXER("O48", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o48_mix, ARRAY_SIZE(mt8512_afe_o48_mix)),
	SND_SOC_DAPM_MIXER("O49", SND_SOC_NOPM, 0, 0,
			   mt8512_afe_o49_mix, ARRAY_SIZE(mt8512_afe_o49_mix)),

	SND_SOC_DAPM_INPUT("Virtual_DL_Input"),
	SND_SOC_DAPM_INPUT("DMIC In"),
	SND_SOC_DAPM_INPUT("ETDM1 In"),
	SND_SOC_DAPM_OUTPUT("ETDM2 Out"),
	SND_SOC_DAPM_INPUT("ETDM2 In"),

	SND_SOC_DAPM_MUX("GASRC0 Input Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc0_input_mux),
	SND_SOC_DAPM_MUX("GASRC1 Input Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc1_input_mux),
	SND_SOC_DAPM_MUX("GASRC2 Input Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc2_input_mux),
	SND_SOC_DAPM_MUX("GASRC3 Input Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc3_input_mux),
	SND_SOC_DAPM_MUX("GASRC0 Output Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc0_output_mux),
	SND_SOC_DAPM_MUX("GASRC1 Output Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc1_output_mux),
	SND_SOC_DAPM_MUX("GASRC2 Output Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc2_output_mux),
	SND_SOC_DAPM_MUX("GASRC3 Output Mux", SND_SOC_NOPM, 0, 0,
			   &mt8512_afe_gasrc3_output_mux),
	SND_SOC_DAPM_MUX("UL3 Mux", SND_SOC_NOPM, 0, 0, &ul3_mux),
	SND_SOC_DAPM_MUX("UL1 Mux", SND_SOC_NOPM, 0, 0, &ul1_mux),

	SND_SOC_DAPM_SUPPLY_S("AFE_DAC_CLK", 2, SND_SOC_NOPM, 0, 0,
			mt8512_afe_dac_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("AFE_ADC_CLK", 2, SND_SOC_NOPM, 0, 0,
			mt8512_afe_adc_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route mt8512_afe_pcm_routes[] = {
	{"I22", NULL, "DLM"},
	{"I23", NULL, "DLM"},
	{"I24", NULL, "DLM"},
	{"I25", NULL, "DLM"},
	{"I26", NULL, "DLM"},
	{"I27", NULL, "DLM"},
	{"I28", NULL, "DLM"},
	{"I29", NULL, "DLM"},

	{"O04", "I22 Switch", "I22"},
	{"O04", "I24 Switch", "I24"},
	{"O04", "I26 Switch", "I26"},
	{"O04", "I28 Switch", "I28"},
	{"O05", "I23 Switch", "I23"},
	{"O05", "I25 Switch", "I25"},
	{"O05", "I27 Switch", "I27"},
	{"O05", "I29 Switch", "I29"},
	{"O06", "I24 Switch", "I24"},
	{"O07", "I25 Switch", "I25"},
	{"O08", "I26 Switch", "I26"},
	{"O09", "I27 Switch", "I27"},
	{"O10", "I28 Switch", "I28"},
	{"O11", "I29 Switch", "I29"},

	{"ETDM2 Playback", NULL, "O04"},
	{"ETDM2 Playback", NULL, "O05"},
	{"ETDM2 Playback", NULL, "O06"},
	{"ETDM2 Playback", NULL, "O07"},
	{"ETDM2 Playback", NULL, "O08"},
	{"ETDM2 Playback", NULL, "O09"},
	{"ETDM2 Playback", NULL, "O10"},
	{"ETDM2 Playback", NULL, "O11"},
	{"ETDM2 Out", NULL, "ETDM2 Playback"},

	{"I40", NULL, "DL2"},
	{"I41", NULL, "DL2"},

	{"O04", "I40 Switch", "I40"},
	{"O05", "I41 Switch", "I41"},

	{"I20", NULL, "DL3"},
	{"I21", NULL, "DL3"},

	{"O04", "I20 Switch", "I20"},
	{"O05", "I21 Switch", "I21"},

	{"I00", NULL, "DL6"},
	{"I01", NULL, "DL6"},

	{"O00", "I00 Switch", "I00"},
	{"O01", "I01 Switch", "I01"},

	{"PCM1 Playback", NULL, "O00"},
	{"PCM1 Playback", NULL, "O01"},

	{"UL1 Mux", "Spdif_In", "SPDIF Capture"},
	{"UL1 Mux", "Multi_In", "MULTI Capture"},
	{"UL1", NULL, "UL1 Mux"},

	{"UL9", NULL, "O26"},
	{"UL9", NULL, "O27"},
	{"UL9", NULL, "O28"},
	{"UL9", NULL, "O29"},
	{"UL9", NULL, "O30"},
	{"UL9", NULL, "O31"},
	{"UL9", NULL, "O32"},
	{"UL9", NULL, "O33"},
	{"UL9", NULL, "O34"},
	{"UL9", NULL, "O35"},
	{"UL9", NULL, "O36"},
	{"UL9", NULL, "O37"},
	{"UL9", NULL, "O38"},
	{"UL9", NULL, "O39"},
	{"UL9", NULL, "O40"},
	{"UL9", NULL, "O41"},

	{"UL2", NULL, "O34"},
	{"UL2", NULL, "O35"},
	{"UL2", NULL, "O36"},
	{"UL2", NULL, "O37"},
	{"UL2", NULL, "O38"},
	{"UL2", NULL, "O39"},
	{"UL2", NULL, "O40"},
	{"UL2", NULL, "O41"},

	{"UL2", NULL, "O18"},
	{"UL2", NULL, "O19"},

	{"O26", "I12 Switch", "I12"},
	{"O27", "I13 Switch", "I13"},
	{"O28", "I14 Switch", "I14"},
	{"O29", "I15 Switch", "I15"},
	{"O30", "I16 Switch", "I16"},
	{"O31", "I17 Switch", "I17"},
	{"O32", "I18 Switch", "I18"},
	{"O33", "I19 Switch", "I19"},
	{"O34", "I12 Switch", "I12"},
	{"O35", "I13 Switch", "I13"},
	{"O36", "I14 Switch", "I14"},
	{"O37", "I15 Switch", "I15"},
	{"O38", "I16 Switch", "I16"},
	{"O39", "I17 Switch", "I17"},
	{"O40", "I18 Switch", "I18"},
	{"O41", "I19 Switch", "I19"},

	{"O18", "I04 Switch", "I04"},
	{"O19", "I05 Switch", "I05"},
	{"O18", "I12 Switch", "I12"},
	{"O19", "I13 Switch", "I13"},
	{"O18", "I42 Switch", "I42"},
	{"O19", "I43 Switch", "I43"},

	{"I12", NULL, "ETDM2 Capture"},
	{"I13", NULL, "ETDM2 Capture"},
	{"I14", NULL, "ETDM2 Capture"},
	{"I15", NULL, "ETDM2 Capture"},
	{"I16", NULL, "ETDM2 Capture"},
	{"I17", NULL, "ETDM2 Capture"},
	{"I18", NULL, "ETDM2 Capture"},
	{"I19", NULL, "ETDM2 Capture"},
	{"ETDM2 Capture", NULL, "ETDM2 In"},

	{"I60", NULL, "INT ADDA Capture"},
	{"I61", NULL, "INT ADDA Capture"},
	{"O26", "I60 Switch", "I60"},
	{"O27", "I61 Switch", "I61"},
	{"O18", "I60 Switch", "I60"},
	{"O19", "I61 Switch", "I61"},
	{"O12", "I60 Switch", "I60"},
	{"O13", "I61 Switch", "I61"},
	{"O14", "I60 Switch", "I60"},
	{"O15", "I61 Switch", "I61"},
	{"O16", "I60 Switch", "I60"},
	{"O17", "I61 Switch", "I61"},

	{"INT ADDA Playback", NULL, "O20"},
	{"INT ADDA Playback", NULL, "O21"},
	{"O20", "I22 Switch", "I22"},
	{"O20", "I23 Switch", "I23"},
	{"O20", "I24 Switch", "I24"},
	{"O20", "I26 Switch", "I26"},
	{"O20", "I28 Switch", "I28"},
	{"O21", "I22 Switch", "I22"},
	{"O21", "I23 Switch", "I23"},
	{"O21", "I25 Switch", "I25"},
	{"O21", "I27 Switch", "I27"},
	{"O21", "I29 Switch", "I29"},
	{"O20", "I40 Switch", "I40"},
	{"O20", "I41 Switch", "I41"},
	{"O21", "I40 Switch", "I40"},
	{"O21", "I41 Switch", "I41"},
	{"O20", "I20 Switch", "I20"},
	{"O20", "I21 Switch", "I21"},
	{"O21", "I20 Switch", "I20"},
	{"O21", "I21 Switch", "I21"},

	{"O26", "I42 Switch", "I42"},
	{"O27", "I43 Switch", "I43"},
	{"O28", "I44 Switch", "I44"},
	{"O29", "I45 Switch", "I45"},
	{"O30", "I46 Switch", "I46"},
	{"O31", "I47 Switch", "I47"},
	{"O32", "I48 Switch", "I48"},
	{"O33", "I49 Switch", "I49"},
	{"O34", "I50 Switch", "I50"},
	{"O35", "I51 Switch", "I51"},
	{"O36", "I52 Switch", "I52"},
	{"O37", "I53 Switch", "I53"},
	{"O38", "I54 Switch", "I54"},
	{"O39", "I55 Switch", "I55"},
	{"O40", "I56 Switch", "I56"},
	{"O41", "I57 Switch", "I57"},

	{"I42", NULL, "ETDM1 Capture"},
	{"I43", NULL, "ETDM1 Capture"},
	{"I44", NULL, "ETDM1 Capture"},
	{"I45", NULL, "ETDM1 Capture"},
	{"I46", NULL, "ETDM1 Capture"},
	{"I47", NULL, "ETDM1 Capture"},
	{"I48", NULL, "ETDM1 Capture"},
	{"I49", NULL, "ETDM1 Capture"},
	{"I50", NULL, "ETDM1 Capture"},
	{"I51", NULL, "ETDM1 Capture"},
	{"I52", NULL, "ETDM1 Capture"},
	{"I53", NULL, "ETDM1 Capture"},
	{"I54", NULL, "ETDM1 Capture"},
	{"I55", NULL, "ETDM1 Capture"},
	{"I56", NULL, "ETDM1 Capture"},
	{"I57", NULL, "ETDM1 Capture"},
	{"ETDM1 Capture", NULL, "ETDM1 In"},

	{"O26", "I04 Switch", "I04"},
	{"O27", "I05 Switch", "I05"},
	{"O28", "I06 Switch", "I06"},
	{"O29", "I07 Switch", "I07"},
	{"O30", "I08 Switch", "I08"},
	{"O31", "I09 Switch", "I09"},
	{"O32", "I10 Switch", "I10"},
	{"O33", "I11 Switch", "I11"},
	{"O34", "I04 Switch", "I04"},
	{"O35", "I05 Switch", "I05"},
	{"O36", "I06 Switch", "I06"},
	{"O37", "I07 Switch", "I07"},
	{"O38", "I08 Switch", "I08"},
	{"O39", "I09 Switch", "I09"},
	{"O40", "I10 Switch", "I10"},
	{"O41", "I11 Switch", "I11"},

	{"O42", "I22 Switch", "I22"},
	{"O43", "I23 Switch", "I23"},
	{"O48", "I22 Switch", "I22"},
	{"O49", "I23 Switch", "I23"},
	{"O44", "I24 Switch", "I24"},
	{"O45", "I25 Switch", "I25"},
	{"O46", "I26 Switch", "I26"},
	{"O47", "I27 Switch", "I27"},
	{"O48", "I28 Switch", "I28"},
	{"O49", "I29 Switch", "I29"},

	{"O42", "I40 Switch", "I40"},
	{"O43", "I41 Switch", "I41"},
	{"O48", "I40 Switch", "I40"},
	{"O49", "I41 Switch", "I41"},

	{"O42", "I20 Switch", "I20"},
	{"O43", "I21 Switch", "I21"},
	{"O48", "I20 Switch", "I20"},
	{"O49", "I21 Switch", "I21"},

	{"GASRC0 Input Mux", "Gasrc_8ch", "O42"},
	{"GASRC0 Input Mux", "Gasrc_8ch", "O43"},
	{"GASRC0 Input Mux", "Gasrc_6ch", "O42"},
	{"GASRC0 Input Mux", "Gasrc_6ch", "O43"},
	{"GASRC0 Input Mux", "Gasrc_4ch", "O42"},
	{"GASRC0 Input Mux", "Gasrc_4ch", "O43"},
	{"GASRC0 Input Mux", "Gasrc_2ch", "O42"},
	{"GASRC0 Input Mux", "Gasrc_2ch", "O43"},
	{"GASRC0 Playback", NULL, "GASRC0 Input Mux"},
	{"GASRC0 Capture", NULL, "GASRC0 Input Mux"},

	{"GASRC1 Input Mux", "Gasrc_8ch", "O44"},
	{"GASRC1 Input Mux", "Gasrc_8ch", "O45"},
	{"GASRC1 Input Mux", "Gasrc_6ch", "O44"},
	{"GASRC1 Input Mux", "Gasrc_6ch", "O45"},
	{"GASRC1 Input Mux", "Gasrc_4ch", "O44"},
	{"GASRC1 Input Mux", "Gasrc_4ch", "O45"},
	{"GASRC1 Input Mux", "Gasrc_2ch", "O44"},
	{"GASRC1 Input Mux", "Gasrc_2ch", "O45"},
	{"GASRC1 Playback", NULL, "GASRC1 Input Mux"},
	{"GASRC1 Capture", NULL, "GASRC1 Input Mux"},

	{"GASRC2 Input Mux", "Gasrc_8ch", "O46"},
	{"GASRC2 Input Mux", "Gasrc_8ch", "O47"},
	{"GASRC2 Input Mux", "Gasrc_6ch", "O46"},
	{"GASRC2 Input Mux", "Gasrc_6ch", "O47"},
	{"GASRC2 Input Mux", "Gasrc_2ch", "O46"},
	{"GASRC2 Input Mux", "Gasrc_2ch", "O47"},
	{"GASRC2 Playback", NULL, "GASRC2 Input Mux"},
	{"GASRC2 Capture", NULL, "GASRC2 Input Mux"},

	{"GASRC3 Input Mux", "Gasrc_8ch", "O48"},
	{"GASRC3 Input Mux", "Gasrc_8ch", "O49"},
	{"GASRC3 Input Mux", "Gasrc_2ch", "O48"},
	{"GASRC3 Input Mux", "Gasrc_2ch", "O49"},
	{"GASRC3 Playback", NULL, "GASRC3 Input Mux"},
	{"GASRC3 Capture", NULL, "GASRC3 Input Mux"},

	{"GASRC0 Output Mux", "Gasrc_8ch", "GASRC0 Capture"},
	{"GASRC0 Output Mux", "Gasrc_8ch", "GASRC0 Playback"},
	{"GASRC0 Output Mux", "Gasrc_6ch", "GASRC0 Capture"},
	{"GASRC0 Output Mux", "Gasrc_6ch", "GASRC0 Playback"},
	{"GASRC0 Output Mux", "Gasrc_4ch", "GASRC0 Capture"},
	{"GASRC0 Output Mux", "Gasrc_4ch", "GASRC0 Playback"},
	{"GASRC0 Output Mux", "Gasrc_2ch", "GASRC0 Capture"},
	{"GASRC0 Output Mux", "Gasrc_2ch", "GASRC0 Playback"},

	{"GASRC1 Output Mux", "Gasrc_8ch", "GASRC1 Capture"},
	{"GASRC1 Output Mux", "Gasrc_8ch", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "Gasrc_6ch", "GASRC1 Capture"},
	{"GASRC1 Output Mux", "Gasrc_6ch", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "Gasrc_4ch", "GASRC1 Capture"},
	{"GASRC1 Output Mux", "Gasrc_4ch", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "Gasrc_2ch", "GASRC1 Capture"},
	{"GASRC1 Output Mux", "Gasrc_2ch", "GASRC1 Playback"},

	{"GASRC2 Output Mux", "Gasrc_8ch", "GASRC2 Capture"},
	{"GASRC2 Output Mux", "Gasrc_8ch", "GASRC2 Playback"},
	{"GASRC2 Output Mux", "Gasrc_6ch", "GASRC2 Capture"},
	{"GASRC2 Output Mux", "Gasrc_6ch", "GASRC2 Playback"},
	{"GASRC2 Output Mux", "Gasrc_2ch", "GASRC2 Capture"},
	{"GASRC2 Output Mux", "Gasrc_2ch", "GASRC2 Playback"},

	{"GASRC3 Output Mux", "Gasrc_8ch", "GASRC3 Capture"},
	{"GASRC3 Output Mux", "Gasrc_8ch", "GASRC3 Playback"},
	{"GASRC3 Output Mux", "Gasrc_2ch", "GASRC3 Capture"},
	{"GASRC3 Output Mux", "Gasrc_2ch", "GASRC3 Playback"},

	{"I32", NULL, "GASRC0 Output Mux"},
	{"I33", NULL, "GASRC0 Output Mux"},
	{"I34", NULL, "GASRC1 Output Mux"},
	{"I35", NULL, "GASRC1 Output Mux"},
	{"I36", NULL, "GASRC2 Output Mux"},
	{"I37", NULL, "GASRC2 Output Mux"},
	{"I38", NULL, "GASRC3 Output Mux"},
	{"I39", NULL, "GASRC3 Output Mux"},

	{"O42", "I04 Switch", "I04"},
	{"O43", "I05 Switch", "I05"},
	{"O44", "I06 Switch", "I06"},
	{"O45", "I07 Switch", "I07"},
	{"O46", "I08 Switch", "I08"},
	{"O47", "I09 Switch", "I09"},
	{"O48", "I10 Switch", "I10"},
	{"O49", "I11 Switch", "I11"},

	{"O42", "I45 Switch", "I45"},
	{"O43", "I44 Switch", "I44"},
	{"O44", "I43 Switch", "I43"},
	{"O45", "I42 Switch", "I42"},

	{"O42", "I42 Switch", "I42"},
	{"O43", "I43 Switch", "I43"},
	{"O44", "I44 Switch", "I44"},
	{"O45", "I45 Switch", "I45"},
	{"O46", "I46 Switch", "I46"},
	{"O47", "I47 Switch", "I47"},
	{"O48", "I48 Switch", "I48"},
	{"O49", "I49 Switch", "I49"},

	{"O42", "I10 Switch", "I10"},
	{"O43", "I11 Switch", "I11"},
	{"O44", "I10 Switch", "I10"},
	{"O45", "I11 Switch", "I11"},
	{"O46", "I10 Switch", "I10"},
	{"O47", "I11 Switch", "I11"},

	{"O42", "I12 Switch", "I12"},
	{"O43", "I13 Switch", "I13"},
	{"O44", "I14 Switch", "I14"},
	{"O45", "I15 Switch", "I15"},
	{"O46", "I16 Switch", "I16"},
	{"O47", "I17 Switch", "I17"},
	{"O48", "I18 Switch", "I18"},
	{"O49", "I19 Switch", "I19"},

	{"O42", "I18 Switch", "I18"},
	{"O43", "I19 Switch", "I19"},
	{"O44", "I18 Switch", "I18"},
	{"O45", "I19 Switch", "I19"},
	{"O46", "I18 Switch", "I18"},
	{"O47", "I19 Switch", "I19"},

	{"O04", "I32 Switch", "I32"},
	{"O05", "I33 Switch", "I33"},
	{"O06", "I34 Switch", "I34"},
	{"O07", "I35 Switch", "I35"},
	{"O08", "I36 Switch", "I36"},
	{"O09", "I37 Switch", "I37"},
	{"O10", "I38 Switch", "I38"},
	{"O11", "I39 Switch", "I39"},

	{"O26", "I32 Switch", "I32"},
	{"O27", "I33 Switch", "I33"},
	{"O28", "I34 Switch", "I34"},
	{"O29", "I35 Switch", "I35"},
	{"O30", "I36 Switch", "I36"},
	{"O31", "I37 Switch", "I37"},
	{"O32", "I38 Switch", "I38"},
	{"O33", "I39 Switch", "I39"},

	{"O28", "I32 Switch", "I32"},
	{"O29", "I33 Switch", "I33"},
	{"O30", "I32 Switch", "I32"},
	{"O31", "I33 Switch", "I33"},
	{"O32", "I32 Switch", "I32"},
	{"O33", "I33 Switch", "I33"},

	{"O28", "I38 Switch", "I38"},
	{"O29", "I39 Switch", "I39"},
	{"O30", "I38 Switch", "I38"},
	{"O31", "I39 Switch", "I39"},
	{"O34", "I38 Switch", "I38"},
	{"O35", "I39 Switch", "I39"},

	{"O34", "I32 Switch", "I32"},
	{"O35", "I33 Switch", "I33"},
	{"O36", "I34 Switch", "I34"},
	{"O37", "I35 Switch", "I35"},
	{"O38", "I36 Switch", "I36"},
	{"O39", "I37 Switch", "I37"},
	{"O40", "I38 Switch", "I38"},
	{"O41", "I39 Switch", "I39"},

	{"O12", "I32 Switch", "I32"},
	{"O13", "I33 Switch", "I33"},
	{"O14", "I32 Switch", "I32"},
	{"O15", "I33 Switch", "I33"},
	{"O16", "I32 Switch", "I32"},
	{"O17", "I33 Switch", "I33"},

	{"O12", "I10 Switch", "I10"},
	{"O13", "I11 Switch", "I11"},

	{"O12", "I18 Switch", "I18"},
	{"O13", "I19 Switch", "I19"},

	{"I04", NULL, "DMIC Capture"},
	{"I05", NULL, "DMIC Capture"},
	{"I06", NULL, "DMIC Capture"},
	{"I07", NULL, "DMIC Capture"},
	{"I08", NULL, "DMIC Capture"},
	{"I09", NULL, "DMIC Capture"},
	{"I10", NULL, "DMIC Capture"},
	{"I11", NULL, "DMIC Capture"},
	{"DMIC Capture", NULL, "DMIC In"},

	{"UL5", NULL, "O02"},
	{"UL5", NULL, "O03"},

	{"O02", "I02 Switch", "I02"},
	{"O03", "I03 Switch", "I03"},

	{"I02", NULL, "PCM1 Capture"},
	{"I03", NULL, "PCM1 Capture"},

	{"UL8", NULL, "ETDM1 Capture"},

	{"O14", "I12 Switch", "I12"},
	{"O15", "I13 Switch", "I13"},
	{"O16", "I12 Switch", "I12"},
	{"O17", "I13 Switch", "I13"},
	{"O14", "I42 Switch", "I42"},
	{"O15", "I43 Switch", "I43"},
	{"O16", "I42 Switch", "I42"},
	{"O17", "I43 Switch", "I43"},
	{"O14", "I04 Switch", "I04"},
	{"O15", "I05 Switch", "I05"},
	{"O16", "I04 Switch", "I04"},
	{"O17", "I05 Switch", "I05"},

	{"UL10", NULL, "O12"},
	{"UL10", NULL, "O13"},

	{"O14_O15", NULL, "O14"},
	{"O14_O15", NULL, "O15"},
	{"UL3 Mux", "Interconn", "O14_O15"},
	{"UL3 Mux", "Direct", "ETDM2 Capture"},
	{"UL3", NULL, "UL3 Mux"},

	{"UL4", NULL, "O16"},
	{"UL4", NULL, "O17"},

	{"VIRTUAL_DL_SRC", NULL, "Virtual_DL_Input"},

	{"I22V", NULL, "VIRTUAL_DL_SRC"},
	{"I23V", NULL, "VIRTUAL_DL_SRC"},
	{"I24V", NULL, "VIRTUAL_DL_SRC"},
	{"I25V", NULL, "VIRTUAL_DL_SRC"},
	{"I26V", NULL, "VIRTUAL_DL_SRC"},
	{"I27V", NULL, "VIRTUAL_DL_SRC"},
	{"I28V", NULL, "VIRTUAL_DL_SRC"},
	{"I29V", NULL, "VIRTUAL_DL_SRC"},

	{"O42", "I22 Switch", "I22V"},
	{"O43", "I23 Switch", "I23V"},
	{"O44", "I24 Switch", "I24V"},
	{"O45", "I25 Switch", "I25V"},
	{"O46", "I26 Switch", "I26V"},
	{"O47", "I27 Switch", "I27V"},
	{"O48", "I28 Switch", "I28V"},
	{"O49", "I29 Switch", "I29V"},

	{"O12", "I22 Switch", "I22V"},
	{"O13", "I23 Switch", "I23V"},

	{"O28", "I22 Switch", "I22V"},
	{"O29", "I23 Switch", "I23V"},
	{"O30", "I22 Switch", "I22V"},
	{"O31", "I23 Switch", "I23V"},
	{"O32", "I22 Switch", "I22V"},
	{"O33", "I23 Switch", "I23V"},
	{"O34", "I22 Switch", "I22V"},
	{"O35", "I23 Switch", "I23V"},

	{"O36", "I24 Switch", "I24V"},
	{"O37", "I25 Switch", "I25V"},
	{"O38", "I26 Switch", "I26V"},
	{"O39", "I27 Switch", "I27V"},
	{"O40", "I28 Switch", "I28V"},
	{"O41", "I29 Switch", "I29V"},

	{"O32", "I24 Switch", "I24"},
	{"O33", "I25 Switch", "I25"},
	{"O34", "I24 Switch", "I24"},
	{"O35", "I25 Switch", "I25"},

	{"O20", "I38 Switch", "I38"},
	{"O21", "I39 Switch", "I39"},
	{"O36", "I38 Switch", "I38"},
	{"O37", "I39 Switch", "I39"},

	{"O30", "I40 Switch", "I40"},
	{"O31", "I41 Switch", "I41"},
	{"O32", "I40 Switch", "I40"},
	{"O33", "I41 Switch", "I41"},
};

static const struct snd_soc_component_driver mt8512_afe_pcm_dai_component = {
	.name = "mt8512-afe-pcm-dai",
	.dapm_widgets = mt8512_afe_pcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8512_afe_pcm_widgets),
	.dapm_routes = mt8512_afe_pcm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8512_afe_pcm_routes),
};

static const struct mtk_base_memif_data memif_data[MT8512_AFE_MEMIF_NUM] = {
	{
		.name = "DLM",
		.id = MT8512_AFE_MEMIF_DLM,
		.reg_ofs_base = AFE_DL10_BASE,
		.reg_ofs_cur = AFE_DL10_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_DL10_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 26,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 26,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 26,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 26,
		.ch_config_reg = AFE_DL10_CON0,
		.ch_config_shift = 0,
		.int_odd_reg = -1,
		.int_odd_shift = -1,
		.buffer_bytes_align = 64,
	}, {
		.name = "DL2",
		.id = MT8512_AFE_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_DL2_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 18,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 18,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 18,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 18,
		.ch_config_reg = AFE_DL2_CON0,
		.ch_config_shift = 0,
		.int_odd_reg = -1,
		.int_odd_shift = -1,
		.buffer_bytes_align = 64,
	}, {
		.name = "DL3",
		.id = MT8512_AFE_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_DL3_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 19,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 19,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 19,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 19,
		.ch_config_reg = AFE_DL3_CON0,
		.ch_config_shift = 0,
		.int_odd_reg = -1,
		.int_odd_shift = -1,
		.buffer_bytes_align = 64,
	}, {
		.name = "DL6",
		.id = MT8512_AFE_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.hd_reg = AFE_DL6_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 22,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 22,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 22,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 22,
		.ch_config_reg = AFE_DL6_CON0,
		.ch_config_shift = 0,
		.int_odd_reg = -1,
		.int_odd_shift = -1,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL1",
		.id = MT8512_AFE_MEMIF_UL1,
		.reg_ofs_base = AFE_UL1_BASE,
		.reg_ofs_cur = AFE_UL1_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL1_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL1_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 1,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 0,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 0,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 0,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL1_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL2",
		.id = MT8512_AFE_MEMIF_UL2,
		.reg_ofs_base = AFE_UL2_BASE,
		.reg_ofs_cur = AFE_UL2_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL2_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL2_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 2,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 1,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 1,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 1,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL2_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL3",
		.id = MT8512_AFE_MEMIF_UL3,
		.reg_ofs_base = AFE_UL3_BASE,
		.reg_ofs_cur = AFE_UL3_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL3_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL3_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 3,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 2,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 2,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 2,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL3_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL4",
		.id = MT8512_AFE_MEMIF_UL4,
		.reg_ofs_base = AFE_UL4_BASE,
		.reg_ofs_cur = AFE_UL4_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL4_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL4_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 4,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 3,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 3,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 3,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL4_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL5",
		.id = MT8512_AFE_MEMIF_UL5,
		.reg_ofs_base = AFE_UL5_BASE,
		.reg_ofs_cur = AFE_UL5_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL5_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL5_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 5,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 4,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 4,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 4,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL5_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	},  {
		.name = "UL8",
		.id = MT8512_AFE_MEMIF_UL8,
		.reg_ofs_base = AFE_UL8_BASE,
		.reg_ofs_cur = AFE_UL8_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL8_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL8_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 8,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 7,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 7,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 7,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL8_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL9",
		.id = MT8512_AFE_MEMIF_UL9,
		.reg_ofs_base = AFE_UL9_BASE,
		.reg_ofs_cur = AFE_UL9_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL9_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL9_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 9,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 8,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 8,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 8,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL9_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	}, {
		.name = "UL10",
		.id = MT8512_AFE_MEMIF_UL10,
		.reg_ofs_base = AFE_UL10_BASE,
		.reg_ofs_cur = AFE_UL10_CUR,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL10_CON0,
		.mono_shift = 16,
		.hd_reg = AFE_UL10_CON0,
		.hd_shift = 5,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 10,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 10,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 10,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 9,
		.ch_config_reg = -1,
		.ch_config_shift = -1,
		.int_odd_reg = AFE_UL10_CON0,
		.int_odd_shift = 0,
		.buffer_bytes_align = 64,
	},
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mtk_base_afe *mt8512_afe_pcm_get_info(void)
{
	return g_priv;
}

EXPORT_SYMBOL(mt8512_afe_pcm_get_info);

#endif

static int spdif_in_irq_handler(int irq, void *dev_id)
{
	struct mtk_base_afe *afe = dev_id;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mtk_base_afe_memif *memif = &afe->memif[MT8512_AFE_MEMIF_UL1];
	struct mt8512_multi_in_data *multi_in = &afe_priv->multi_in_data;

	multi_in->current_irq_count++;
	if (multi_in->current_irq_count >= multi_in->notify_irq_count) {
		snd_pcm_period_elapsed(memif->substream);
		multi_in->current_irq_count = 0;
	}

	return 0;
}

static unsigned int get_spdif_in_clear_bits(unsigned int v)
{
	unsigned int bits = 0;

	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_NON_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_B_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_B_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_M_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_M_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_W_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_W_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_BITCNT_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_BITCNT_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_PRE_ERR_PARITY_STS)
		bits |= AFE_SPDIFIN_EC_PRE_ERR_PARITY_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_TIMEOUT_ERR_STS)
		bits |= AFE_SPDIFIN_EC_TIMEOUT_INT_CLEAR;
	if (v & AFE_SPDIFIN_INT_EXT2_LRCK_CHANGE)
		bits |= AFE_SPDIFIN_EC_DATA_LRCK_CHANGE_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG1_DATALAT_ERR)
		bits |= AFE_SPDIFIN_EC_DATA_LATCH_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG3_CHSTS_PREAMPHASIS_STS)
		bits |= AFE_SPDIFIN_EC_CHSTS_PREAMPHASIS_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG2_FIFO_ERR)
		bits |= AFE_SPDIFIN_EC_FIFO_ERR_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG2_CHSTS_INT_FLAG)
		bits |= AFE_SPDIFIN_EC_CHSTS_COLLECTION_CLEAR;
	if (v & AFE_SPDIFIN_DEBUG2_PERR_9TIMES_FLAG)
		bits |= AFE_SPDIFIN_EC_USECODE_COLLECTION_CLEAR;

	return bits;
}

static unsigned int get_spdif_in_sample_rate(struct mtk_base_afe *afe)
{
	unsigned int val, fs;

	regmap_read(afe->regmap, AFE_SPDIFIN_INT_EXT2, &val);

	val &= AFE_SPDIFIN_INT_EXT2_ROUGH_FS_MASK;

	switch (val) {
	case AFE_SPDIFIN_INT_EXT2_FS_32K:
		fs = 32000;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_44D1K:
		fs = 44100;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_48K:
		fs = 48000;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_88D2K:
		fs = 88200;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_96K:
		fs = 96000;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_176D4K:
		fs = 176400;
		break;
	case AFE_SPDIFIN_INT_EXT2_FS_192K:
		fs = 192000;
		break;
	default:
		fs = 0;
		break;
	}

	return fs;
}

static void spdif_in_reset_and_clear_error(struct mtk_base_afe *afe,
	unsigned int err_bits)
{
	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_INT_EN |
			   AFE_SPDIFIN_CFG0_EN,
			   0x0);

	regmap_write(afe->regmap, AFE_SPDIFIN_EC, err_bits);

	regmap_update_bits(afe->regmap,
			   AFE_SPDIFIN_CFG0,
			   AFE_SPDIFIN_CFG0_INT_EN |
			   AFE_SPDIFIN_CFG0_EN,
			   AFE_SPDIFIN_CFG0_INT_EN |
			   AFE_SPDIFIN_CFG0_EN);
}

static int spdif_in_detect_irq_handler(int irq, void *dev_id)
{
	struct mtk_base_afe *afe = dev_id;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_spdif_in_data *spdif_in = &afe_priv->spdif_in_data;
	unsigned int debug1, debug2, debug3, int_ext2;
	unsigned int err;
	unsigned int rate;

	regmap_read(afe->regmap, AFE_SPDIFIN_INT_EXT2, &int_ext2);
	regmap_read(afe->regmap, AFE_SPDIFIN_DEBUG1, &debug1);
	regmap_read(afe->regmap, AFE_SPDIFIN_DEBUG2, &debug2);
	regmap_read(afe->regmap, AFE_SPDIFIN_DEBUG3, &debug3);

	err = (int_ext2 & AFE_SPDIFIN_INT_EXT2_LRCK_CHANGE) |
	      (debug1 & AFE_SPDIFIN_DEBUG1_DATALAT_ERR) |
	      (debug2 & AFE_SPDIFIN_DEBUG2_FIFO_ERR) |
	      (debug3 & AFE_SPDIFIN_DEBUG3_ALL_ERR);

	dev_dbg(afe->dev,
		"%s int_ext2(0x%x) debug(0x%x,0x%x,0x%x) err(0x%x)\n",
		__func__, int_ext2, debug1, debug2, debug3, err);

	if (err != 0) {
		spdif_in_reset_and_clear_error(afe,
			get_spdif_in_clear_bits(err));

		spdif_in->rate = 0;

		return 0;
	}

	rate = get_spdif_in_sample_rate(afe);
	if (rate == 0) {
		spdif_in_reset_and_clear_error(afe, AFE_SPDIFIN_EC_CLEAR_ALL);

		spdif_in->rate = 0;

		return 0;
	}

	spdif_in->rate = rate;

	/* clear all interrupt bits */
	regmap_write(afe->regmap,
		     AFE_SPDIFIN_EC,
		     AFE_SPDIFIN_EC_CLEAR_ALL);

	return 0;
}

static const struct mtk_base_irq_data irq_data[MT8512_AFE_IRQ_NUM] = {
	{
		.id = MT8512_AFE_IRQ1,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ1_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 0,
		.irq_status_shift = 16,
	}, {
		.id = MT8512_AFE_IRQ2,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ2_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 1,
		.irq_status_shift = 17,
		.custom_handler = spdif_in_detect_irq_handler,
	}, {
		.id = MT8512_AFE_IRQ3,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ3_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 2,
		.irq_status_shift = 18,
		.custom_handler = spdif_in_irq_handler,
	}, {
		.id = MT8512_AFE_IRQ4,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ4_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 3,
		.irq_status_shift = 19,
	}, {
		.id = MT8512_AFE_IRQ5,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ5_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 4,
		.irq_status_shift = 20,
	}, {
		.id = MT8512_AFE_IRQ6,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ6_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 5,
		.irq_status_shift = 21,
	}, {
		.id = MT8512_AFE_IRQ7,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_IRQ7_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 6,
		.irq_status_shift = 22,
	}, {
		.id = MT8512_AFE_IRQ8,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_TSF_CON1,
		.irq_en_shift = 0,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 7,
		.irq_status_shift = 23,
	}, {
		.id = MT8512_AFE_IRQ9,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = -1,
		.irq_cnt_maskbit = -1,
		.irq_en_reg = AFE_TSF_CON1,
		.irq_en_shift = 1,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_fs_maskbit = -1,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 8,
		.irq_status_shift = 24,
	}, {
		.id = MT8512_AFE_IRQ10,
		.irq_cnt_reg = ASYS_IRQ1_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ1_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ1_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 0,
		.irq_status_shift = 0,
	}, {
		.id = MT8512_AFE_IRQ11,
		.irq_cnt_reg = ASYS_IRQ2_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ2_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ2_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 1,
		.irq_status_shift = 1,
	}, {
		.id = MT8512_AFE_IRQ12,
		.irq_cnt_reg = ASYS_IRQ3_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ3_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ3_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 2,
		.irq_status_shift = 2,
	}, {
		.id = MT8512_AFE_IRQ13,
		.irq_cnt_reg = ASYS_IRQ4_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ4_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ4_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 3,
		.irq_status_shift = 3,
	}, {
		.id = MT8512_AFE_IRQ14,
		.irq_cnt_reg = ASYS_IRQ5_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ5_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ5_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 4,
		.irq_status_shift = 4,
	}, {
		.id = MT8512_AFE_IRQ15,
		.irq_cnt_reg = ASYS_IRQ6_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ6_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ6_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 5,
		.irq_status_shift = 5,
	}, {
		.id = MT8512_AFE_IRQ16,
		.irq_cnt_reg = ASYS_IRQ7_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ7_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ7_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 6,
		.irq_status_shift = 6,
	}, {
		.id = MT8512_AFE_IRQ17,
		.irq_cnt_reg = ASYS_IRQ8_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ8_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ8_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 7,
		.irq_status_shift = 7,
	}, {
		.id = MT8512_AFE_IRQ18,
		.irq_cnt_reg = ASYS_IRQ9_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ9_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ9_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 8,
		.irq_status_shift = 8,
	}, {
		.id = MT8512_AFE_IRQ19,
		.irq_cnt_reg = ASYS_IRQ10_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ10_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ10_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 9,
		.irq_status_shift = 9,
	}, {
		.id = MT8512_AFE_IRQ20,
		.irq_cnt_reg = ASYS_IRQ11_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ11_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ11_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 10,
		.irq_status_shift = 10,
	}, {
		.id = MT8512_AFE_IRQ21,
		.irq_cnt_reg = ASYS_IRQ12_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ12_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ12_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 11,
		.irq_status_shift = 11,
	}, {
		.id = MT8512_AFE_IRQ22,
		.irq_cnt_reg = ASYS_IRQ13_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ13_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ13_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 12,
		.irq_status_shift = 12,
	}, {
		.id = MT8512_AFE_IRQ23,
		.irq_cnt_reg = ASYS_IRQ14_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ14_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ14_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 13,
		.irq_status_shift = 13,
	}, {
		.id = MT8512_AFE_IRQ24,
		.irq_cnt_reg = ASYS_IRQ15_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ15_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ15_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 14,
		.irq_status_shift = 14,
	}, {
		.id = MT8512_AFE_IRQ25,
		.irq_cnt_reg = ASYS_IRQ16_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_en_reg = ASYS_IRQ16_CON,
		.irq_en_shift = 31,
		.irq_fs_reg = ASYS_IRQ16_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 15,
		.irq_status_shift = 15,
	},
};

static const int memif_specified_irqs[MT8512_AFE_MEMIF_NUM] = {
	[MT8512_AFE_MEMIF_DLM] = MT8512_AFE_IRQ10,
	[MT8512_AFE_MEMIF_DL2] = MT8512_AFE_IRQ11,
	[MT8512_AFE_MEMIF_DL3] = MT8512_AFE_IRQ12,
	[MT8512_AFE_MEMIF_DL6] = MT8512_AFE_IRQ13,
	[MT8512_AFE_MEMIF_UL1] = MT8512_AFE_IRQ3,
	[MT8512_AFE_MEMIF_UL2] = MT8512_AFE_IRQ15,
	[MT8512_AFE_MEMIF_UL3] = MT8512_AFE_IRQ16,
	[MT8512_AFE_MEMIF_UL4] = MT8512_AFE_IRQ17,
	[MT8512_AFE_MEMIF_UL5] = MT8512_AFE_IRQ18,
	[MT8512_AFE_MEMIF_UL8] = MT8512_AFE_IRQ19,
	[MT8512_AFE_MEMIF_UL9] = MT8512_AFE_IRQ20,
	[MT8512_AFE_MEMIF_UL10] = MT8512_AFE_IRQ21,
};

static const int aux_irqs[] = {
	MT8512_AFE_IRQ2,
};

#ifdef DEBUG_AFE_REGISTER_RW
int mt8512_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct mtk_base_afe *afe = context;

	mt8512_afe_enable_reg_rw_clk(afe);

	dev_notice(afe->dev, "%s reg 0x%x >>\n", __func__, reg);

	*val = readl(afe->base_addr + reg);

	dev_notice(afe->dev, "%s reg 0x%x val 0x%x <<\n", __func__, reg, *val);

	mt8512_afe_disable_reg_rw_clk(afe);

	return 0;
}

int mt8512_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct mtk_base_afe *afe = context;

	mt8512_afe_enable_reg_rw_clk(afe);

	dev_notice(afe->dev, "%s reg 0x%x val 0x%x >>\n", __func__, reg, val);

	writel(val, afe->base_addr + reg);

	dev_notice(afe->dev, "%s reg 0x%x val 0x%x <<\n", __func__, reg, val);

	mt8512_afe_disable_reg_rw_clk(afe);

	return 0;
}

static const struct regmap_bus mt8512_afe_regmap_bus = {
	.fast_io = true,
	.reg_write = mt8512_reg_write,
	.reg_read = mt8512_reg_read,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};
#endif

static const struct regmap_config mt8512_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = MAX_REGISTER,
	.cache_type = REGCACHE_NONE,
};

static irqreturn_t mt8512_afe_irq_handler(int irq, void *dev_id)
{
	struct mtk_base_afe *afe = dev_id;
	unsigned int val;
	unsigned int asys_irq_clr_bits = 0;
	unsigned int afe_irq_clr_bits = 0;
	unsigned int irq_status_bits;
	unsigned int irq_clr_bits;
	unsigned int mcu_irq_mask;
	int i, ret;

	ret = regmap_read(afe->regmap, AFE_IRQ_STATUS, &val);
	if (ret) {
		dev_info(afe->dev, "%s irq status err\n", __func__);
		afe_irq_clr_bits = AFE_IRQ_MCU_CLR_BITS;
		asys_irq_clr_bits = ASYS_IRQ_CLR_BITS;
		goto err_irq;
	}

	ret = regmap_read(afe->regmap, AFE_IRQ_MASK, &mcu_irq_mask);
	if (ret) {
		dev_info(afe->dev, "%s read irq mask err\n", __func__);
		afe_irq_clr_bits = AFE_IRQ_MCU_CLR_BITS;
		asys_irq_clr_bits = ASYS_IRQ_CLR_BITS;
		goto err_irq;
	}

	/* only clr cpu irq */
	val &= mcu_irq_mask;

	for (i = 0; i < MT8512_AFE_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];
		struct mtk_base_irq_data const *irq_data;

		if (memif->irq_usage < 0)
			continue;

		irq_data = afe->irqs[memif->irq_usage].irq_data;

		irq_status_bits = BIT(irq_data->irq_status_shift);
		irq_clr_bits = BIT(irq_data->irq_clr_shift);

		if (!(val & irq_status_bits))
			continue;

		if (irq_data->irq_clr_reg == ASYS_IRQ_CLR)
			asys_irq_clr_bits |= irq_clr_bits;
		else
			afe_irq_clr_bits |= irq_clr_bits;

		if (irq_data->custom_handler)
			irq_data->custom_handler(irq, dev_id);
		else
			snd_pcm_period_elapsed(memif->substream);
	}

	for (i = 0; i < ARRAY_SIZE(aux_irqs); i++) {
		struct mtk_base_irq_data const *irq_data;
		unsigned int irq_id = aux_irqs[i];

		irq_data = afe->irqs[irq_id].irq_data;

		irq_status_bits = BIT(irq_data->irq_status_shift);
		irq_clr_bits = BIT(irq_data->irq_clr_shift);

		if (!(val & irq_status_bits))
			continue;

		if (irq_data->irq_clr_reg == ASYS_IRQ_CLR)
			asys_irq_clr_bits |= irq_clr_bits;
		else
			afe_irq_clr_bits |= irq_clr_bits;

		if (irq_data->custom_handler)
			irq_data->custom_handler(irq, dev_id);
	}

err_irq:
	/* clear irq */
	if (asys_irq_clr_bits)
		regmap_write(afe->regmap, ASYS_IRQ_CLR, asys_irq_clr_bits);
	if (afe_irq_clr_bits)
		regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, afe_irq_clr_bits);

	return IRQ_HANDLED;
}

static int mt8512_afe_handle_etdm_force_on(struct mtk_base_afe *afe,
	bool probe)
{
	struct mt8512_afe_private *priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm1 = &priv->etdm_data[MT8512_ETDM1];
	struct mt8512_etdm_data *etdm2 = &priv->etdm_data[MT8512_ETDM2];
	bool force_apply_etdm1_in;
	bool force_apply_etdm2_out;
	bool force_apply_etdm2_in;

	force_apply_etdm1_in =
		(etdm1->force_on[SNDRV_PCM_STREAM_CAPTURE] &&
		 (!probe ||
		  etdm1->force_on_policy[SNDRV_PCM_STREAM_CAPTURE] ==
		  MT8512_ETDM_FORCE_ON_DEFAULT));

	force_apply_etdm2_out =
		(etdm2->force_on[SNDRV_PCM_STREAM_PLAYBACK] &&
		 (!probe ||
		  etdm2->force_on_policy[SNDRV_PCM_STREAM_PLAYBACK] ==
		  MT8512_ETDM_FORCE_ON_DEFAULT)) ||
		(etdm2->force_on[SNDRV_PCM_STREAM_CAPTURE] &&
		 (!probe ||
		  etdm2->force_on_policy[SNDRV_PCM_STREAM_CAPTURE] ==
		  MT8512_ETDM_FORCE_ON_DEFAULT) &&
		 (etdm2->clock_mode == MT8512_ETDM_SHARED_CLOCK));

	force_apply_etdm2_in =
		(etdm2->force_on[SNDRV_PCM_STREAM_CAPTURE] &&
		 (!probe ||
		  etdm2->force_on_policy[SNDRV_PCM_STREAM_CAPTURE] ==
		  MT8512_ETDM_FORCE_ON_DEFAULT)) ||
		(etdm2->force_on[SNDRV_PCM_STREAM_PLAYBACK] &&
		 (!probe ||
		  etdm2->force_on_policy[SNDRV_PCM_STREAM_PLAYBACK] ==
		  MT8512_ETDM_FORCE_ON_DEFAULT) &&
		 (etdm2->clock_mode == MT8512_ETDM_SHARED_CLOCK));

	if (!force_apply_etdm1_in &&
	    !force_apply_etdm2_out &&
	    !force_apply_etdm2_in)
		return 0;

	mt8512_afe_enable_main_clk(afe);

	if (force_apply_etdm1_in)
		mt8512_afe_etdm1_in_force_on(afe);

	if (force_apply_etdm2_out)
		mt8512_afe_etdm2_out_force_on(afe);

	if (force_apply_etdm2_in)
		mt8512_afe_etdm2_in_force_on(afe);

	return 0;
}

static int mt8512_afe_handle_etdm_force_off(struct mtk_base_afe *afe)
{
	struct mt8512_afe_private *priv = afe->platform_priv;
	struct mt8512_etdm_data *etdm1 = &priv->etdm_data[MT8512_ETDM1];
	struct mt8512_etdm_data *etdm2 = &priv->etdm_data[MT8512_ETDM2];
	bool force_apply_etdm1_in;
	bool force_apply_etdm2_out;
	bool force_apply_etdm2_in;
	bool need_tuner = false;
	unsigned int rate = 0;

	force_apply_etdm1_in = etdm1->force_on[SNDRV_PCM_STREAM_CAPTURE];

	force_apply_etdm2_out = etdm2->force_on[SNDRV_PCM_STREAM_PLAYBACK] ||
		(etdm2->force_on[SNDRV_PCM_STREAM_CAPTURE] &&
		(etdm2->clock_mode == MT8512_ETDM_SHARED_CLOCK));

	force_apply_etdm2_in = etdm2->force_on[SNDRV_PCM_STREAM_CAPTURE] ||
		(etdm2->force_on[SNDRV_PCM_STREAM_PLAYBACK] &&
		(etdm2->clock_mode == MT8512_ETDM_SHARED_CLOCK));

	if (!force_apply_etdm1_in &&
	    !force_apply_etdm2_out &&
	    !force_apply_etdm2_in)
		return 0;

	if (force_apply_etdm1_in) {
		rate = etdm1->force_rate[SNDRV_PCM_STREAM_CAPTURE];

		mt8512_afe_disable_etdm(afe, MT8512_ETDM1,
			SNDRV_PCM_STREAM_CAPTURE);

		need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
			  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM1,
					SNDRV_PCM_STREAM_CAPTURE))
			 || (!mt8512_afe_is_int_1x_en_low_power(afe)
			   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM1,
					SNDRV_PCM_STREAM_CAPTURE));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL1);
			} else {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_TDM_IN);
		mt8512_afe_disable_clk(afe, priv->clocks[MT8512_CLK_TDMIN_MCK]);

		etdm1->force_on_status[SNDRV_PCM_STREAM_CAPTURE] = false;
	}

	if (force_apply_etdm2_out) {
		rate = etdm2->force_rate[SNDRV_PCM_STREAM_PLAYBACK];

		mt8512_afe_disable_etdm(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_PLAYBACK);

		need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
			  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
					SNDRV_PCM_STREAM_PLAYBACK))
			 || (!mt8512_afe_is_int_1x_en_low_power(afe)
			   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
					SNDRV_PCM_STREAM_PLAYBACK));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL1);
			} else {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_I2S_OUT);
		mt8512_afe_disable_clk(afe,
			priv->clocks[MT8512_CLK_I2SOUT_MCK]);

		etdm2->force_on_status[SNDRV_PCM_STREAM_PLAYBACK] = false;
	}

	if (force_apply_etdm2_in) {
		rate = etdm2->force_rate[SNDRV_PCM_STREAM_CAPTURE];

		mt8512_afe_disable_etdm(afe, MT8512_ETDM2,
			SNDRV_PCM_STREAM_CAPTURE);

		need_tuner = (mt8512_afe_is_int_1x_en_low_power(afe)
			  && !mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
					SNDRV_PCM_STREAM_CAPTURE))
			 || (!mt8512_afe_is_int_1x_en_low_power(afe)
			   && mt8512_afe_is_etdm_low_power(afe, MT8512_ETDM2,
					SNDRV_PCM_STREAM_CAPTURE));

		if (need_tuner) {
			if (rate % 8000) {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL1);
			} else {
				mt8512_afe_disable_apll_associated_cfg(
					afe, MT8512_AFE_APLL2);
			}
		}

		mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_I2S_IN);
		mt8512_afe_disable_clk(afe, priv->clocks[MT8512_CLK_I2SIN_MCK]);

		etdm2->force_on_status[SNDRV_PCM_STREAM_CAPTURE] = false;
	}

	if (rate % 8000)
		mt8512_afe_disable_clk(afe, priv->clocks[MT8512_CLK_FA2SYS]);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static int mt8512_afe_runtime_suspend(struct device *dev)
{
	return 0;
}

static int mt8512_afe_runtime_resume(struct device *dev)
{
	return 0;
}

static int mt8512_afe_dev_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_dbg(afe->dev, "%s suspend %d %d >>\n", __func__,
		pm_runtime_status_suspended(dev), afe->suspended);

	if (pm_runtime_status_suspended(dev) || afe->suspended)
		return 0;

	afe->suspended = true;

	dev_dbg(afe->dev, "%s <<\n", __func__);

	return 0;
}

static int mt8512_afe_dev_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_dbg(afe->dev, "%s suspend %d %d >>\n", __func__,
		pm_runtime_status_suspended(dev), afe->suspended);

	if (pm_runtime_status_suspended(dev) || !afe->suspended)
		return 0;

	afe->suspended = false;

	dev_dbg(afe->dev, "%s <<\n", __func__);

	return 0;
}

static int mt8512_afe_init_registers(struct mtk_base_afe *afe)
{
	size_t i;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	static struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} init_regs[] = {
		{ AFE_IRQ_MASK, AFE_IRQ_MASK_EN_MASK, AFE_IRQ_MASK_EN_BITS },
		{ AFE_CONN_24BIT, GENMASK(31, 0), 0x0 },
		{ AFE_CONN_24BIT_1, GENMASK(17, 0), 0x0 },
		{ AFE_CONN_16BIT, GENMASK(31, 0), 0x0 },
		{ AFE_CONN_16BIT_1, GENMASK(17, 0), 0x0 },
		{ AFE_SINEGEN_CON0, AFE_SINEGEN_CON0_INIT_MASK,
		  AFE_SINEGEN_CON0_INIT_VAL },
	};

	mt8512_afe_enable_main_clk(afe);

	for (i = 0; i < ARRAY_SIZE(init_regs); i++)
		regmap_update_bits(afe->regmap, init_regs[i].reg,
				   init_regs[i].mask, init_regs[i].val);

	if (afe_priv->use_bypass_afe_pinmux)
		regmap_update_bits(afe->regmap, ETDM_IN2_CON0,
				   ETDM_CON0_SLAVE_MODE,
				   ETDM_CON0_SLAVE_MODE);

	mt8512_afe_disable_main_clk(afe);

	return 0;
}

static bool mt8512_afe_validate_sram(struct mtk_base_afe *afe,
	u32 phy_addr, u32 size)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	if (afe_priv->afe_sram_phy_addr &&
	    (phy_addr >= afe_priv->afe_sram_phy_addr) &&
	    ((phy_addr + size) <=
	     (afe_priv->afe_sram_phy_addr + afe_priv->afe_sram_size))) {
		return true;
	}

	return false;
}

static void __iomem *mt8512_afe_sram_pa_to_va(struct mtk_base_afe *afe,
	u32 phy_addr)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	u32 off = phy_addr - afe_priv->afe_sram_phy_addr;

	if (afe_priv->afe_sram_phy_addr &&
	    (off >= 0) && (off < afe_priv->afe_sram_size)) {
		return ((unsigned char *)afe_priv->afe_sram_vir_addr + off);
	}

	return NULL;
}

static void mt8512_afe_parse_of(struct mtk_base_afe *afe,
				struct device_node *np)
{
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	size_t i;
	int ret;
	unsigned int stream;
	unsigned int temps[4];
	char prop[128];
	unsigned int val[2];
	struct mt8512_etdm_data *etdm_data;
	struct mt8512_gasrc_data *gasrc_data;
	struct {
		char *name;
		unsigned int val;
	} of_fe_table[] = {
		{ "dlm",	MT8512_AFE_MEMIF_DLM },
		{ "dl2",	MT8512_AFE_MEMIF_DL2 },
		{ "dl3",	MT8512_AFE_MEMIF_DL3 },
		{ "dl6",	MT8512_AFE_MEMIF_DL6 },
		{ "ul1",	MT8512_AFE_MEMIF_UL1 },
		{ "ul2",	MT8512_AFE_MEMIF_UL2 },
		{ "ul3",	MT8512_AFE_MEMIF_UL3 },
		{ "ul4",	MT8512_AFE_MEMIF_UL4 },
		{ "ul5",	MT8512_AFE_MEMIF_UL5 },
		{ "ul8",	MT8512_AFE_MEMIF_UL8 },
		{ "ul9",	MT8512_AFE_MEMIF_UL9 },
		{ "ul10",	MT8512_AFE_MEMIF_UL10 },
	};
	struct {
		char *name;
		unsigned int set;
		unsigned int stream;
	} of_afe_etdms[] = {
		{"etdm1-in", MT8512_ETDM1, SNDRV_PCM_STREAM_CAPTURE},
		{"etdm2-out", MT8512_ETDM2, SNDRV_PCM_STREAM_PLAYBACK},
		{"etdm2-in", MT8512_ETDM2, SNDRV_PCM_STREAM_CAPTURE},
	};
	struct {
		char *name;
		unsigned int set;
	} of_afe_gasrcs[] = {
		{"gasrc0", MT8512_GASRC0},
		{"gasrc1", MT8512_GASRC1},
		{"gasrc2", MT8512_GASRC2},
		{"gasrc3", MT8512_GASRC3},
	};

	ret = of_property_read_u32_array(np, "mediatek,etdm-clock-modes",
					 &temps[0],
					 MT8512_ETDM_SETS);
	if (ret == 0) {
		for (i = 0; i < MT8512_ETDM_SETS; i++)
			afe_priv->etdm_data[i].clock_mode = temps[i];
	}

	ret = of_property_read_u32_array(np, "mediatek,etdm-out-data-modes",
					 &temps[0],
					 MT8512_ETDM_SETS);
	if (ret == 0) {
		stream = SNDRV_PCM_STREAM_PLAYBACK;
		for (i = 0; i < MT8512_ETDM_SETS; i++)
			afe_priv->etdm_data[i].data_mode[stream] = temps[i];
	}

	ret = of_property_read_u32_array(np, "mediatek,etdm-in-data-modes",
					 &temps[0],
					 MT8512_ETDM_SETS);
	if (ret == 0) {
		stream = SNDRV_PCM_STREAM_CAPTURE;
		for (i = 0; i < MT8512_ETDM_SETS; i++)
			afe_priv->etdm_data[i].data_mode[stream] = temps[i];
	}

	for (i = 0; i < ARRAY_SIZE(of_afe_etdms); i++) {
		unsigned int stream = of_afe_etdms[i].stream;
		const char *str;
		bool force_on = false;
		bool force_on_1st_trigger = false;

		etdm_data = &afe_priv->etdm_data[of_afe_etdms[i].set];

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-on",
			 of_afe_etdms[i].name);
		force_on = of_property_read_bool(np, prop);

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-on-1st-trigger",
			 of_afe_etdms[i].name);
		force_on_1st_trigger = of_property_read_bool(np, prop);

		if (force_on) {
			etdm_data->force_on[stream] = true;
			etdm_data->force_on_policy[stream] =
				MT8512_ETDM_FORCE_ON_DEFAULT;
		} else if (force_on_1st_trigger) {
			etdm_data->force_on[stream] = true;
			etdm_data->force_on_policy[stream] =
				MT8512_ETDM_FORCE_ON_1ST_TRIGGER;
		}

		/* skip related force-on properties parsing */
		if (!etdm_data->force_on[stream])
			continue;

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-format",
			 of_afe_etdms[i].name);
		ret = of_property_read_string(np, prop, &str);
		if (ret == 0) {
			if (strcmp(str, "i2s") == 0) {
				etdm_data->format[stream] =
					MT8512_ETDM_FORMAT_I2S;
			} else if (strcmp(str, "right_j") == 0) {
				etdm_data->format[stream] =
					MT8512_ETDM_FORMAT_RJ;
			} else if (strcmp(str, "left_j") == 0) {
				etdm_data->format[stream] =
					MT8512_ETDM_FORMAT_LJ;
			} else if (strcmp(str, "dsp_a") == 0) {
				etdm_data->format[stream] =
					MT8512_ETDM_FORMAT_DSPA;
			} else if (strcmp(str, "dsp_b") == 0) {
				etdm_data->format[stream] =
					MT8512_ETDM_FORMAT_DSPB;
			}
		}

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-mclk-freq",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0)
			etdm_data->mclk_freq[stream] = temps[0];

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-lrck-inverse",
			 of_afe_etdms[i].name);
		etdm_data->lrck_inv[stream] =
			of_property_read_bool(np, prop);

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-bck-inverse",
			 of_afe_etdms[i].name);
		etdm_data->bck_inv[stream] =
			of_property_read_bool(np, prop);

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-lrck-width",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0)
			etdm_data->lrck_width[stream] = temps[0];

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-rate",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0)
			etdm_data->force_rate[stream] = temps[0];

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-channels",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0)
			etdm_data->force_channels[stream] = temps[0];

		snprintf(prop, sizeof(prop),
			 "mediatek,%s-force-bit-width",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0)
			etdm_data->force_bit_width[stream] = temps[0];

		snprintf(prop, sizeof(prop),
			"mediatek,%s-force-master-clock",
			 of_afe_etdms[i].name);
		ret = of_property_read_u32(np, prop, &temps[0]);
		if (ret == 0) {
			if (temps[0])
				etdm_data->slave_mode[stream] = false;
			else
				etdm_data->slave_mode[stream] = true;
		}
	}

	ret = of_property_read_u32_array(np, "mediatek,dmic-two-wire-mode",
					 &temps[0],
					 1);
	if (ret == 0)
		afe_priv->dmic_data.two_wire_mode = temps[0];

	ret = of_property_read_u32_array(np, "mediatek,dmic-clk-phases",
					 &temps[0],
					 2);
	if (ret == 0) {
		afe_priv->dmic_data.clk_phase_sel_ch1 = temps[0];
		afe_priv->dmic_data.clk_phase_sel_ch2 = temps[1];
	} else if (!afe_priv->dmic_data.two_wire_mode) {
		afe_priv->dmic_data.clk_phase_sel_ch1 = 0;
		afe_priv->dmic_data.clk_phase_sel_ch2 = 4;
	}

	ret = of_property_read_u32_array(np, "mediatek,dmic-src-sels",
					 &afe_priv->dmic_data.dmic_src_sel[0],
					 DMIC_MAX_CH);
	if (ret != 0) {
		if (afe_priv->dmic_data.two_wire_mode) {
			afe_priv->dmic_data.dmic_src_sel[0] = 0;
			afe_priv->dmic_data.dmic_src_sel[1] = 1;
			afe_priv->dmic_data.dmic_src_sel[2] = 2;
			afe_priv->dmic_data.dmic_src_sel[3] = 3;
			afe_priv->dmic_data.dmic_src_sel[4] = 4;
			afe_priv->dmic_data.dmic_src_sel[5] = 5;
			afe_priv->dmic_data.dmic_src_sel[6] = 6;
			afe_priv->dmic_data.dmic_src_sel[7] = 7;
		} else {
			afe_priv->dmic_data.dmic_src_sel[0] = 0;
			afe_priv->dmic_data.dmic_src_sel[2] = 1;
			afe_priv->dmic_data.dmic_src_sel[4] = 2;
			afe_priv->dmic_data.dmic_src_sel[6] = 3;
		}
	}

	afe_priv->dmic_data.iir_on = of_property_read_bool(np,
		"mediatek,dmic-iir-on");

	of_property_read_u32(np,
		"mediatek,dmic-setup-time-us",
		&afe_priv->dmic_data.setup_time_us);

	for (i = 0; i < ARRAY_SIZE(of_fe_table); i++) {
		bool valid_sram;
		struct mt8512_fe_dai_data *fe_data;

		memset(val, 0, sizeof(val));

		snprintf(prop, sizeof(prop), "mediatek,%s-use-sram",
			 of_fe_table[i].name);
		ret = of_property_read_u32_array(np, prop, &val[0], 2);
		if (ret)
			continue;

		valid_sram = mt8512_afe_validate_sram(afe, val[0], val[1]);
		if (!valid_sram) {
			dev_info(afe->dev, "%s %s validate 0x%x 0x%x fail\n",
				 __func__, of_fe_table[i].name,
				 val[0], val[1]);
			continue;
		}

		fe_data = &afe_priv->fe_data[of_fe_table[i].val];

		fe_data->sram_phy_addr = val[0];
		fe_data->sram_size = val[1];
		fe_data->sram_vir_addr = mt8512_afe_sram_pa_to_va(afe,
			fe_data->sram_phy_addr);
	}

	afe_priv->use_bypass_afe_pinmux = of_property_read_bool(np,
		"mediatek,use-bypass-afe-pinmux");

	of_property_read_u32(np,
		"mediatek,spdif-in-opt-mux",
		&afe_priv->spdif_in_data.ports_mux[SPDIF_IN_PORT_OPT]);

	of_property_read_u32(np,
		"mediatek,spdif-in-coa-mux",
		&afe_priv->spdif_in_data.ports_mux[SPDIF_IN_PORT_COAXIAL]);

	of_property_read_u32(np,
		"mediatek,spdif-in-arc-mux",
		&afe_priv->spdif_in_data.ports_mux[SPDIF_IN_PORT_ARC]);

	for (i = 0; i < ARRAY_SIZE(of_afe_gasrcs); i++) {
		memset(val, 0, sizeof(val));
		gasrc_data = &afe_priv->gasrc_data[of_afe_gasrcs[i].set];

		snprintf(prop, sizeof(prop), "mediatek,%s-fix-rate",
				 of_afe_gasrcs[i].name);
		ret = of_property_read_u32_array(np, prop, &val[0], 2);
		if (ret)
			continue;

		gasrc_data->fix_input_rate = val[0];
		gasrc_data->fix_output_rate  = val[1];
	}
}

static int mt8512_afe_pcm_probe(struct snd_soc_platform *platform)
{
	int ret;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);

	ret = mt8512_afe_add_controls(platform);
	if (ret)
		return ret;

	ret = mt8512_afe_handle_etdm_force_on(afe, true);
	if (ret)
		return ret;

	return 0;
}

const struct snd_soc_platform_driver mt8512_afe_pcm_platform = {
	.probe = mt8512_afe_pcm_probe,
	.pcm_new = mtk_afe_pcm_new,
	.pcm_free = mtk_afe_pcm_free,
	.ops = &mtk_afe_pcm_ops,
};

static int mt8512_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i, sel_irq;
	unsigned int irq_id;
	struct mtk_base_afe *afe;
	struct mt8512_afe_private *afe_priv;
	struct resource *res;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(33));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	afe_priv = afe->platform_priv;
	if (!afe_priv)
		return -ENOMEM;

	spin_lock_init(&afe_priv->afe_ctrl_lock);

	mutex_init(&afe_priv->afe_clk_mutex);

	mutex_init(&afe_priv->block_dpidle_mutex);

	afe->dev = &pdev->dev;

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_info(afe->dev, "np %s no irq\n", afe->dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(afe->dev, irq_id, mt8512_afe_irq_handler,
			       0, "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_info(afe->dev, "could not request_irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		afe_priv->afe_sram_vir_addr =
			devm_ioremap_resource(&pdev->dev, res);
		if (!IS_ERR(afe_priv->afe_sram_vir_addr)) {
			afe_priv->afe_sram_phy_addr = res->start;
			afe_priv->afe_sram_size = resource_size(res);
		}
	}

	/* initial audio related clock */
	ret = mt8512_afe_init_audio_clk(afe);
	if (ret) {
		dev_info(afe->dev, "mt8512_afe_init_audio_clk fail\n");
		return ret;
	}

#ifdef DEBUG_AFE_REGISTER_RW
	afe->regmap = devm_regmap_init(&pdev->dev,
		&mt8512_afe_regmap_bus, afe,
		&mt8512_afe_regmap_config);
#else
	afe->regmap = devm_regmap_init_mmio_clk(&pdev->dev,
		"top_aud_26m", afe->base_addr,
		&mt8512_afe_regmap_config);
#endif
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	/* memif % irq initialize*/
	afe->memif_size = MT8512_AFE_MEMIF_NUM;
	afe->memif = devm_kcalloc(afe->dev, afe->memif_size,
				  sizeof(*afe->memif), GFP_KERNEL);
	if (!afe->memif)
		return -ENOMEM;

	afe->irqs_size = MT8512_AFE_IRQ_NUM;
	afe->irqs = devm_kcalloc(afe->dev, afe->irqs_size,
				 sizeof(*afe->irqs), GFP_KERNEL);
	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		sel_irq = memif_specified_irqs[i];
		if (sel_irq >= 0) {
			afe->memif[i].irq_usage = sel_irq;
			afe->memif[i].const_irq = 1;
			afe->irqs[sel_irq].irq_occupyed = true;
		} else {
			afe->memif[i].irq_usage = -1;
		}
	}

	afe->mtk_afe_hardware = &mt8512_afe_hardware;
	afe->memif_fs = mt8512_memif_fs;
	afe->irq_fs = mt8512_irq_fs;
	afe->alloc_dmabuf = mt8512_alloc_dmabuf;
	afe->free_dmabuf = mt8512_free_dmabuf;

	platform_set_drvdata(pdev, afe);
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	g_priv = afe;
#endif
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		dev_info(afe->dev, "%s pm_runtime not enabled\n", __func__);
		ret = mt8512_afe_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	pm_runtime_get_sync(&pdev->dev);

	afe->reg_back_up_list = mt8512_afe_backup_list;
	afe->reg_back_up_list_num = ARRAY_SIZE(mt8512_afe_backup_list);
	afe->runtime_resume = mt8512_afe_runtime_resume;
	afe->runtime_suspend = mt8512_afe_runtime_suspend;
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	afe_priv->adsp_data.get_afe_memif_sram =
		mt8512_adsp_get_afe_memif_sram;
	afe_priv->adsp_data.set_afe_memif =
		mt8512_adsp_set_afe_memif;
	afe_priv->adsp_data.set_afe_memif_enable =
		mt8512_adsp_set_afe_memif_enable;
	afe_priv->adsp_data.set_afe_init =
		mt8512_adsp_set_afe_init;
	afe_priv->adsp_data.set_afe_uninit =
		mt8512_adsp_set_afe_uninit;
	afe_priv->adsp_data.set_afe_interconn =
		mt8512_adsp_set_afe_interconn;
#endif

	/* keep these cg open for dapm to read/write audio register */
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_enable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_enable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);

	mt8512_afe_parse_of(afe, pdev->dev.of_node);

	ret = snd_soc_register_platform(&pdev->dev, &mt8512_afe_pcm_platform);
	if (ret)
		goto err_platform;

	ret = snd_soc_register_component(&pdev->dev,
					 &mt8512_afe_pcm_dai_component,
					 mt8512_afe_pcm_dais,
					 ARRAY_SIZE(mt8512_afe_pcm_dais));
	if (ret)
		goto err_component;

	mt8512_afe_init_registers(afe);

	mt8512_afe_init_debugfs(afe);

	dev_info(&pdev->dev, "MT8512 AFE driver initialized.\n");
	return 0;

err_component:
	snd_soc_unregister_platform(&pdev->dev);
err_platform:
	pm_runtime_put_sync(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int mt8512_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);
	struct mt8512_afe_private *afe_priv = afe->platform_priv;

	mt8512_afe_cleanup_debugfs(afe);

	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE_CONN);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_AFE);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS_HOPPING);
	mt8512_afe_disable_top_cg(afe, MT8512_TOP_CG_A1SYS);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_TOP_AUD_BUS]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUD_26M_CG]);
	mt8512_afe_disable_clk(afe, afe_priv->clocks[MT8512_CLK_AUDIO_CG]);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt8512_afe_runtime_suspend(&pdev->dev);

	pm_runtime_put_sync(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt8512_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8512-afe-pcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8512_afe_pcm_dt_match);

static const struct dev_pm_ops mt8512_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt8512_afe_dev_runtime_suspend,
			   mt8512_afe_dev_runtime_resume, NULL)
};

static struct platform_driver mt8512_afe_pcm_driver = {
	.driver = {
		   .name = "mt8512-afe-pcm",
		   .of_match_table = mt8512_afe_pcm_dt_match,
		   .pm = &mt8512_afe_pm_ops,
	},
	.probe = mt8512_afe_pcm_dev_probe,
	.remove = mt8512_afe_pcm_dev_remove,
};

module_platform_driver(mt8512_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver");
MODULE_AUTHOR("Mengge Wang <mengge.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
