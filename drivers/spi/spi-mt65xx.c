/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/platform_data/spi-mt65xx.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>

enum spi_regs {
	SPI_TX_SRC_REG,
	SPI_RX_DST_REG,
	SPI_TX_DATA_REG,
	SPI_RX_DATA_REG,
	SPI_STATUS1_REG,
	SPI_PAD_SEL_REG,
	SPI_TX_SRC_REG_64,
	SPI_RX_DST_REG_64,

	/* only for multipin transfer IC*/
	SPI_CTROL0_REG,
	SPI_CFG3_REG,

	/* virtual regs for merge diffrence regs which with same func*/
	VSPI_TRIG_REG,
	VSPI_IE_REG,
	VSPI_CFG_XFER_REG,
	VSPI_CTROL_REG,
	VSPI_INT_REG,
	VSPI_CFG_MSG_REG,
	VSPI_TIM0_REG,
	VSPI_TIM1_REG,
};

static const u16 normal_spi_regs[] = {
	[SPI_TX_SRC_REG] =		0x0008,
	[SPI_RX_DST_REG] =		0x000c,
	[SPI_TX_DATA_REG] =		0x0010,
	[SPI_RX_DATA_REG] =		0x0014,
	[SPI_STATUS1_REG] =		0x0020,
	[SPI_PAD_SEL_REG] =		0x0024,
	[SPI_TX_SRC_REG_64] =		0x002c,
	[SPI_RX_DST_REG_64] =		0x0030,
	[VSPI_TRIG_REG] =		0x0018,
	[VSPI_IE_REG] =			0x0018,
	[VSPI_CFG_XFER_REG] =		0x0004,
	[VSPI_CTROL_REG] =		0x0018,
	[VSPI_INT_REG] =		0x001c,
	[VSPI_CFG_MSG_REG] =		0x0018,
	[VSPI_TIM0_REG] =		0x0028,
	[VSPI_TIM1_REG] =		0x0000,
};

static const u16 six_pins_spi_regs[] = {
	[SPI_CTROL0_REG] =		0x0000,
	[SPI_TX_DATA_REG] =		0x0018,
	[SPI_RX_DATA_REG] =		0x001c,
	[SPI_TX_SRC_REG] =		0x0020,
	[SPI_RX_DST_REG] =		0x0024,
	[SPI_CFG3_REG] =		0x0034,
	[VSPI_TRIG_REG] =		0x0008,
	[VSPI_IE_REG] =			0x000c,
	[VSPI_CFG_XFER_REG] =		0x0030,
	[VSPI_CTROL_REG] =		0x0004,
	[VSPI_INT_REG] =		0x0010,
	[VSPI_CFG_MSG_REG] =		0x0000,
	[VSPI_TIM0_REG] =		0x002c,
	[VSPI_TIM1_REG] =		0x0028,
};

enum spi_regs_offset {
	SPI_SCK_HIGH_CNT_OFFSET,
	SPI_SCK_LOW_CNT_OFFSET,
	SPI_ADJUST_SCK_LOW_CNT_OFFSET,
	SPI_CS_HOLD_CNT_OFFSET,
	SPI_ADJUST_CS_HOLD_CNT_OFFSET,
	SPI_CS_SETUP_CNT_OFFSET,
	SPI_ADJUST_CS_SETUP_CNT_OFFSET,
	SPI_CS_IDLE_CNT_OFFSET,
	SPI_PACKET_LOOP_OFFSET,
	SPI_PACKET_LENGTH_OFFSET,
	SPI_GET_TICK_DLY_OFFSET,
	SPI_CMD_ACT,
	SPI_CMD_RESUME,
	SPI_CMD_RST,
	SPI_CMD_PAUSE_EN,
	SPI_CMD_DEASSERT,
	SPI_CMD_SAMPLE_SEL,
	SPI_CMD_CS_POL,
	SPI_CMD_CPHA,
	SPI_CMD_CPOL,
	SPI_CMD_RX_DMA,
	SPI_CMD_TX_DMA,
	SPI_CMD_TXMSBF,
	SPI_CMD_RXMSBF,
	SPI_CMD_RX_ENDIAN,
	SPI_CMD_TX_ENDIAN,
	SPI_CMD_FINISH_IE,
	SPI_CMD_PAUSE_IE,

	/* only for multipin transfer IC*/
	SPI_TYPE_OFFSET,
	SPI_RW_MODE,
	SPI_COMMAND_CNT_OFFSET,
	SPI_DUMMY_CNT_OFFSET,
};

static const u16 normal_spi_offset[] = {
	[SPI_SCK_HIGH_CNT_OFFSET] =		0,
	[SPI_SCK_LOW_CNT_OFFSET] =		8,
	[SPI_ADJUST_SCK_LOW_CNT_OFFSET] =	16,
	[SPI_CS_HOLD_CNT_OFFSET] =		16,
	[SPI_ADJUST_CS_HOLD_CNT_OFFSET] =	0,
	[SPI_CS_SETUP_CNT_OFFSET] =		24,
	[SPI_ADJUST_CS_SETUP_CNT_OFFSET] =	16,
	[SPI_CS_IDLE_CNT_OFFSET] =		0,
	[SPI_PACKET_LOOP_OFFSET] =		8,
	[SPI_PACKET_LENGTH_OFFSET] =		16,
	[SPI_GET_TICK_DLY_OFFSET] =		29,
	[SPI_CMD_ACT] =				0,
	[SPI_CMD_RESUME] =			1,
	[SPI_CMD_RST] =				2,
	[SPI_CMD_PAUSE_EN] =			4,
	[SPI_CMD_DEASSERT] =			5,
	[SPI_CMD_SAMPLE_SEL] =			6,
	[SPI_CMD_CS_POL] =			7,
	[SPI_CMD_CPHA] =			8,
	[SPI_CMD_CPOL] =			9,
	[SPI_CMD_RX_DMA] =			10,
	[SPI_CMD_TX_DMA] =			11,
	[SPI_CMD_TXMSBF] =			12,
	[SPI_CMD_RXMSBF] =			13,
	[SPI_CMD_RX_ENDIAN] =			14,
	[SPI_CMD_TX_ENDIAN] =			15,
	[SPI_CMD_FINISH_IE] =			16,
	[SPI_CMD_PAUSE_IE] =			17,
};

