/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Chen Zhong <chen.zhong@mediatek.com>
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

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"

#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infrasys,
	scpsys,
	apmixedsys,
	mcucfg,
	ipsys,
	mmsys,
	imgsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen),
	[infrasys] = REGBASE_V(0x10001000, infrasys),
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixedsys]  = REGBASE_V(0x1000c000, apmixedsys),
	[mcucfg]  = REGBASE_V(0x10200000, mcucfg),
	[ipsys]  = REGBASE_V(0x13002000, ipsys),
	[mmsys]    = REGBASE_V(0x14000000, mmsys),
	[imgsys]   = REGBASE_V(0x15000000, imgsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen, 0x000, CLK_MODE),
	REGNAME(topckgen, 0x040, CLK_CFG_0),
	REGNAME(topckgen, 0x050, CLK_CFG_1),
	REGNAME(topckgen, 0x060, CLK_CFG_2),
	REGNAME(topckgen, 0x070, CLK_CFG_3),
	REGNAME(topckgen, 0x080, CLK_CFG_4),
	REGNAME(topckgen, 0x090, CLK_CFG_5),
	REGNAME(topckgen, 0x0A0, CLK_CFG_6),
	REGNAME(topckgen, 0x0B0, CLK_CFG_7),
	REGNAME(topckgen, 0x0C0, CLK_CFG_8),
	REGNAME(topckgen, 0x0D0, CLK_CFG_9),
	REGNAME(topckgen, 0x0E0, CLK_CFG_10),
	REGNAME(topckgen, 0x0EC, CLK_CFG_11),
	REGNAME(topckgen, 0x104, CLK_MISC_CFG_0),
	REGNAME(topckgen, 0x108, CLK_MISC_CFG_1),
	REGNAME(topckgen, 0x10C, CLK_DBG_CFG),
	REGNAME(topckgen, 0x200, CLK_SCP_CFG_0),
	REGNAME(topckgen, 0x204, CLK_SCP_CFG_1),
	REGNAME(topckgen, 0x220, CLK26CALI_0),
	REGNAME(topckgen, 0x224, CLK26CALI_1),
	REGNAME(topckgen, 0x228, CLK26CALI_2),
	REGNAME(topckgen, 0x320, CLK_AUDDIV_0),
	REGNAME(topckgen, 0x324, CLK_AUDDIV_1),
	REGNAME(topckgen, 0x328, CLK_AUDDIV_2),
	REGNAME(topckgen, 0x32C, CLK_AUDDIV_3),
	REGNAME(topckgen, 0x340, CLK_AUDDIV_4),
	REGNAME(topckgen, 0x344, CLK_AUDDIV_5),
	REGNAME(topckgen, 0x348, CLK_AUDDIV_6),
	REGNAME(topckgen, 0x34C, CLK_AUDDIV_7),
	REGNAME(infrasys, 0x090, MODULE_SW_CG_0_STA),
	REGNAME(infrasys, 0x094, MODULE_SW_CG_1_STA),
	REGNAME(infrasys, 0x0AC, MODULE_SW_CG_2_STA),
	REGNAME(infrasys, 0x0C8, MODULE_SW_CG_3_STA),
	REGNAME(infrasys, 0x0D8, MODULE_SW_CG_4_STA),
	REGNAME(infrasys, 0x294, INFRA_MFG_MASTER_M0_GALS_CTRL),
	REGNAME(scpsys, 0x32C, CONN_PWR_CON),
	REGNAME(scpsys, 0x374, MM_PWR_CON),
	REGNAME(scpsys, 0x38C, IMG_PWR_CON),
	REGNAME(scpsys, 0x39C, IP0_PWR_CON),
	REGNAME(scpsys, 0x384, IP1_PWR_CON),
	REGNAME(scpsys, 0x388, IP2_PWR_CON),
	REGNAME(scpsys, 0x3A4, USB_MAC_P1_PWR_CON),
	REGNAME(scpsys, 0x314, AUDIO_PWR_CON),
	REGNAME(scpsys, 0x328, ASRC_PWR_CON),
	REGNAME(scpsys, 0x37C, DSP_PWR_CON),
	REGNAME(scpsys, 0x180, PWR_STATUS),
	REGNAME(scpsys, 0x184, PWR_STATUS_2ND),
	REGNAME(apmixedsys, 0x0208, UNIVPLL_CON0),
	REGNAME(apmixedsys, 0x020C, UNIVPLL_CON1),
	REGNAME(apmixedsys, 0x0210, UNIVPLL_CON2),
	REGNAME(apmixedsys, 0x0214, UNIVPLL_CON3),
	REGNAME(apmixedsys, 0x0228, MAINPLL_CON0),
	REGNAME(apmixedsys, 0x022C, MAINPLL_CON1),
	REGNAME(apmixedsys, 0x0230, MAINPLL_CON2),
	REGNAME(apmixedsys, 0x0234, MAINPLL_CON3),
	REGNAME(apmixedsys, 0x030C, ARMPLL_CON0),
	REGNAME(apmixedsys, 0x0310, ARMPLL_CON1),
	REGNAME(apmixedsys, 0x0314, ARMPLL_CON2),
	REGNAME(apmixedsys, 0x0318, ARMPLL_CON3),
	REGNAME(apmixedsys, 0x031C, APLL1_CON0),
	REGNAME(apmixedsys, 0x0320, APLL1_CON1),
	REGNAME(apmixedsys, 0x0324, APLL1_CON2),
	REGNAME(apmixedsys, 0x0328, APLL1_CON3),
	REGNAME(apmixedsys, 0x032C, APLL1_CON4),
	REGNAME(apmixedsys, 0x0350, MSDCPLL_CON0),
	REGNAME(apmixedsys, 0x0354, MSDCPLL_CON1),
	REGNAME(apmixedsys, 0x0358, MSDCPLL_CON2),
	REGNAME(apmixedsys, 0x035C, MSDCPLL_CON3),
	REGNAME(apmixedsys, 0x0360, APLL2_CON0),
	REGNAME(apmixedsys, 0x0364, APLL2_CON1),
	REGNAME(apmixedsys, 0x0368, APLL2_CON2),
	REGNAME(apmixedsys, 0x036C, APLL2_CON3),
	REGNAME(apmixedsys, 0x0370, APLL2_CON4),
	REGNAME(apmixedsys, 0x0374, IPPLL_CON0),
	REGNAME(apmixedsys, 0x0378, IPPLL_CON1),
	REGNAME(apmixedsys, 0x037C, IPPLL_CON2),
	REGNAME(apmixedsys, 0x0380, IPPLL_CON3),
	REGNAME(apmixedsys, 0x0390, DSPPLL_CON0),
	REGNAME(apmixedsys, 0x0394, DSPPLL_CON1),
	REGNAME(apmixedsys, 0x0398, DSPPLL_CON2),
	REGNAME(apmixedsys, 0x039C, DSPPLL_CON3),
	REGNAME(apmixedsys, 0x03A0, TCONPLL_CON0),
	REGNAME(apmixedsys, 0x03A4, TCONPLL_CON1),
	REGNAME(apmixedsys, 0x03A8, TCONPLL_CON2),
	REGNAME(apmixedsys, 0x03AC, TCONPLL_CON3),
	REGNAME(mcucfg, 0x640, ACLKEN_DIV),
	REGNAME(mcucfg, 0x644, PCLKEN_DIV),
	REGNAME(mcucfg, 0x7C0, MCU_BUS_MUX),
	REGNAME(ipsys, 0x110, IPSYS_NNA0_PWR_ON),
	REGNAME(ipsys, 0x114, IPSYS_NNA1_PWR_ON),
	REGNAME(ipsys, 0x118, IPSYS_WFST_PWR_ON),
	REGNAME(ipsys, 0x108, IPSYS_AXI_CK_CG),
	REGNAME(ipsys, 0xfc, IPSYS_26M_CK_CG),
	REGNAME(ipsys, 0x100, IPSYS_EMI_CK_CG),
	REGNAME(ipsys, 0x104, IPSYS_SRAM_CK_CG),
	REGNAME(ipsys, 0x98, IPSYS_NNAO_CK_CG),
	REGNAME(ipsys, 0x9c, IPSYS_NNA1_CK_CG),
	REGNAME(ipsys, 0xa0, IPSYS_WFST_CK_CG),
	REGNAME(mmsys, 0x100, MMSYS_CG_CON0),
	REGNAME(imgsys, 0x100, IMGSYS_CG_CON0),
	REGNAME(imgsys, 0x110, IMGSYS_CG_CON1),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, mask, val)	\
	clk_writel(addr, (clk_readl(addr) & ~(mask)) | (val))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,  1, "axi_ck"),
	FMCLK(CKGEN,  2, "mem_ck"),
	FMCLK(CKGEN,  3, "uart_ck"),
	FMCLK(CKGEN,  4, "spi_ck"),
	FMCLK(CKGEN,  5, "spis_ck"),
	FMCLK(CKGEN,  6, "msdc50_0_hclk_ck"),
	FMCLK(CKGEN,  7, "msdc2_2_hclk_ck"),
	FMCLK(CKGEN,  8, "msdc50_0_ck"),
	FMCLK(CKGEN,  9, "msdc50_2_ck"),
	FMCLK(CKGEN, 10, "msdc30_1_ck"),
	FMCLK(CKGEN, 11, "audio_ck"),
	FMCLK(CKGEN, 12, "aud_intbus_ck"),
	FMCLK(CKGEN, 13, "hapll1_ck"),
	FMCLK(CKGEN, 14, "hapll2_ck"),
	FMCLK(CKGEN, 15, "a2sys_ck"),
	FMCLK(CKGEN, 16, "a1sys_ck"),
	FMCLK(CKGEN, 17, "asm_l_ck"),
	FMCLK(CKGEN, 18, "asm_m_ck"),
	FMCLK(CKGEN, 19, "asm_h_ck"),
	FMCLK(CKGEN, 20, "aud_spdif_in_ck"),
	FMCLK(CKGEN, 21, "aud_1_ck"),
	FMCLK(CKGEN, 22, "aud_2_ck"),
	FMCLK(CKGEN, 23, "ssusb_sys_ck"),
	FMCLK(CKGEN, 24, "ssusb_xhci_ck"),
	FMCLK(CKGEN, 25, "spm_ck"),
	FMCLK(CKGEN, 26, "i2c_ck"),
	FMCLK(CKGEN, 27, "pwm_ck"),
	FMCLK(CKGEN, 28, "dsp_ck"),
	FMCLK(CKGEN, 29, "nfi2x_ck"),
	FMCLK(CKGEN, 30, "spinfi_ck"),
	FMCLK(CKGEN, 31, "ecc_ck"),
	FMCLK(CKGEN, 32, "gcpu_ck"),
	FMCLK(CKGEN, 33, "gcpum_ck"),
	FMCLK(CKGEN, 34, "mbist_diag_ck"),
	FMCLK(CKGEN, 35, "ip0_nna_ck"),
	FMCLK(CKGEN, 36, "ip1_nna_ck"),
	FMCLK(CKGEN, 37, "ip2_wfst_ck"),
	FMCLK(CKGEN, 38, "sflash_ck"),
	FMCLK(CKGEN, 39, "sram_ck"),
	FMCLK(CKGEN, 40, "mm_ck"),
	FMCLK(CKGEN, 41, "dpi0_ck"),
	FMCLK(CKGEN, 42, "dbg_atclk_ck"),
	FMCLK(CKGEN, 43, "occ_104m_ck"),
	FMCLK(CKGEN, 44, "occ_68m_ck"),
	FMCLK(CKGEN, 45, "occ_182m_ck"),
	FMCLK(CKGEN, 48, "f_ufs_mp_sap_cfg_ck"),
	FMCLK(CKGEN, 49, "f_ufs_tick1us_ck"),
	FMCLK(CKGEN, 50, "hd_faxi_east_ck"),
	FMCLK(CKGEN, 51, "hd_faxi_west_ck"),
	FMCLK(CKGEN, 52, "hd_faxi_north_ck"),
	FMCLK(CKGEN, 53, "hd_faxi_south_ck"),
	FMCLK(CKGEN, 54, "hg_fmipicfg_tx_ck"),
	FMCLK(ABIST,  1, "AD_ARMPLL_CK"),
	FMCLK(ABIST,  3, "AD_MAINPLL_CK"),
	FMCLK(ABIST,  4, "AD_XO_LP_CLK_26M"),
	FMCLK(ABIST, 10, "AD_MAINPLL_H546M_CK"),
	FMCLK(ABIST, 11, "AD_MAINPLL_H364M_CK"),
	FMCLK(ABIST, 12, "AD_MAINPLL_H218P4M_CK"),
	FMCLK(ABIST, 13, "AD_MAINPLL_H156M_CK"),
	FMCLK(ABIST, 14, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST, 15, "AD_USB20_192M_CK"),
	FMCLK(ABIST, 16, "AD_UNIVPLL_624M_CK"),
	FMCLK(ABIST, 17, "AD_UNIVPLL_416M_CK"),
	FMCLK(ABIST, 18, "AD_UNIVPLL_249P6M_CK"),
	FMCLK(ABIST, 19, "AD_UNIVPLL_178P3M_CK"),
	FMCLK(ABIST, 20, "AD_MDPLL1_FS26M_CK"),
	FMCLK(ABIST, 25, "clkrtc"),
	FMCLK(ABIST, 26, "DA_PLLGP1_26M_CK"),
	FMCLK(ABIST, 27, "DA_PLLGP2_26M_CK"),
	FMCLK(ABIST, 28, "AD_MSDCPLL_CK"),
	FMCLK(ABIST, 32, "AD_APLL1_CK"),
	FMCLK(ABIST, 33, "AD_APLL2_CK"),
	FMCLK(ABIST, 41, "DA_USB20_48M_DIV_CK"),
	FMCLK(ABIST, 42, "DA_UNIV_48M_DIV_CK"),
	FMCLK(ABIST, 51, "AD_TCONPLL_CK"),
	FMCLK(ABIST, 52, "AD_DSPPLL_CK"),
	FMCLK(ABIST, 53, "AD_XO_HP_CLK_26M"),
	FMCLK(ABIST, 54, "AD_IPPLL_CK"),
	FMCLK(ABIST, 56, "AD_SYS_26M_CK"),
	FMCLK(ABIST, 57, "AD_CLKSQ_26M_CK"),
	FMCLK(ABIST, 58, "AD_26M_CLKMON"),
	{}
};

