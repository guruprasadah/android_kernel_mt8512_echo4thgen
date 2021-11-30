/*
 * mt8512_afe_common.h  --  Mediatek 8512 audio driver common definitions
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

#ifndef _MT8512_AFE_COMMON_H_
#define _MT8512_AFE_COMMON_H_

#define COMMON_CLOCK_FRAMEWORK_API
/* #define DEBUG_AFE_REGISTER_RW */

#include <linux/clk.h>
#include <linux/regmap.h>
#include <sound/asound.h>
#include "../common/mtk-base-afe.h"

enum {
	MT8512_AFE_MEMIF_DLM,
	MT8512_AFE_MEMIF_DL2,
	MT8512_AFE_MEMIF_DL3,
	MT8512_AFE_MEMIF_DL6,
	MT8512_AFE_MEMIF_UL1,
	MT8512_AFE_MEMIF_UL2,
	MT8512_AFE_MEMIF_UL3,
	MT8512_AFE_MEMIF_UL4,
	MT8512_AFE_MEMIF_UL5,
	MT8512_AFE_MEMIF_UL8,
	MT8512_AFE_MEMIF_UL9,
	MT8512_AFE_MEMIF_UL10,
	MT8512_AFE_MEMIF_NUM,
	MT8512_AFE_BACKEND_BASE = MT8512_AFE_MEMIF_NUM,
	MT8512_AFE_IO_ETDM1_IN = MT8512_AFE_BACKEND_BASE,
	MT8512_AFE_IO_ETDM2_OUT,
	MT8512_AFE_IO_ETDM2_IN,
	MT8512_AFE_IO_PCM1,
	MT8512_AFE_IO_VIRTUAL_DL_SRC,
	MT8512_AFE_IO_DMIC,
	MT8512_AFE_IO_INT_ADDA,
	MT8512_AFE_IO_GASRC0,
	MT8512_AFE_IO_GASRC1,
	MT8512_AFE_IO_GASRC2,
	MT8512_AFE_IO_GASRC3,
	MT8512_AFE_IO_SPDIF_IN,
	MT8512_AFE_IO_MULTI_IN,
	MT8512_AFE_BACKEND_END,
	MT8512_AFE_BACKEND_NUM = (MT8512_AFE_BACKEND_END -
				  MT8512_AFE_BACKEND_BASE),
};

enum {
	MT8512_AFE_IRQ1, /* SPDIF OUT */
	MT8512_AFE_IRQ2, /* SPDIF IN DETECT */
	MT8512_AFE_IRQ3, /* SPDIF IN DATA */
	MT8512_AFE_IRQ4, /* TSF DLM */
	MT8512_AFE_IRQ5, /* TSF DL2 */
	MT8512_AFE_IRQ6, /* TSF DL3 */
	MT8512_AFE_IRQ7, /* TSF TDMOUT */
	MT8512_AFE_IRQ8, /* TSF ACC1 */
	MT8512_AFE_IRQ9, /* TSF ACC2 */
	MT8512_AFE_IRQ10,
	MT8512_AFE_IRQ11,
	MT8512_AFE_IRQ12,
	MT8512_AFE_IRQ13,
	MT8512_AFE_IRQ14,
	MT8512_AFE_IRQ15,
	MT8512_AFE_IRQ16,
	MT8512_AFE_IRQ17,
	MT8512_AFE_IRQ18,
	MT8512_AFE_IRQ19,
	MT8512_AFE_IRQ20,
	MT8512_AFE_IRQ21,
	MT8512_AFE_IRQ22,
	MT8512_AFE_IRQ23,
	MT8512_AFE_IRQ24,
	MT8512_AFE_IRQ25,
	MT8512_AFE_IRQ_NUM,
};