static const u16 six_pins_spi_offset[] = {
	[SPI_CMD_CPHA] =			0,
	[SPI_CMD_CPOL] =			1,
	[SPI_CMD_TXMSBF] =			2,
	[SPI_CMD_RXMSBF] =			3,
	[SPI_TYPE_OFFSET] =			4,
	[SPI_RW_MODE] =				6,
	[SPI_CMD_DEASSERT] =			8,
	[SPI_CMD_PAUSE_EN] =			16,
	[SPI_CMD_SAMPLE_SEL] =			0,
	[SPI_CMD_CS_POL] =			1,
	[SPI_CMD_TX_ENDIAN] =			2,
	[SPI_CMD_RX_ENDIAN] =			3,
	[SPI_GET_TICK_DLY_OFFSET] =		8,
	[SPI_CMD_TX_DMA] =			16,
	[SPI_CMD_RX_DMA] =			24,
	[SPI_CMD_ACT] =				0,
	[SPI_CMD_RESUME] =			8,
	[SPI_CMD_RST] =				16,
	[SPI_CMD_FINISH_IE] =			0,
	[SPI_CMD_PAUSE_IE] =			1,
	[SPI_CS_HOLD_CNT_OFFSET] =		0,
	[SPI_CS_SETUP_CNT_OFFSET] =		16,
	[SPI_SCK_HIGH_CNT_OFFSET] =		0,
	[SPI_ADJUST_SCK_LOW_CNT_OFFSET] =	16,
	[SPI_ADJUST_CS_HOLD_CNT_OFFSET] =	0,
	[SPI_ADJUST_CS_SETUP_CNT_OFFSET] =	16,
	[SPI_CS_IDLE_CNT_OFFSET] =		0,
	[SPI_PACKET_LOOP_OFFSET] =		8,
	[SPI_PACKET_LENGTH_OFFSET] =		16,
	[SPI_COMMAND_CNT_OFFSET] =		0,
	[SPI_DUMMY_CNT_OFFSET] =		8,
};

enum spi_regs_mask {
	SPI_GET_TICK_DLY_CNT_MASK,
	SPI_CS_HOLD_CNT_MASK,
	SPI_ADJUST_CS_HOLD_CNT_MASK,
	SPI_CS_SETUP_CNT_MASK,
	SPI_ADJUST_CS_SETUP_CNT_MASK,
	SPI_SCK_HIGH_CNT_MASK,
	SPI_ADJUST_SCK_HIGH_CNT_MASK,
	SPI_SCK_LOW_CNT_MASK,
	SPI_ADJUST_SCK_LOW_CNT_MASK,
	SPI_CS_IDLE_CNT_MASK,
	SPI_PACKET_LOOP_CNT_MASK,
	SPI_PACKET_LENGTH_CNT_MASK,

	/* only for multipin transfer IC*/
	SPI_TYPE_MASK,
	SPI_COMMAND_CNT_MASK,
	SPI_DUMMY_CNT_MASK,
};

static const u32 normal_spi_mask[] = {
	[SPI_GET_TICK_DLY_CNT_MASK] =		0x0007,
	[SPI_CS_HOLD_CNT_MASK] =		0x00ff,
	[SPI_ADJUST_CS_HOLD_CNT_MASK] =		0xffff,
	[SPI_CS_SETUP_CNT_MASK] =		0x00ff,
	[SPI_ADJUST_CS_SETUP_CNT_MASK] =	0xffff,
	[SPI_SCK_HIGH_CNT_MASK] =		0x00ff,
	[SPI_ADJUST_SCK_HIGH_CNT_MASK] =	0xffff,
	[SPI_SCK_LOW_CNT_MASK] =		0x00ff,
	[SPI_ADJUST_SCK_LOW_CNT_MASK] =		0xffff,
	[SPI_CS_IDLE_CNT_MASK] =		0x00ff,
	[SPI_PACKET_LOOP_CNT_MASK] =		0x00ff,
	[SPI_PACKET_LENGTH_CNT_MASK] =		0x03ff,
};


static const u32 six_pins_spi_mask[] = {
	[SPI_GET_TICK_DLY_CNT_MASK] =		0x0007,
	[SPI_ADJUST_CS_HOLD_CNT_MASK] =		0xffff,
	[SPI_ADJUST_CS_SETUP_CNT_MASK] =	0xffff,
	[SPI_ADJUST_SCK_HIGH_CNT_MASK] =	0xffff,
	[SPI_ADJUST_SCK_LOW_CNT_MASK] =		0xffff,
	[SPI_CS_IDLE_CNT_MASK] =		0x00ff,
	[SPI_PACKET_LOOP_CNT_MASK] =		0x00ff,
	[SPI_PACKET_LENGTH_CNT_MASK] =		0xffff,
	[SPI_TYPE_MASK] =			0x0003,
	[SPI_COMMAND_CNT_MASK] =		0x000f,
	[SPI_DUMMY_CNT_MASK] =			0x000f,
};

#define MTK_SPI_PAUSE_INT_STATUS 0x2

#define MTK_SPI_IDLE		0
#define MTK_SPI_PAUSED		1

#define MTK_SPI_MAX_FIFO_SIZE 32U

#define ADDRSHIFT_R_OFFSET  (6)
#define ADDRSHIFT_R_MASK    (0xFFFFF03F)
#define ADDRSHIFT_W_MASK    (0xFFFFFFC0)
#define MTK_SPI_32BIS_MASK  (0xFFFFFFFF)
#define MTK_SPI_32BIS_SHIFT (32)

#define SPI_REG_OFFSET(x) (mdata->dev_comp->reg_offset[x])
#define SPI_REG_MASK(x) (mdata->dev_comp->reg_mask[x])

#define DMA_ADDR_BITS		(36)
#define MT8173_SPI_MAX_PAD_SEL	(3)

#define MTK_SPI_SINGLE_MODE		0
#define MTK_SPI_DUAL_MODE		1
#define MTK_SPI_QUAD_MODE		2

struct mtk_spi_compatible {
	bool need_pad_sel;
	/* Must explicitly send dummy Tx bytes to do Rx only transfer */
	bool must_tx;
	/* some IC design adjust register define */
	bool enhance_timing;
	/*some chip support 8GB DRAM access, there are two kinds solutions*/
	bool dma8g_peri_ext;
	bool dma8g_spi_ext;
	bool six_pins;
	const u16 *reg;
	const u16 *reg_offset;
	const u32 *reg_mask;
	const u32 max_packet_len;
};

struct mtk_spi {
	void __iomem *base;
	void __iomem *peri_regs;
	u32 state;
	int pad_num;
	u32 *pad_sel;
	struct clk *parent_clk, *sel_clk, *spi_clk;
	struct spi_transfer *cur_transfer;
	u32 xfer_len;
	u32 num_xfered;
	struct scatterlist *tx_sgl, *rx_sgl;
	u32 tx_sgl_len, rx_sgl_len;
	const struct mtk_spi_compatible *dev_comp;
	u32 dram_8gb_offset;
#ifdef CONFIG_SND_SOC_MT8516_abc123_MACH
	u32 spi_clk_hz;
#endif
};

static const struct mtk_spi_compatible mtk_common_compat;