#define PLL_HP_CON0		(rb[apmixedsys].virt + 0xE04)
#define CLK_MISC_CFG_0		(rb[topckgen].virt + 0x104)
#define CLK_DBG_CFG		(rb[topckgen].virt + 0x10C)
#define CLK26CALI_0		(rb[topckgen].virt + 0x220)
#define CLK26CALI_1		(rb[topckgen].virt + 0x224)

static unsigned int mt_get_abist_freq(int k1, unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;
	unsigned int clk26cali_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC) | (ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0,
		  (clk_misc_cfg_0 & 0x00FFFFFF) | (k1 << 24));

	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & ~0x1000) | 0x1000);
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & ~0x10) | 0x10);


	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(30);
		i++;
		if (i > 10)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	if (i > 10)
		return 0;
	else
		return (output * (k1 + 1));
}

static unsigned int mt_get_ckgen_freq(int k1, unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0;
	unsigned int clk26cali_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC) | (ID << 8) | (0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF));

	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & ~0x1000) | 0x1000);
	clk_writel(CLK26CALI_0, (clk_readl(CLK26CALI_0) & ~0x10) | 0x10);

	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(30);
		i++;
		if (i > 10)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	if (i > 10)
		return 0;
	else
		return output;

}

static u32 fmeter_freq(enum FMETER_TYPE type, int clk)
{
	if (type == ABIST)
		return mt_get_abist_freq(3, clk);
	else if (type == CKGEN)
		return mt_get_ckgen_freq(0, clk);

	return 0;
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int clk)
{
	u32 last_freq = 0;
	u32 freq = fmeter_freq(type, clk);
	u32 maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

struct bak {
	u32 pll_hp_con0;
};

static void *prepare_fmeter(void)
{
	static struct bak regs;

	regs.pll_hp_con0 = clk_readl(PLL_HP_CON0);

	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	return &regs;
}

static void unprepare_fmeter(void *data)
{
	struct bak *regs = data;

	/* restore old setting */
	clk_writel(PLL_HP_CON0, regs->pll_hp_con0);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type)
		return measure_stable_fmeter_freq(fclk->type, fclk->id);

	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* plls */
		"armpll",
		"mainpll",
		"univpll2",
		"msdcpll",
		"apll1",
		"apll2",
		"ippll",
		"dsppll",
		"tconpll",
		/* topckgen */
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll_d3",
		"syspll2_d2",
		"syspll2_d4",
		"syspll2_d8",
		"syspll_d5",
		"syspll3_d4",
		"syspll_d7",
		"syspll4_d2",
		"univpll",
		"univpll_d2",
		"univpll1_d2",
		"univpll1_d4",
		"univpll1_d8",
		"univpll_d3",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll_d5",
		"univpll3_d2",
		"univpll3_d4",
		"tconpll_d2",
		"tconpll_d4",
		"tconpll_d8",
		"tconpll_d16",
		"tconpll_d32",
		"tconpll_d64",
		"usb20_192m_ck",
		"usb20_192m_d2",
		"usb20_192m_d4_t",
		"apll1_ck",
		"apll1_d2",
		"apll1_d3",
		"apll1_d4",
		"apll1_d8",
		"apll1_d16",
		"apll2_ck",
		"apll2_d2",
		"apll2_d3",
		"apll2_d4",
		"apll2_d8",
		"apll2_d16",
		"clk26m_ck",
		"sys_26m_d2",
		"msdcpll_ck",
		"msdcpll_d2",
		"dsppll_ck",
		"dsppll_d2",
		"dsppll_d4",
		"dsppll_d8",
		"ippll_ck",
		"ippll_d2",
		"nfi2x_ck_d2",
		"axi_sel",
		"mem_sel",
		"uart_sel",
		"spi_sel",
		"spis_sel",
		"msdc50_0_hc_sel",
		"msdc2_2_hc_sel",
		"msdc50_0_sel",
		"msdc50_2_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"hapll1_sel",
		"hapll2_sel",
		"a2sys_sel",
		"a1sys_sel",
		"asm_l_sel",
		"asm_m_sel",
		"asm_h_sel",
		"aud_spdif_sel",
		"aud_1_sel",
		"aud_2_sel",
		"ssusb_sys_sel",
		"ssusb_xhci_sel",
		"spm_sel",
		"i2c_sel",
		"pwm_sel",
		"dsp_sel",
		"nfi2x_sel",
		"spinfi_sel",
		"ecc_sel",
		"gcpu_sel",
		"gcpu_cpm_sel",
		"mbist_diag_sel",
		"ip0_nna_sel",
		"ip1_nna_sel",
		"ip2_wfst_sel",
		"sflash_sel",
		"sram_sel",
		"mm_sel",
		"dpi0_sel",
		"dbg_atclk_sel",
		"occ_104m_sel",
		"occ_68m_sel",
		"occ_182m_sel",
		"apll_fi2si1_sel",
		"apll_ftdmin_sel",
		"apll_fi2so1_sel",
		"apll12_ck_div7",
		"apll12_ck_div8",
		"apll12_ck_div9",
		"i2si1_mck",
		"tdmin_mck",
		"i2so1_mck",
		"usb20_48m_en",
		"univpll_48m_en",
		"ssusb_top_ck_en",
		"ssusb_phy_ck_en",
		"conn_32k",
		"conn_26m",
		"dsp_32k",
		"dsp_26m",
		/* infrasys */
		"infra_apxgpt",
		"infra_icusb",
		"infra_gce",
		"infra_therm",
		"infra_pwm_hclk",
		"infra_pwm1",
		"infra_pwm2",
		"infra_pwm3",
		"infra_pwm4",
		"infra_pwm5",
		"infra_pwm",
		"infra_uart0",
		"infra_uart1",
		"infra_uart2",
		"infra_dsp_uart",
		"infra_gce_26m",
		"infra_cqdma_fpc",
		"infra_btif",
		"infra_spi",
		"infra_msdc0",
		"infra_msdc1",
		"infra_dvfsrc",
		"infra_gcpu",
		"infra_trng",
		"infra_auxadc",
		"infra_auxadc_md",
		"infra_ap_dma",
		"infra_debugsys",
		"infra_audio",
		"infra_flashif",
		"infra_pwm_fb6",
		"infra_pwm_fb7",
		"infra_aud_asrc",
		"infra_aud_26m",
		"infra_spis",
		"infra_cq_dma",
		"infra_ap_msdc0",
		"infra_md_msdc0",
		"infra_msdc0_src",
		"infra_msdc1_src",
		"infra_irrx_26m",
		"infra_irrx_32k",
		"infra_i2c0_axi",
		"infra_i2c1_axi",
		"infra_i2c2_axi",
		"infra_nfi",
		"infra_nfiecc",
		"infra_nfi_hclk",
		"infra_susb_133",
		"infra_usb_sys",
		"infra_usb_xhci",
		"infra_dsp_axi",
		/* mcusys */
		"mcu_bus_sel",
		/* ipsys */
		"ip_clk_nna0_pwr",
		"ip_clk_nna1_pwr",
		"ip_clk_wfst_pwr",
		"ip_hd_faxi_ck",
		"ip_test_26m_ck",
		"ip_emi_ck_gate",
		"ip_sram_occ",
		"ip_nna0_occ",
		"ip_nna1_occ",
		"ip_wfst_occ",
		/* mmsys */
		"mm_pipeline0",
		"mm_pipeline1",
		"mm_pipeline2",
		"mm_pipeline3",
		"mm_pipeline4",
		"mm_pipeline5",
		"mm_pipeline7",
		"mm_dpi_tmp0",
		"mm_dpi_tmp1",
		"mm_disp_fake",
		"mm_smi_common",
		"mm_smi_larb0",
		"mm_smi_comm0",
		"mm_smi_comm1",
		/* imgsys */
		"img_mdp_rdma0",
		"img_mdp_rsz0",
		"img_mdp_tdshp0",
		"img_mdp_wrot0",
		"img_disp_ovl0",
		"img_disp_wdma0",
		"img_disp_gamma0",
		"img_disp_di",
		"img_fake",
		"img_smi_larb1",
		"img_jpgdec",
		"img_pngdec",
		"img_imgrz",
		/* end */
		NULL
	};

	return clks;
}