enum {
	MT8512_TOP_CG_AFE,
	MT8512_TOP_CG_APLL,
	MT8512_TOP_CG_APLL2,
	MT8512_TOP_CG_DAC,
	MT8512_TOP_CG_DAC_PREDIS,
	MT8512_TOP_CG_ADC,
	MT8512_TOP_CG_TML,
	MT8512_TOP_CG_UPLINK_TML,
	MT8512_TOP_CG_APLL_TUNER,
	MT8512_TOP_CG_APLL2_TUNER,
	MT8512_TOP_CG_SPIDFIN_TUNER_APLL,
	MT8512_TOP_CG_A1SYS_HOPPING,
	MT8512_TOP_CG_I2S_IN,
	MT8512_TOP_CG_TDM_IN,
	MT8512_TOP_CG_I2S_OUT,
	MT8512_TOP_CG_ASRC11,
	MT8512_TOP_CG_ASRC12,
	MT8512_TOP_CG_A1SYS,
	MT8512_TOP_CG_A2SYS,
	MT8512_TOP_CG_DL_ASRC,
	MT8512_TOP_CG_AFE_CONN,
	MT8512_TOP_CG_PCMIF,
	MT8512_TOP_CG_GASRC0,
	MT8512_TOP_CG_GASRC1,
	MT8512_TOP_CG_GASRC2,
	MT8512_TOP_CG_GASRC3,
	MT8512_TOP_CG_DMIC0,
	MT8512_TOP_CG_DMIC1,
	MT8512_TOP_CG_DMIC2,
	MT8512_TOP_CG_DMIC3,
	MT8512_TOP_CG_A1SYS_TIMING,
	MT8512_TOP_CG_A2SYS_TIMING,
	MT8512_TOP_CG_26M_TIMING,
	MT8512_TOP_CG_LP_MODE,
	MT8512_TOP_CG_MULTI_IN,
	MT8512_TOP_CG_INTDIR,
	MT8512_TOP_CG_NUM
};

enum {
	MT8512_CLK_TOP_AUD_26M,
	MT8512_CLK_TOP_AUD_BUS,
	MT8512_CLK_FA1SYS,
	MT8512_CLK_FA2SYS,
	MT8512_CLK_FAPLL1,
	MT8512_CLK_FAPLL2,
	MT8512_CLK_AUD1,
	MT8512_CLK_AUD2,
	MT8512_CLK_FASM_L,
	MT8512_CLK_FASM_M,
	MT8512_CLK_FASM_H,
	MT8512_CLK_SPDIF_IN,
	MT8512_CLK_I2SIN_M_SEL,
	MT8512_CLK_TDMIN_M_SEL,
	MT8512_CLK_I2SOUT_M_SEL,
	MT8512_CLK_APLL12_DIV0,
	MT8512_CLK_APLL12_DIV1,
	MT8512_CLK_APLL12_DIV2,
	MT8512_CLK_I2SIN_MCK,
	MT8512_CLK_TDMIN_MCK,
	MT8512_CLK_I2SOUT_MCK,
	MT8512_CLK_CLK26M,
	MT8512_CLK_AUD_SYSPLL1_D4,
	MT8512_CLK_AUDIO_CG,
	MT8512_CLK_AUD_26M_CG,
	MT8512_CLK_NUM
};

enum {
	MT8512_AFE_APLL1 = 0,
	MT8512_AFE_APLL2,
	MT8512_AFE_APLL_NUM,
};

enum {
	MT8512_ETDM1 = 0,
	MT8512_ETDM2,
	MT8512_ETDM_SETS,
};

enum {
	MT8512_ETDM_DATA_ONE_PIN = 0,
	MT8512_ETDM_DATA_MULTI_PIN,
};

enum {
	MT8512_ETDM_SEPARATE_CLOCK = 0,
	MT8512_ETDM_SHARED_CLOCK,
};

enum {
	MT8512_ETDM_FORMAT_I2S = 0,
	MT8512_ETDM_FORMAT_LJ,
	MT8512_ETDM_FORMAT_RJ,
	MT8512_ETDM_FORMAT_EIAJ,
	MT8512_ETDM_FORMAT_DSPA,
	MT8512_ETDM_FORMAT_DSPB,
};

enum {
	MT8512_PCM_FORMAT_I2S = 0,
	MT8512_PCM_FORMAT_EIAJ,
	MT8512_PCM_FORMAT_PCMA,
	MT8512_PCM_FORMAT_PCMB,
};

enum {
	MT8512_MULTI_IN_FORMAT_I2S = 0,
	MT8512_MULTI_IN_FORMAT_LJ,
	MT8512_MULTI_IN_FORMAT_RJ,
};