static const struct mtk_spi_compatible mt2712_compat = {
	.must_tx = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

static const struct mtk_spi_compatible mt6758_compat = {
	.need_pad_sel = true,
	.enhance_timing = true,
	.dma8g_peri_ext = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

static const struct mtk_spi_compatible mt6765_compat = {
	.need_pad_sel = true,
	.enhance_timing = true,
	.dma8g_spi_ext = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

static const struct mtk_spi_compatible mt7622_compat = {
	.must_tx = true,
	.enhance_timing = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

static const struct mtk_spi_compatible mt8173_compat = {
	.need_pad_sel = true,
	.must_tx = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,

};

static const struct mtk_spi_compatible mt8183_compat = {
	.need_pad_sel = true,
	.must_tx = true,
	.enhance_timing = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

static const struct mtk_spi_compatible mt8518_compat = {
	.must_tx = true,
	.enhance_timing = true,
	.six_pins = true,
	.reg = six_pins_spi_regs,
	.reg_offset = six_pins_spi_offset,
	.reg_mask = six_pins_spi_mask,
	.max_packet_len = SZ_64K,
};

static const struct mtk_spi_compatible mt8695_compat = {
	.must_tx = true,
	.reg = normal_spi_regs,
	.reg_offset = normal_spi_offset,
	.reg_mask = normal_spi_mask,
	.max_packet_len = SZ_1K,
};

/*
 * A piece of default chip info unless the platform
 * supplies it.
 */
static const struct mtk_chip_config mtk_default_chip_info = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.cs_pol = 0,
	.sample_sel = 0,
	.command_cnt = 1,
	.dummy_cnt = 0,
};

static const struct of_device_id mtk_spi_of_match[] = {
	{ .compatible = "mediatek,mt2701-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt2712-spi",
		.data = (void *)&mt2712_compat,
	},
	{ .compatible = "mediatek,mt6589-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt6758-spi",
		.data = (void *)&mt6758_compat,
	},
	{ .compatible = "mediatek,mt6765-spi",
		.data = (void *)&mt6765_compat,
	},
	{ .compatible = "mediatek,mt7622-spi",
		.data = (void *)&mt7622_compat,
	},
	{ .compatible = "mediatek,mt8135-spi",
		.data = (void *)&mtk_common_compat,
	},
	{ .compatible = "mediatek,mt8173-spi",
		.data = (void *)&mt8173_compat,
	},
	{ .compatible = "mediatek,mt8183-spi",
		.data = (void *)&mt8183_compat,
	},
	{ .compatible = "mediatek,mt8518-spi",
		.data = (void *)&mt8518_compat,
	},
	{ .compatible = "mediatek,mt8695-spi",
		.data = (void *)&mt8695_compat,
	},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_spi_of_match);

#define LOG_CLOSE   0
#define LOG_OPEN    1
u8 spi_log_status = LOG_CLOSE;

#define spi_debug(fmt, args...) do { \
	if (spi_log_status == LOG_OPEN) {\
		pr_info("[spi]%s() " fmt, __func__, ##args); \
	} \
} while (0)

static ssize_t spi_log_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	char buf_temp[50] = { 0 };

	if (buf == NULL) {
		pr_notice("%s() *buf is NULL\n", __func__);
		return -EINVAL;
	}

	sprintf(buf_temp, "Now spi log %s.\n",
		(spi_log_status == LOG_CLOSE)?"disabled":"enabled");
		strncat(buf, buf_temp, strlen(buf_temp));

	return strlen(buf);
}

static ssize_t spi_log_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!\n", __func__);
		return -EINVAL;
	}

	pr_info("[spi]%s buflen:%d buf:%s\n", __func__, (u32)strlen(buf), buf);
	if (!strncmp(buf, "1", 1)) {
		pr_info("[spi]%s Now enable spi log\n", __func__);
		spi_log_status = LOG_OPEN;
	} else if (!strncmp(buf, "0", 1)) {
		pr_info("[spi]%s Now disable spi log\n", __func__);
		spi_log_status = LOG_CLOSE;
	} else
		pr_info("[spi]%s invalid parameter.Plz Input 1 or 0\n",
			__func__);

	return count;
}

static DEVICE_ATTR(spi_log, 0644, spi_log_show, spi_log_store);

static u32 spi_readl(struct mtk_spi *mdata, enum spi_regs reg)
{
	return readl(mdata->base + mdata->dev_comp->reg[reg]);
}

static void spi_writel(struct mtk_spi *mdata, u32 val, enum spi_regs reg)
{
	writel(val, mdata->base + mdata->dev_comp->reg[reg]);
}

static void spi_dump_reg(struct mtk_spi *ms)
{
	spi_debug("||* spi_dump_reg *******************||\n");
	spi_debug("cfg0:0x%.8x\n", spi_readl(ms, VSPI_TIM1_REG));
	spi_debug("cfg1:0x%.8x\n", spi_readl(ms, VSPI_CFG_XFER_REG));
	spi_debug("cfg2:0x%.8x\n", spi_readl(ms, VSPI_TIM0_REG));
	spi_debug("cmd :0x%.8x\n", spi_readl(ms, VSPI_TRIG_REG));
	spi_debug("tx_s:0x%.8x\n", spi_readl(ms, SPI_TX_SRC_REG));
	spi_debug("rx_d:0x%.8x\n", spi_readl(ms, SPI_RX_DST_REG));
	spi_debug("status1:0x%.8x\n", spi_readl(ms, SPI_STATUS1_REG));
	spi_debug("pad_sel:0x%.8x\n", spi_readl(ms, SPI_PAD_SEL_REG));
	spi_debug("||*****************************************||\n");
}

static void spi_dump_config(struct spi_master *master, struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;
	struct mtk_chip_config *chip_config = spi->controller_data;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	spi_debug("||* spi_dump_config *******************||\n");
	spi_debug("spi->mode:0x%.4x\n", spi->mode);
	spi_debug("chip_config->tx_mlsb:%d.\n", chip_config->tx_mlsb);
	spi_debug("chip_config->rx_mlsb:%d.\n", chip_config->rx_mlsb);
	spi_debug("chip_config->cs_pol:%d.\n", chip_config->cs_pol);
	spi_debug("chip_config->sample_sel:%d\n", chip_config->sample_sel);
	if (mdata->dev_comp->need_pad_sel)
		spi_debug("spi->chip_select:%d pad_sel[spi->chip_select]:%d\n",
			spi->chip_select, mdata->pad_sel[spi->chip_select]);
	spi_debug("||*****************************************||\n");
}

static void mtk_spi_reset(struct mtk_spi *mdata)
{
	u32 reg_val;

	/* set the software reset bit in SPI_CMD_REG. */
	reg_val = spi_readl(mdata, VSPI_TRIG_REG);
	reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_RST));
	spi_writel(mdata, reg_val, VSPI_TRIG_REG);

	reg_val = spi_readl(mdata, VSPI_TRIG_REG);
	reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_RST));
	spi_writel(mdata, reg_val, VSPI_TRIG_REG);
}