/*
 * clkdbg pwr_status
 */

static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0]  = "",
		[1]  = "CONN",
		[2]  = "DDRPHY",
		[3]  = "",
		[4]  = "",
		[5]  = "",
		[6]  = "INFRA",
		[7]  = "",
		[8]  = "MP0_CPUTOP",
		[9]  = "MP0_CPU0",
		[10] = "MP0_CPU1",
		[11] = "",
		[12] = "",
		[13] = "",
		[14] = "",
		[15] = "MM",
		[16] = "IMG",
		[17] = "DSP",
		[18] = "",
		[19] = "",
		[20] = "USB",
		[21] = "",
		[22] = "",
		[23] = "AUDSRC",
		[24] = "AUDAFE",
		[25] = "IP0_NNA",
		[26] = "IP1_NNA",
		[27] = "IP2_WFST",
		[28] = "",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	return pwr_names;
}

u32 get_spm_pwr_status(void)
{
	static void __iomem *scpsys_base, *pwr_sta, *pwr_sta_2nd;

	if (scpsys_base == NULL || pwr_sta == NULL || pwr_sta_2nd == NULL) {
		scpsys_base = ioremap(0x10006000, PAGE_SIZE);
		pwr_sta = scpsys_base + 0x180;
		pwr_sta_2nd = scpsys_base + 0x184;
	}

	return clk_readl(pwr_sta) & clk_readl(pwr_sta_2nd);
}

/*
 * clkdbg dump_clks
 */

static void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
		{"mmsys", BIT(15)},
		{"imgsys", BIT(15) | BIT(16)},
	};

	size_t i;
	const char *pvdname = pvdck->provider_name;

	if (pvdname == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt8512_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
	.get_spm_pwr_status = get_spm_pwr_status,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt8512_init(void)
{
	if (of_machine_is_compatible("mediatek,mt8512") == 0)
		return -ENODEV;

	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt8512_ops);

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt8512_init);