enum {
	MT8512_FS_8K = 0,
	MT8512_FS_12K,
	MT8512_FS_16K,
	MT8512_FS_24K,
	MT8512_FS_32K,
	MT8512_FS_48K,
	MT8512_FS_96K,
	MT8512_FS_192K,
	MT8512_FS_384K,
	MT8512_FS_ETDMOUT1_1X_EN,
	MT8512_FS_ETDMOUT2_1X_EN,
	MT8512_FS_ETDMIN1_1X_EN = 12,
	MT8512_FS_ETDMIN2_1X_EN,
	MT8512_FS_EXT_PCM_1X_EN = 15,
	MT8512_FS_7P35K,
	MT8512_FS_11P025K,
	MT8512_FS_14P7K,
	MT8512_FS_22P05K,
	MT8512_FS_29P4K,
	MT8512_FS_44P1K,
	MT8512_FS_88P2K,
	MT8512_FS_176P4K,
	MT8512_FS_352P8K,
	MT8512_FS_ETDMIN1_NX_EN,
	MT8512_FS_ETDMIN2_NX_EN,
	MT8512_FS_AMIC_1X_EN_ASYNC = 28,
	MT8512_FS_DL_1X_EN = 30,
};

enum {
	SPDIF_IN_PORT_NONE = 0,
	SPDIF_IN_PORT_OPT,
	SPDIF_IN_PORT_COAXIAL,
	SPDIF_IN_PORT_ARC,
	SPDIF_IN_PORT_NUM
};

enum {
	SPDIF_IN_MUX_0 = 0,
	SPDIF_IN_MUX_1,
	SPDIF_IN_MUX_2,
};

enum {
	MT8512_ETDM_FORCE_ON_DEFAULT = 0,
	MT8512_ETDM_FORCE_ON_1ST_TRIGGER,
};

enum {
	MT8512_AFE_DEBUGFS_ETDM,
	MT8512_AFE_DEBUGFS_MEMIF,
	MT8512_AFE_DEBUGFS_IRQ,
	MT8512_AFE_DEBUGFS_CONN,
	MT8512_AFE_DEBUGFS_ADDA,
	MT8512_AFE_DEBUGFS_GASRC,
	MT8512_AFE_DEBUGFS_SPDIF,
	MT8512_AFE_DEBUGFS_DBG,
	MT8512_AFE_DEBUGFS_NUM,
};

enum {
	MT8512_AFE_IRQ_DIR_MCU = 0,
	MT8512_AFE_IRQ_DIR_DSP,
	MT8512_AFE_IRQ_DIR_BOTH,
};

struct mt8512_fe_dai_data {
	bool slave_mode;
	bool use_sram;
	unsigned int sram_phy_addr;
	void __iomem *sram_vir_addr;
	unsigned int sram_size;
};