static int mtk_spi_prepare_message(struct spi_master *master,
				   struct spi_message *msg)
{
	u16 cpha, cpol;
	u32 reg_val;
	struct spi_device *spi = msg->spi;
	struct mtk_chip_config *chip_config = spi->controller_data;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	cpha = spi->mode & SPI_CPHA ? 1 : 0;
	cpol = spi->mode & SPI_CPOL ? 1 : 0;

	spi_debug("cpha:%d cpol:%d. chip_config as below\n", cpha, cpol);
	spi_dump_config(master, msg);

	reg_val = spi_readl(mdata, VSPI_CFG_XFER_REG);
	reg_val &= ~(SPI_REG_MASK(SPI_GET_TICK_DLY_CNT_MASK) <<
			SPI_REG_OFFSET(SPI_GET_TICK_DLY_OFFSET));
	reg_val |= (chip_config->get_tick_dly <<
			SPI_REG_OFFSET(SPI_GET_TICK_DLY_OFFSET));
	spi_writel(mdata, reg_val, VSPI_CFG_XFER_REG);

	reg_val = spi_readl(mdata, VSPI_CFG_MSG_REG);
	if (cpha)
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_CPHA));
	else
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_CPHA));
	if (cpol)
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_CPOL));
	else
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_CPOL));

	/* set the mlsbx and mlsbtx */
	if (chip_config->tx_mlsb)
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_TXMSBF));
	else
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_TXMSBF));
	if (chip_config->rx_mlsb)
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_RXMSBF));
	else
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_RXMSBF));
	reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_DEASSERT));
	spi_writel(mdata, reg_val, VSPI_CFG_MSG_REG);

	reg_val = spi_readl(mdata, VSPI_CTROL_REG);
	if (mdata->dev_comp->enhance_timing) {
		if (chip_config->cs_pol)
			reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_CS_POL));
		else
			reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_CS_POL));
		if (chip_config->sample_sel)
			reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_SAMPLE_SEL));
		else
			reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_SAMPLE_SEL));
	}
#ifdef __LITTLE_ENDIAN
	reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_RX_ENDIAN));
	reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_TX_ENDIAN));
#else
	reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_RX_ENDIAN));
	reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_TX_ENDIAN));
#endif
	reg_val &= ~((1 << SPI_REG_OFFSET(SPI_CMD_TX_DMA)) |
			(1 << SPI_REG_OFFSET(SPI_CMD_RX_DMA)));
	spi_writel(mdata, reg_val, VSPI_CTROL_REG);

	reg_val = spi_readl(mdata, VSPI_IE_REG);
	reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_FINISH_IE)) |
			(1 << SPI_REG_OFFSET(SPI_CMD_PAUSE_IE));
	spi_writel(mdata, reg_val, VSPI_IE_REG);

	if (mdata->dev_comp->six_pins) {
		reg_val = spi_readl(mdata, SPI_CFG3_REG);
		reg_val &= ~(SPI_REG_MASK(SPI_COMMAND_CNT_MASK) <<
			     SPI_REG_OFFSET(SPI_COMMAND_CNT_OFFSET));
		reg_val |= ((chip_config->command_cnt &
			    SPI_REG_MASK(SPI_COMMAND_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_COMMAND_CNT_OFFSET));
		reg_val &= ~(SPI_REG_MASK(SPI_DUMMY_CNT_MASK) <<
			     SPI_REG_OFFSET(SPI_DUMMY_CNT_OFFSET));
		reg_val |= ((chip_config->dummy_cnt &
			     SPI_REG_MASK(SPI_DUMMY_CNT_MASK)) <<
			     SPI_REG_OFFSET(SPI_DUMMY_CNT_OFFSET));
		spi_writel(mdata, reg_val, SPI_CFG3_REG);
	}
	if (mdata->dev_comp->need_pad_sel)
		spi_writel(mdata, mdata->pad_sel[spi->chip_select],
			   SPI_PAD_SEL_REG);
	return 0;
}

static void mtk_spi_set_cs(struct spi_device *spi, bool enable)
{
	u32 reg_val;
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);

	reg_val = spi_readl(mdata, VSPI_IE_REG);
	if (!enable) {
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_PAUSE_EN));
		reg_val |= (1 << SPI_REG_OFFSET(SPI_CMD_PAUSE_IE));
		spi_writel(mdata, reg_val, VSPI_IE_REG);
	} else {
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_PAUSE_EN));
		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_CMD_PAUSE_IE));
		spi_writel(mdata, reg_val, VSPI_IE_REG);
		mdata->state = MTK_SPI_IDLE;
		mtk_spi_reset(mdata);
	}
}

static void mtk_spi_prepare_transfer(struct spi_master *master,
				     struct spi_transfer *xfer)
{
	u32 spi_clk_hz, div, sck_time, cs_time, reg_val, type;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

#ifdef CONFIG_SND_SOC_MT8516_abc123_MACH
	/* clk_get_rate() waits for mutex can be 30ms long found in ftraces */
	spi_clk_hz = mdata->spi_clk_hz;
#else
	spi_clk_hz = clk_get_rate(mdata->spi_clk);
#endif
	if (xfer->speed_hz < spi_clk_hz / 2)
		div = DIV_ROUND_UP(spi_clk_hz, xfer->speed_hz);
	else
		div = 1;

	sck_time = (div + 1) / 2;
	cs_time = sck_time * 2;

	if (mdata->dev_comp->six_pins) {
		if (xfer->tx_nbits == SPI_NBITS_DUAL ||
		    xfer->rx_nbits == SPI_NBITS_DUAL)
			type = MTK_SPI_DUAL_MODE;
		else if (xfer->tx_nbits == SPI_NBITS_QUAD ||
			 xfer->rx_nbits == SPI_NBITS_QUAD)
			type = MTK_SPI_QUAD_MODE;
		else
			type = MTK_SPI_SINGLE_MODE;

		reg_val = spi_readl(mdata, SPI_CTROL0_REG);
		reg_val &= ~(SPI_REG_MASK(SPI_TYPE_MASK) <<
			     SPI_REG_OFFSET(SPI_TYPE_OFFSET));
		reg_val |= ((type &
			    SPI_REG_MASK(SPI_TYPE_MASK)) <<
			    SPI_REG_OFFSET(SPI_TYPE_OFFSET));

		reg_val &= ~(1 << SPI_REG_OFFSET(SPI_RW_MODE));
		if (type == MTK_SPI_DUAL_MODE || type == MTK_SPI_QUAD_MODE) {
			if (xfer->tx_buf && !xfer->rx_buf)
				reg_val |= (1 << SPI_REG_OFFSET(SPI_RW_MODE));
			else
				reg_val &= ~(1 << SPI_REG_OFFSET(SPI_RW_MODE));
		}
		spi_writel(mdata, reg_val, SPI_CTROL0_REG);
	}