struct mt8512_be_dai_data {
	bool prepared[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int fmt_mode;
};

struct mt8512_etdm_data {
	int occupied[SNDRV_PCM_STREAM_LAST + 1];
	int active[SNDRV_PCM_STREAM_LAST + 1];
	bool slave_mode[SNDRV_PCM_STREAM_LAST + 1];
	bool lrck_inv[SNDRV_PCM_STREAM_LAST + 1];
	bool bck_inv[SNDRV_PCM_STREAM_LAST + 1];
	bool enable_interlink[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int lrck_width[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int data_mode[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int format[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int mclk_freq[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int clock_mode;
	bool force_on[SNDRV_PCM_STREAM_LAST + 1];
	bool force_on_status[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_on_policy[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_rate[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_channels[SNDRV_PCM_STREAM_LAST + 1];
	unsigned int force_bit_width[SNDRV_PCM_STREAM_LAST + 1];
};

struct mt8512_pcm_intf_data {
	bool slave_mode;
	bool lrck_inv;
	bool bck_inv;
	unsigned int format;
};

struct mt8512_multi_in_data {
	bool lrck_inv;
	bool bck_inv;
	unsigned int format;
	unsigned int period_update_bytes;
	unsigned int notify_irq_count;
	unsigned int current_irq_count;
};

struct mt8512_spdif_in_data {
	unsigned int port;
	unsigned int rate;
	unsigned int ch_status[6];
	unsigned int ports_mux[SPDIF_IN_PORT_NUM];
};

struct mt8512_etdm_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
};

struct mt8512_control_data {
	bool bypass_cm0;
	bool bypass_cm1;
	bool spdif_output_iec61937;
};

#define DMIC_MAX_CH (8)

enum {
	DMIC_3M25M = 0,
	DMIC_1P625M = 1,
	DMIC_812P5K = 2,
	DMIC_406P25K = 3
};

struct mt8512_dmic_data {
	bool two_wire_mode;
	unsigned int clk_phase_sel_ch1;
	unsigned int clk_phase_sel_ch2;
	unsigned int dmic_src_sel[DMIC_MAX_CH];
	bool iir_on;
	unsigned int setup_time_us;
	unsigned int ul_mode;
};

enum {
	MT8512_GASRC0 = 0,
	MT8512_GASRC1,
	MT8512_GASRC2,
	MT8512_GASRC3,
	MT8512_GASRC_NUM,
};

struct mt8512_gasrc_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
	unsigned int con6;
	unsigned int con7;
	unsigned int con10;
	unsigned int con11;
	unsigned int con13;
	unsigned int con14;
};

struct mt8512_gasrc_data {
	unsigned int input_mux;
	unsigned int output_mux;
	unsigned int fix_input_rate;
	unsigned int fix_output_rate;
	bool cali_tx;
	bool cali_rx;
	bool one_heart;
	bool iir_on;
	bool duplex;
	bool op_freq_45m;
	unsigned int cali_cycles;
	bool re_enable[SNDRV_PCM_STREAM_LAST + 1];
	atomic_t ref_cnt;
};

enum mt8512_afe_gasrc_mux {
	MUX_GASRC_8CH = 0,
	MUX_GASRC_6CH,
	MUX_GASRC_4CH,
	MUX_GASRC_2CH,
};

struct mt8512_afe_gasrc_mux_map {
	int gasrc_id;
	int idx;
	int mux;
};

enum mt8512_afe_gasrc_lrck_sel_src {
	MT8512_AFE_GASRC_LRCK_SEL_ETDM_IN2 = 0,
	MT8512_AFE_GASRC_LRCK_SEL_ETDM_IN1,
	MT8512_AFE_GASRC_LRCK_SEL_ETDM_OUT2,
	MT8512_AFE_GASRC_LRCK_SEL_ETDM_OUT1,
	MT8512_AFE_GASRC_LRCK_SEL_PCM_IF,
	MT8512_AFE_GASRC_LRCK_SEL_UL_VIRTUAL,
};

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mt8512_adsp_data {
	/* information adsp supply */
	bool adsp_on;
	int (*hostless_active)(void);
	/* information afe supply */
	int (*set_afe_memif)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int channels,
				       snd_pcm_format_t format);
	int (*set_afe_memif_enable)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int rate,
				       unsigned int period_size,
				       int enable);
	void (*get_afe_memif_sram)(struct mtk_base_afe *afe,
				       int memif_id,
				       unsigned int *paddr,
				       unsigned int *size);
	void (*set_afe_init)(struct mtk_base_afe *afe);
	void (*set_afe_uninit)(struct mtk_base_afe *afe);
	void (*set_afe_interconn)(struct mtk_base_afe *afe, int enable);
};
#endif

struct mt8512_afe_private {
	struct clk *clocks[MT8512_CLK_NUM];
	struct mt8512_fe_dai_data fe_data[MT8512_AFE_MEMIF_NUM];
	struct mt8512_be_dai_data be_data[MT8512_AFE_BACKEND_NUM];
	struct mt8512_etdm_data etdm_data[MT8512_ETDM_SETS];
	struct mt8512_pcm_intf_data pcm_intf_data;
	struct mt8512_multi_in_data multi_in_data;
	struct mt8512_spdif_in_data spdif_in_data;
	struct mt8512_control_data ctrl_data;
	struct mt8512_dmic_data dmic_data;
	struct mt8512_gasrc_data gasrc_data[MT8512_GASRC_NUM];
#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
	struct mt8512_adsp_data adsp_data;
#endif
	int afe_on_ref_cnt;
	int top_cg_ref_cnt[MT8512_TOP_CG_NUM];
	void __iomem *afe_sram_vir_addr;
	u32 afe_sram_phy_addr;
	u32 afe_sram_size;
	bool use_bypass_afe_pinmux;
	/* locks */
	spinlock_t afe_ctrl_lock;
	struct mutex afe_clk_mutex;
	struct regmap *topckgen;
	struct regmap *scpsys;
	int block_dpidle_ref_cnt;
	struct mutex block_dpidle_mutex;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry[MT8512_AFE_DEBUGFS_NUM];
#endif
	int apll_tuner_ref_cnt[MT8512_AFE_APLL_NUM];
};

bool mt8512_afe_rate_supported(unsigned int rate, unsigned int id);
bool mt8512_afe_channel_supported(unsigned int channel, unsigned int id);

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
struct mtk_base_afe *mt8512_afe_pcm_get_info(void);
#endif

#endif