	reg_val = spi_readl(mdata, VSPI_CFG_XFER_REG);
	reg_val &= ~(SPI_REG_MASK(SPI_CS_IDLE_CNT_MASK) <<
		     SPI_REG_OFFSET(SPI_CS_IDLE_CNT_OFFSET));
	reg_val |= (((cs_time - 1) &
		    SPI_REG_MASK(SPI_CS_IDLE_CNT_MASK)) <<
		    SPI_REG_OFFSET(SPI_CS_IDLE_CNT_OFFSET));
	spi_writel(mdata, reg_val, VSPI_CFG_XFER_REG);

	if (mdata->dev_comp->enhance_timing) {
		reg_val = 0;
		reg_val |= (((sck_time - 1) &
			    SPI_REG_MASK(SPI_ADJUST_SCK_HIGH_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_SCK_HIGH_CNT_OFFSET));
		reg_val |= (((sck_time - 1) &
			    SPI_REG_MASK(SPI_ADJUST_SCK_LOW_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_ADJUST_SCK_LOW_CNT_OFFSET));
		spi_writel(mdata, reg_val, VSPI_TIM0_REG);

		reg_val = spi_readl(mdata, VSPI_TIM1_REG);
		reg_val |= (((cs_time - 1) &
			    SPI_REG_MASK(SPI_ADJUST_CS_HOLD_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_ADJUST_CS_HOLD_CNT_OFFSET));
		reg_val |= (((cs_time - 1) &
			    SPI_REG_MASK(SPI_ADJUST_CS_SETUP_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_ADJUST_CS_SETUP_CNT_OFFSET));
		spi_writel(mdata, reg_val, VSPI_TIM1_REG);
	} else {
		reg_val = spi_readl(mdata, VSPI_TIM1_REG);
		reg_val |= (((sck_time - 1) &
			    SPI_REG_MASK(SPI_SCK_HIGH_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_SCK_HIGH_CNT_OFFSET));
		reg_val |= (((sck_time - 1) &
			    SPI_REG_MASK(SPI_SCK_LOW_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_SCK_LOW_CNT_OFFSET));
		reg_val |= (((cs_time - 1) &
			    SPI_REG_MASK(SPI_CS_HOLD_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_CS_HOLD_CNT_OFFSET));
		reg_val |= (((cs_time - 1) &
			    SPI_REG_MASK(SPI_CS_SETUP_CNT_MASK)) <<
			    SPI_REG_OFFSET(SPI_CS_SETUP_CNT_OFFSET));
		spi_writel(mdata, reg_val, VSPI_TIM1_REG);
	}
}

static void mtk_spi_setup_packet(struct spi_master *master)
{
	u32 packet_size, packet_loop, reg_val;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	packet_size = min_t(u32, mdata->xfer_len,
			mdata->dev_comp->max_packet_len);
	packet_loop = mdata->xfer_len / packet_size;

	reg_val = spi_readl(mdata, VSPI_CFG_XFER_REG);
	reg_val &= ~(SPI_REG_MASK(SPI_PACKET_LENGTH_CNT_MASK) <<
		     SPI_REG_OFFSET(SPI_PACKET_LENGTH_OFFSET) |
		     (SPI_REG_MASK(SPI_PACKET_LOOP_CNT_MASK) <<
		     SPI_REG_OFFSET(SPI_PACKET_LOOP_OFFSET)));
	reg_val |= (((packet_loop - 1) &
		    SPI_REG_MASK(SPI_PACKET_LOOP_CNT_MASK)) <<
		    SPI_REG_OFFSET(SPI_PACKET_LOOP_OFFSET));
	reg_val |= (((packet_size - 1) &
		    SPI_REG_MASK(SPI_PACKET_LENGTH_CNT_MASK)) <<
		    SPI_REG_OFFSET(SPI_PACKET_LENGTH_OFFSET));
	spi_writel(mdata, reg_val, VSPI_CFG_XFER_REG);
}

static void mtk_spi_enable_transfer(struct spi_master *master)
{
	u32 cmd;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	cmd = spi_readl(mdata, VSPI_TRIG_REG);
	if (mdata->state == MTK_SPI_IDLE)
		cmd |= (1 << SPI_REG_OFFSET(SPI_CMD_ACT));
	else
		cmd |= (1 << SPI_REG_OFFSET(SPI_CMD_RESUME));
	spi_writel(mdata, cmd, VSPI_TRIG_REG);
}

static int mtk_spi_get_mult_delta(struct spi_master *master, u32 xfer_len)
{
	u32 mult_delta;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (xfer_len > mdata->dev_comp->max_packet_len)
		mult_delta = xfer_len % (mdata->dev_comp->max_packet_len);
	else
		mult_delta = 0;

	return mult_delta;
}

static void mtk_spi_update_mdata_len(struct spi_master *master)
{
	int mult_delta;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (mdata->tx_sgl_len && mdata->rx_sgl_len) {
		if (mdata->tx_sgl_len > mdata->rx_sgl_len) {
			mult_delta = mtk_spi_get_mult_delta(master,
						mdata->rx_sgl_len);
			mdata->xfer_len = mdata->rx_sgl_len - mult_delta;
			mdata->rx_sgl_len = mult_delta;
			mdata->tx_sgl_len -= mdata->xfer_len;
		} else {
			mult_delta = mtk_spi_get_mult_delta(master,
						mdata->tx_sgl_len);
			mdata->xfer_len = mdata->tx_sgl_len - mult_delta;
			mdata->tx_sgl_len = mult_delta;
			mdata->rx_sgl_len -= mdata->xfer_len;
		}
	} else if (mdata->tx_sgl_len) {
		mult_delta = mtk_spi_get_mult_delta(master, mdata->tx_sgl_len);
		mdata->xfer_len = mdata->tx_sgl_len - mult_delta;
		mdata->tx_sgl_len = mult_delta;
	} else if (mdata->rx_sgl_len) {
		mult_delta = mtk_spi_get_mult_delta(master, mdata->rx_sgl_len);
		mdata->xfer_len = mdata->rx_sgl_len - mult_delta;
		mdata->rx_sgl_len = mult_delta;
	}
}

static void mtk_spi_setup_dma_addr(struct spi_master *master,
				   struct spi_transfer *xfer)
{
	u32 addr_ext;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	spi_debug("xfer->tx_dma:0x%llx,xfer->rx_dma:0x%llx\n",
		(uint64_t)xfer->tx_dma, (uint64_t)xfer->rx_dma);
	if (mdata->dev_comp->dma8g_peri_ext) {
		if (mdata->tx_sgl) {
			addr_ext = readl(mdata->peri_regs +
					 mdata->dram_8gb_offset);
			addr_ext = ((addr_ext & (ADDRSHIFT_W_MASK)) |
				    (u32)(cpu_to_le64(xfer->tx_dma) /
				    SZ_1G));
			writel(addr_ext,
			       mdata->peri_regs + mdata->dram_8gb_offset);
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->tx_dma) %
				   SZ_1G), SPI_TX_SRC_REG);
		}
		if (mdata->rx_sgl) {
			addr_ext = readl(mdata->peri_regs +
					 mdata->dram_8gb_offset);
			addr_ext = ((addr_ext & (ADDRSHIFT_R_MASK)) |
				    (u32)((cpu_to_le64(xfer->rx_dma) /
				    SZ_1G) << ADDRSHIFT_R_OFFSET));
			writel(addr_ext,
			       mdata->dram_8gb_offset + mdata->peri_regs);
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->rx_dma) %
				   SZ_1G), SPI_RX_DST_REG);
		}
	} else if (mdata->dev_comp->dma8g_spi_ext) {
		if (mdata->tx_sgl) {
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->tx_dma) &
				   MTK_SPI_32BIS_MASK), SPI_TX_SRC_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->tx_dma) >>
				   MTK_SPI_32BIS_SHIFT), SPI_TX_SRC_REG_64);
#endif
		}
		if (mdata->rx_sgl) {
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->rx_dma) &
				   MTK_SPI_32BIS_MASK), SPI_RX_DST_REG);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			spi_writel(mdata, (u32)(cpu_to_le64(xfer->rx_dma) >>
				   MTK_SPI_32BIS_SHIFT), SPI_RX_DST_REG_64);
#endif
		}
	} else {
		if (mdata->tx_sgl)
			spi_writel(mdata, xfer->tx_dma, SPI_TX_SRC_REG);
		if (mdata->rx_sgl)
			spi_writel(mdata, xfer->rx_dma, SPI_RX_DST_REG);
	}
}

static int mtk_spi_fifo_transfer(struct spi_master *master,
				 struct spi_device *spi,
				 struct spi_transfer *xfer)
{
	int cnt, remainder;
	u32 reg_val;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	mdata->cur_transfer = xfer;
	mdata->xfer_len = min(MTK_SPI_MAX_FIFO_SIZE, xfer->len);
	mdata->num_xfered = 0;

	mtk_spi_prepare_transfer(master, xfer);
	mtk_spi_setup_packet(master);

	cnt = xfer->len / 4;
	if (xfer->tx_buf)
		iowrite32_rep(mdata->base +
			      mdata->dev_comp->reg[SPI_TX_DATA_REG],
			      xfer->tx_buf, cnt);

	remainder = xfer->len % 4;
	if (xfer->tx_buf && remainder > 0) {
		reg_val = 0;
		memcpy(&reg_val, xfer->tx_buf + (cnt * 4), remainder);
		spi_writel(mdata, reg_val, SPI_TX_DATA_REG);
	}

	spi_debug("spi setting Done.Dump reg before Transfer start:\n");
	spi_dump_reg(mdata);

	mtk_spi_enable_transfer(master);

	return 1;
}

static int mtk_spi_dma_transfer(struct spi_master *master,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	int cmd;
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	mdata->tx_sgl = NULL;
	mdata->rx_sgl = NULL;
	mdata->tx_sgl_len = 0;
	mdata->rx_sgl_len = 0;
	mdata->cur_transfer = xfer;
	mdata->num_xfered = 0;

	mtk_spi_prepare_transfer(master, xfer);

	cmd = spi_readl(mdata, VSPI_CTROL_REG);
	if (xfer->tx_buf)
		cmd |= (1 << SPI_REG_OFFSET(SPI_CMD_TX_DMA));
	if (xfer->rx_buf)
		cmd |= (1 << SPI_REG_OFFSET(SPI_CMD_RX_DMA));
	spi_writel(mdata, cmd, VSPI_CTROL_REG);

	if (xfer->tx_buf)
		mdata->tx_sgl = xfer->tx_sg.sgl;
	if (xfer->rx_buf)
		mdata->rx_sgl = xfer->rx_sg.sgl;

	if (mdata->tx_sgl) {
		xfer->tx_dma = sg_dma_address(mdata->tx_sgl);
		mdata->tx_sgl_len = sg_dma_len(mdata->tx_sgl);
	}
	if (mdata->rx_sgl) {
		xfer->rx_dma = sg_dma_address(mdata->rx_sgl);
		mdata->rx_sgl_len = sg_dma_len(mdata->rx_sgl);
	}

	mtk_spi_update_mdata_len(master);
	mtk_spi_setup_packet(master);
	mtk_spi_setup_dma_addr(master, xfer);

	spi_debug("spi setting Done.Dump reg before Transfer start:\n");
	spi_dump_reg(mdata);

	mtk_spi_enable_transfer(master);

	return 1;
}

static int mtk_spi_transfer_one(struct spi_master *master,
				struct spi_device *spi,
				struct spi_transfer *xfer)
{
	spi_debug("xfer->len:%d\n", xfer->len);
	if (master->can_dma(master, spi, xfer))
		return mtk_spi_dma_transfer(master, spi, xfer);
	else
		return mtk_spi_fifo_transfer(master, spi, xfer);
}

static bool mtk_spi_can_dma(struct spi_master *master,
			    struct spi_device *spi,
			    struct spi_transfer *xfer)
{
	return xfer->len > MTK_SPI_MAX_FIFO_SIZE;
}

static int mtk_spi_setup(struct spi_device *spi)
{
	struct mtk_spi *mdata = spi_master_get_devdata(spi->master);

	if (!spi->controller_data)
		spi->controller_data = (void *)&mtk_default_chip_info;

	if (mdata->dev_comp->need_pad_sel && gpio_is_valid(spi->cs_gpio))
		gpio_direction_output(spi->cs_gpio, !(spi->mode & SPI_CS_HIGH));

	return 0;
}

static irqreturn_t mtk_spi_interrupt(int irq, void *dev_id)
{
	u32 cmd, reg_val, cnt, remainder, len;
	struct spi_master *master = dev_id;
	struct mtk_spi *mdata = spi_master_get_devdata(master);
	struct spi_transfer *trans = mdata->cur_transfer;

	reg_val = spi_readl(mdata, VSPI_INT_REG);

	if (reg_val & MTK_SPI_PAUSE_INT_STATUS)
		mdata->state = MTK_SPI_PAUSED;
	else
		mdata->state = MTK_SPI_IDLE;

	if (!master->can_dma(master, master->cur_msg->spi, trans)) {
		if (trans->rx_buf) {
			cnt = mdata->xfer_len / 4;
			ioread32_rep(mdata->base +
				     mdata->dev_comp->reg[SPI_RX_DATA_REG],
				     trans->rx_buf + mdata->num_xfered, cnt);
			remainder = mdata->xfer_len % 4;
			if (remainder > 0) {
				reg_val = spi_readl(mdata, SPI_RX_DATA_REG);
				memcpy(trans->rx_buf +
						mdata->num_xfered +
						(cnt * 4),
				       &reg_val, remainder);
			}
		}

		mdata->num_xfered += mdata->xfer_len;
		if (mdata->num_xfered == trans->len) {
			spi_finalize_current_transfer(master);
			spi_debug("The last fifo transfer Done.\n");
			return IRQ_HANDLED;
		}

		len = trans->len - mdata->num_xfered;
		mdata->xfer_len = min(MTK_SPI_MAX_FIFO_SIZE, len);
		mtk_spi_setup_packet(master);
		cnt = (mdata->xfer_len) / 4;
		iowrite32_rep(mdata->base +
				 mdata->dev_comp->reg[SPI_TX_DATA_REG],
				 trans->tx_buf + mdata->num_xfered, cnt);

		remainder = (mdata->xfer_len) % 4;
		if (remainder > 0) {
			reg_val = 0;
			memcpy(&reg_val,
				trans->tx_buf + (cnt * 4) + mdata->num_xfered,
				remainder);
			spi_writel(mdata, reg_val, SPI_TX_DATA_REG);
		}

		mtk_spi_enable_transfer(master);
		return IRQ_HANDLED;
	}

	if (mdata->tx_sgl)
		trans->tx_dma += mdata->xfer_len;
	if (mdata->rx_sgl)
		trans->rx_dma += mdata->xfer_len;

	if (mdata->tx_sgl && (mdata->tx_sgl_len == 0)) {
		mdata->tx_sgl = sg_next(mdata->tx_sgl);
		if (mdata->tx_sgl) {
			trans->tx_dma = sg_dma_address(mdata->tx_sgl);
			mdata->tx_sgl_len = sg_dma_len(mdata->tx_sgl);
		}
	}
	if (mdata->rx_sgl && (mdata->rx_sgl_len == 0)) {
		mdata->rx_sgl = sg_next(mdata->rx_sgl);
		if (mdata->rx_sgl) {
			trans->rx_dma = sg_dma_address(mdata->rx_sgl);
			mdata->rx_sgl_len = sg_dma_len(mdata->rx_sgl);
		}
	}

	if (!mdata->tx_sgl && !mdata->rx_sgl) {
		/* spi disable dma */
		cmd = spi_readl(mdata, VSPI_CTROL_REG);
		cmd &= ~(1 << SPI_REG_OFFSET(SPI_CMD_TX_DMA));
		cmd &= ~(1 << SPI_REG_OFFSET(SPI_CMD_RX_DMA));
		spi_writel(mdata, cmd, VSPI_CTROL_REG);

		spi_finalize_current_transfer(master);
		spi_debug("The last DMA transfer Done.\n");
		return IRQ_HANDLED;
	}

	spi_debug("One DMA transfer Done.Start Next\n");

	mtk_spi_update_mdata_len(master);
	mtk_spi_setup_packet(master);
	mtk_spi_setup_dma_addr(master, trans);
	mtk_spi_enable_transfer(master);

	return IRQ_HANDLED;
}

static int mtk_spi_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct mtk_spi *mdata;
	const struct of_device_id *of_id;
	struct resource *res;
	int i, irq, ret;
	struct device_node *node_pericfg;

	master = spi_alloc_master(&pdev->dev, sizeof(*mdata));
	if (!master) {
		dev_err(&pdev->dev, "failed to alloc spi master\n");
		return -ENOMEM;
	}

	master->max_dma_len = SZ_256K;
	master->auto_runtime_pm = true;
	master->dev.of_node = pdev->dev.of_node;
	master->mode_bits = SPI_CPOL | SPI_CPHA;

	master->set_cs = mtk_spi_set_cs;
	master->prepare_message = mtk_spi_prepare_message;
	master->transfer_one = mtk_spi_transfer_one;
	master->can_dma = mtk_spi_can_dma;
	master->setup = mtk_spi_setup;

	of_id = of_match_node(mtk_spi_of_match, pdev->dev.of_node);
	if (!of_id) {
		dev_err(&pdev->dev, "failed to probe of_node\n");
		ret = -EINVAL;
		goto err_put_master;
	}

	mdata = spi_master_get_devdata(master);
	mdata->dev_comp = of_id->data;

	if (mdata->dev_comp->six_pins)
		master->mode_bits = SPI_TX_DUAL | SPI_RX_DUAL |
					SPI_TX_QUAD | SPI_RX_QUAD;

	if (mdata->dev_comp->must_tx)
		master->flags = SPI_MASTER_MUST_TX;

	if (mdata->dev_comp->need_pad_sel) {
		mdata->pad_num = of_property_count_u32_elems(
			pdev->dev.of_node,
			"mediatek,pad-select");
		if (mdata->pad_num < 0) {
			dev_err(&pdev->dev,
				"No 'mediatek,pad-select' property\n");
			ret = -EINVAL;
			goto err_put_master;
		}

		mdata->pad_sel = devm_kmalloc_array(&pdev->dev, mdata->pad_num,
						    sizeof(u32), GFP_KERNEL);
		if (!mdata->pad_sel) {
			ret = -ENOMEM;
			goto err_put_master;
		}

		for (i = 0; i < mdata->pad_num; i++) {
			of_property_read_u32_index(pdev->dev.of_node,
						   "mediatek,pad-select",
						   i, &mdata->pad_sel[i]);
			if (mdata->pad_sel[i] > MT8173_SPI_MAX_PAD_SEL) {
				dev_err(&pdev->dev, "wrong pad-sel[%d]: %u\n",
					i, mdata->pad_sel[i]);
				ret = -EINVAL;
				goto err_put_master;
			}
		}
	}

	platform_set_drvdata(pdev, master);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "failed to determine base address\n");
		goto err_put_master;
	}

	mdata->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mdata->base)) {
		ret = PTR_ERR(mdata->base);
		goto err_put_master;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq (%d)\n", irq);
		ret = irq;
		goto err_put_master;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	ret = devm_request_irq(&pdev->dev, irq, mtk_spi_interrupt,
			       IRQF_TRIGGER_NONE, dev_name(&pdev->dev), master);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq (%d)\n", ret);
		goto err_put_master;
	}

	mdata->parent_clk = devm_clk_get(&pdev->dev, "parent-clk");
	if (IS_ERR(mdata->parent_clk)) {
		ret = PTR_ERR(mdata->parent_clk);
		dev_err(&pdev->dev, "failed to get parent-clk: %d\n", ret);
		goto err_put_master;
	}

	mdata->sel_clk = devm_clk_get(&pdev->dev, "sel-clk");
	if (IS_ERR(mdata->sel_clk)) {
		ret = PTR_ERR(mdata->sel_clk);
		dev_err(&pdev->dev, "failed to get sel-clk: %d\n", ret);
		goto err_put_master;
	}

	mdata->spi_clk = devm_clk_get(&pdev->dev, "spi-clk");
	if (IS_ERR(mdata->spi_clk)) {
		ret = PTR_ERR(mdata->spi_clk);
		dev_err(&pdev->dev, "failed to get spi-clk: %d\n", ret);
		goto err_put_master;
	}

	ret = clk_prepare_enable(mdata->spi_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable spi_clk (%d)\n", ret);
		goto err_put_master;
	}

	ret = clk_set_parent(mdata->sel_clk, mdata->parent_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to clk_set_parent (%d)\n", ret);
		clk_disable_unprepare(mdata->spi_clk);
		goto err_put_master;
	}

#ifdef CONFIG_SND_SOC_MT8516_abc123_MACH
	/* PM is put off. Clock rate is obtained once since this function can
	 * wait for mutex for as long as 10-30ms proven from ftraces */
	mdata->spi_clk_hz = clk_get_rate(mdata->spi_clk);
#endif
	pm_runtime_enable(&pdev->dev);

	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret) {
		dev_err(&pdev->dev, "failed to register master (%d)\n", ret);
		clk_disable_unprepare(mdata->spi_clk);
		goto err_disable_runtime_pm;
	}

	clk_disable_unprepare(mdata->spi_clk);

	if (mdata->dev_comp->need_pad_sel) {
		if (mdata->pad_num != master->num_chipselect) {
			dev_err(&pdev->dev,
				"pad_num does not match num_chipselect(%d != %d)\n",
				mdata->pad_num, master->num_chipselect);
			ret = -EINVAL;
			goto err_disable_runtime_pm;
		}

		if (!master->cs_gpios && master->num_chipselect > 1) {
			dev_err(&pdev->dev,
				"cs_gpios not specified and num_chipselect > 1\n");
			ret = -EINVAL;
			goto err_disable_runtime_pm;
		}

		if (master->cs_gpios) {
			for (i = 0; i < master->num_chipselect; i++) {
				ret = devm_gpio_request(&pdev->dev,
							master->cs_gpios[i],
							dev_name(&pdev->dev));
				if (ret) {
					dev_err(&pdev->dev,
						"can't get CS GPIO %i\n", i);
					goto err_disable_runtime_pm;
				}
			}
		}
	}

	if (mdata->dev_comp->dma8g_peri_ext) {
		node_pericfg = of_find_compatible_node(NULL, NULL,
				"mediatek,pericfg");
		if (!node_pericfg) {
			dev_notice(&pdev->dev, "error: node_pericfg init fail\n");
			goto err_disable_runtime_pm;
		}
		mdata->peri_regs = of_iomap(node_pericfg, 0);
		if (IS_ERR(*(void **)&(mdata->peri_regs))) {
			ret = PTR_ERR(*(void **)&mdata->peri_regs);
			dev_notice(&pdev->dev, "error: ms->peri_regs init fail\n");
			mdata->peri_regs = NULL;
			goto err_disable_runtime_pm;
		}
		if (of_property_read_u32(pdev->dev.of_node,
			"mediatek,dram-8gb-offset", &mdata->dram_8gb_offset)) {
			dev_notice(&pdev->dev, "SPI get dram-8gb-offset failed\n");
			goto err_disable_runtime_pm;
		}
	}

	ret = device_create_file(&pdev->dev, &dev_attr_spi_log);
	if (ret)
		dev_notice(&pdev->dev, "SPI sysfs_create_file fail, ret:%d\n",
			ret);

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(DMA_ADDR_BITS));
	if (ret)
		dev_notice(&pdev->dev, "SPI dma_set_mask(%d) failed, ret:%d\n",
			DMA_ADDR_BITS, ret);

	return 0;

err_disable_runtime_pm:
	pm_runtime_disable(&pdev->dev);
err_put_master:
	spi_master_put(master);

	return ret;
}

static int mtk_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	pm_runtime_disable(&pdev->dev);

	mtk_spi_reset(mdata);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_spi_suspend(struct device *dev)
{
	int ret;
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	ret = spi_master_suspend(master);
	if (ret)
		return ret;

	if (!pm_runtime_suspended(dev))
		clk_disable_unprepare(mdata->spi_clk);

	return ret;
}

static int mtk_spi_resume(struct device *dev)
{
	int ret;
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	if (!pm_runtime_suspended(dev)) {
		ret = clk_prepare_enable(mdata->spi_clk);
		if (ret < 0) {
			dev_err(dev, "failed to enable spi_clk (%d)\n", ret);
			return ret;
		}
	}

	ret = spi_master_resume(master);
	if (ret < 0)
		clk_disable_unprepare(mdata->spi_clk);

#ifdef CONFIG_SND_SOC_MT8516_abc123_MACH
	mdata->spi_clk_hz = clk_get_rate(mdata->spi_clk);
#endif

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int mtk_spi_runtime_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);

	clk_disable_unprepare(mdata->spi_clk);

	return 0;
}

static int mtk_spi_runtime_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct mtk_spi *mdata = spi_master_get_devdata(master);
	int ret;

	ret = clk_prepare_enable(mdata->spi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to enable spi_clk (%d)\n", ret);
		return ret;
	}
#ifdef CONFIG_SND_SOC_MT8516_abc123_MACH
	mdata->spi_clk_hz = clk_get_rate(mdata->spi_clk);
#endif

	return 0;
}
#endif /* CONFIG_PM */

static const struct dev_pm_ops mtk_spi_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_spi_suspend, mtk_spi_resume)
	SET_RUNTIME_PM_OPS(mtk_spi_runtime_suspend,
			   mtk_spi_runtime_resume, NULL)
};

static struct platform_driver mtk_spi_driver = {
	.driver = {
		.name = "mtk-spi",
		.pm	= &mtk_spi_pm,
		.of_match_table = mtk_spi_of_match,
	},
	.probe = mtk_spi_probe,
	.remove = mtk_spi_remove,
};

module_platform_driver(mtk_spi_driver);

MODULE_DESCRIPTION("MTK SPI Controller driver");
MODULE_AUTHOR("Leilk Liu <leilk.liu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mtk-spi");
