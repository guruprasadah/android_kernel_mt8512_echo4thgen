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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8512-clk.h>

static DEFINE_SPINLOCK(mt8512_clk_lock);

static const struct mtk_fixed_clk top_fixed_clks[] = {
	FIXED_CLK(CLK_TOP_CLK_NULL, "clk_null", NULL, 0),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "mainpll", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "mainpll", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "mainpll", 1, 16),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "mainpll", 1, 32),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "mainpll", 1, 6),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "mainpll", 1, 12),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "mainpll", 1, 24),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "mainpll", 1, 20),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "mainpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univpll2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL1_D8, "univpll1_d8", "univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll", 1, 20),
	FACTOR(CLK_TOP_TCONPLL_D2, "tconpll_d2", "tconpll", 1, 2),
	FACTOR(CLK_TOP_TCONPLL_D4, "tconpll_d4", "tconpll", 1, 4),
	FACTOR(CLK_TOP_TCONPLL_D8, "tconpll_d8", "tconpll", 1, 8),
	FACTOR(CLK_TOP_TCONPLL_D16, "tconpll_d16", "tconpll", 1, 16),
	FACTOR(CLK_TOP_TCONPLL_D32, "tconpll_d32", "tconpll", 1, 32),
	FACTOR(CLK_TOP_TCONPLL_D64, "tconpll_d64", "tconpll", 1, 64),
	FACTOR(CLK_TOP_USB20_192M, "usb20_192m_ck", "univpll", 2, 13),
	FACTOR(CLK_TOP_USB20_192M_D2, "usb20_192m_d2", "usb20_192m_ck", 1, 2),
	FACTOR(CLK_TOP_USB20_192M_D4_T, "usb20_192m_d4_t", "usb20_192m_ck",
		1, 4),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D3, "apll1_d3", "apll1", 1, 3),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1", 1, 8),
	FACTOR(CLK_TOP_APLL1_D16, "apll1_d16", "apll1", 1, 16),
	FACTOR(CLK_TOP_APLL2, "apll2_ck", "apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2", "apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D3, "apll2_d3", "apll2", 1, 3),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4", "apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8", "apll2", 1, 8),
	FACTOR(CLK_TOP_APLL2_D16, "apll2_d16", "apll2", 1, 16),
	FACTOR(CLK_TOP_CLK26M, "clk26m_ck", "clk26m", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(CLK_TOP_DSPPLL, "dsppll_ck", "dsppll", 1, 1),
	FACTOR(CLK_TOP_DSPPLL_D2, "dsppll_d2", "dsppll", 1, 2),
	FACTOR(CLK_TOP_DSPPLL_D4, "dsppll_d4", "dsppll", 1, 4),
	FACTOR(CLK_TOP_DSPPLL_D8, "dsppll_d8", "dsppll", 1, 8),
	FACTOR(CLK_TOP_IPPLL, "ippll_ck", "ippll", 1, 1),
	FACTOR(CLK_TOP_IPPLL_D2, "ippll_d2", "ippll", 1, 2),
	FACTOR(CLK_TOP_NFI2X_CK_D2, "nfi2x_ck_d2", "nfi2x_sel", 1, 2),
};

static const char * const mcu_bus_parents[] = {
	"clk26m_ck",
	"armpll",
	"mainpll",
	"univpll_d2"
};

static struct mtk_composite mcu_muxes[] = {
	/* bus_pll_divider_cfg */
	MUX_GATE_FLAGS(CLK_MCU_BUS_SEL, "mcu_bus_sel", mcu_bus_parents,
	    0x7C0, 9, 2, -1, CLK_IS_CRITICAL),
};

static const char * const axi_parents[] = {
	"clk26m_ck",
	"syspll1_d4",
	"univpll3_d2",
	"syspll1_d8",
	"sys_26m_d2",
	"clk32k"
};

static const char * const mem_parents[] = {
	"dsppll_ck",
	"ippll_ck",
	"clk26m_ck",
	"univpll_d3"
};

static const char * const uart_parents[] = {
	"clk26m_ck",
	"univpll2_d8"
};

static const char * const spi_parents[] = {
	"clk26m_ck",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4",
	"syspll1_d4",
	"univpll3_d2",
	"univpll2_d4",
	"syspll4_d2"
};

static const char * const spis_parents[] = {
	"clk26m_ck",
	"univpll_d3",
	"syspll_d3",
	"univpll1_d2",
	"univpll2_d2",
	"univpll1_d4",
	"univpll2_d4",
	"syspll4_d2"
};

static const char * const msdc50_0_hc_parents[] = {
	"clk26m_ck",
	"syspll1_d2",
	"univpll1_d4",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m_ck",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4",
	"syspll1_d4",
	"syspll2_d4",
	"univpll2_d8"
};

static const char * const msdc50_2_parents[] = {
	"clk26m_ck",
	"msdcpll_ck",
	"univpll_d3",
	"univpll1_d2",
	"syspll1_d2",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4"
};

static const char * const audio_parents[] = {
	"clk26m_ck",
	"univpll2_d8",
	"apll1_d4",
	"apll2_d4"
};

static const char * const aud_intbus_parents[] = {
	"clk26m_ck",
	"syspll1_d4",
	"univpll3_d2",
	"apll2_d8",
	"sys_26m_d2",
	"apll1_d8",
	"univpll3_d4"
};

static const char * const hapll1_parents[] = {
	"clk26m_ck",
	"apll1_ck",
	"apll1_d2",
	"apll1_d3",
	"apll1_d4",
	"apll1_d8",
	"apll1_d16",
	"sys_26m_d2"
};

static const char * const hapll2_parents[] = {
	"clk26m_ck",
	"apll2_ck",
	"apll2_d2",
	"apll2_d3",
	"apll2_d4",
	"apll2_d8",
	"apll2_d16",
	"sys_26m_d2"
};

static const char * const asm_l_parents[] = {
	"clk26m_ck",
	"univpll2_d4",
	"univpll2_d2",
	"syspll_d5"
};

static const char * const aud_spdif_parents[] = {
	"clk26m_ck",
	"univpll_d2",
	"dsppll_ck"
};

static const char * const aud_1_parents[] = {
	"clk26m_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"clk26m_ck",
	"apll2_ck"
};

static const char * const ssusb_sys_parents[] = {
	"clk26m_ck",
	"univpll3_d4",
	"univpll2_d4",
	"univpll3_d2"
};

static const char * const spm_parents[] = {
	"clk26m_ck",
	"syspll1_d8"
};

static const char * const i2c_parents[] = {
	"clk26m_ck",
	"sys_26m_d2",
	"univpll3_d4",
	"univpll3_d2",
	"syspll1_d8",
	"syspll2_d8",
	"clk32k"
};

static const char * const pwm_parents[] = {
	"clk26m_ck",
	"univpll3_d4",
	"syspll1_d8",
	"univpll2_d4",
	"sys_26m_d2",
	"clk32k"
};

static const char * const dsp_parents[] = {
	"clk26m_ck",
	"dsppll_ck",
	"dsppll_d2",
	"dsppll_d4",
	"dsppll_d8",
	"apll2_d4",
	"sys_26m_d2",
	"clk32k"
};

static const char * const nfi2x_parents[] = {
	"clk26m_ck",
	"syspll2_d2",
	"syspll_d7",
	"syspll_d3",
	"syspll2_d4",
	"msdcpll_d2",
	"univpll1_d2",
	"univpll_d5"
};

static const char * const spinfi_parents[] = {
	"clk26m_ck",
	"univpll2_d8",
	"univpll3_d4",
	"syspll1_d8",
	"syspll4_d2",
	"syspll2_d4",
	"univpll2_d4",
	"univpll3_d2"
};

static const char * const ecc_parents[] = {
	"clk26m_ck",
	"syspll_d5",
	"syspll_d3",
	"univpll_d3"
};

static const char * const gcpu_parents[] = {
	"clk26m_ck",
	"univpll_d3",
	"syspll_d3",
	"univpll1_d2",
	"syspll1_d2",
	"univpll2_d2"
};

static const char * const gcpu_cpm_parents[] = {
	"clk26m_ck",
	"univpll2_d2",
	"syspll2_d2",
	"univpll1_d4"
};

static const char * const mbist_diag_parents[] = {
	"clk26m_ck",
	"sys_26m_d2"
};

static const char * const ip0_nna_parents[] = {
	"clk26m_ck",
	"dsppll_ck",
	"dsppll_d2",
	"dsppll_d4",
	"ippll_ck",
	"sys_26m_d2",
	"ippll_d2",
	"msdcpll_d2"
};

static const char * const ip2_wfst_parents[] = {
	"clk26m_ck",
	"univpll_d3",
	"univpll1_d2",
	"univpll2_d2",
	"ippll_ck",
	"ippll_d2",
	"sys_26m_d2",
	"msdcpll_ck"
};

static const char * const sflash_parents[] = {
	"clk26m_ck",
	"syspll1_d16",
	"syspll2_d8",
	"syspll3_d4",
	"univpll3_d4",
	"univpll1_d8",
	"usb20_192m_d2",
	"univpll2_d4"
};

static const char * const sram_parents[] = {
	"clk26m_ck",
	"dsppll_ck",
	"univpll_d3",
	"syspll1_d2",
	"apll1_ck",
	"apll2_ck",
	"syspll1_d4",
	"sys_26m_d2"
};

static const char * const mm_parents[] = {
	"clk26m_ck",
	"syspll_d3",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll1_d2",
	"univpll_d3"
};

static const char * const dpi0_parents[] = {
	"clk26m_ck",
	"tconpll_d2",
	"tconpll_d4",
	"tconpll_d8",
	"tconpll_d16",
	"tconpll_d32",
	"tconpll_d64"
};

static const char * const dbg_atclk_parents[] = {
	"clk26m_ck",
	"univpll1_d2",
	"univpll_d5"
};

static const char * const occ_104m_parents[] = {
	"univpll2_d4",
	"univpll2_d8"
};

static const char * const occ_68m_parents[] = {
	"syspll1_d8",
	"univpll2_d8"
};

static const char * const occ_182m_parents[] = {
	"syspll2_d2",
	"univpll1_d4",
	"univpll2_d8"
};

static const char * const apll_fi2si1_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static struct mtk_composite top_misc_muxes[] = {
	/* CLK_AUDDIV_4 */
	MUX(CLK_TOP_APLL_FI2SI1_SEL, "apll_fi2si1_sel", apll_fi2si1_parents,
	    0x340, 11, 1),
	MUX(CLK_TOP_APLL_FTDMIN_SEL, "apll_ftdmin_sel", apll_fi2si1_parents,
	    0x340, 12, 1),
	MUX(CLK_TOP_APLL_FI2SO1_SEL, "apll_fi2so1_sel", apll_fi2si1_parents,
	    0x340, 13, 1),
};

static const struct mtk_mux top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel",
	    axi_parents, 0x040, 0x044, 0x048, 0, 3, 7,
	    0x4, 0, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SEL, "mem_sel",
	    mem_parents, 0x040, 0x044, 0x048, 8, 2, 15, 0x4, 1),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel",
	    uart_parents, 0x040, 0x044, 0x048, 16, 1, 23, 0x4, 2),
	MUX_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel",
	    spi_parents, 0x040, 0x044, 0x048, 24, 3, 31, 0x4, 3),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_SPIS_SEL, "spis_sel",
	    spis_parents, 0x050, 0x054, 0x058, 0, 3, 7, 0x4, 4),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HC_SEL, "msdc50_0_hc_sel",
	    msdc50_0_hc_parents, 0x050, 0x054, 0x058, 8, 2, 15, 0x4, 5),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC2_2_HC_SEL, "msdc2_2_hc_sel",
	    msdc50_0_hc_parents, 0x050, 0x054, 0x058, 16, 2, 23, 0x4, 6),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
	    msdc50_0_parents, 0x050, 0x054, 0x058, 24, 3, 31, 0x4, 7),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_2_SEL, "msdc50_2_sel",
	    msdc50_2_parents, 0x060, 0x064, 0x068, 0, 3, 7, 0x4, 8),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
	    msdc50_0_parents, 0x060, 0x064, 0x068, 8, 3, 15, 0x4, 9),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel",
	    audio_parents, 0x060, 0x064, 0x068, 16, 2, 23, 0x4, 10),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
	    aud_intbus_parents, 0x060, 0x064, 0x068, 24, 3, 31, 0x4, 11),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_HAPLL1_SEL, "hapll1_sel",
	    hapll1_parents, 0x070, 0x074, 0x078, 0, 3, 7, 0x4, 12),
	MUX_CLR_SET_UPD(CLK_TOP_HAPLL2_SEL, "hapll2_sel",
	    hapll2_parents, 0x070, 0x074, 0x078, 8, 3, 15, 0x4, 13),
	MUX_CLR_SET_UPD(CLK_TOP_A2SYS_SEL, "a2sys_sel",
	    hapll1_parents, 0x070, 0x074, 0x078, 16, 3, 23, 0x4, 14),
	MUX_CLR_SET_UPD(CLK_TOP_A1SYS_SEL, "a1sys_sel",
	    hapll2_parents, 0x070, 0x074, 0x078, 24, 3, 31, 0x4, 15),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_ASM_L_SEL, "asm_l_sel",
	    asm_l_parents, 0x080, 0x084, 0x088, 0, 2, 7, 0x4, 16),
	MUX_CLR_SET_UPD(CLK_TOP_ASM_M_SEL, "asm_m_sel",
	    asm_l_parents, 0x080, 0x084, 0x088, 8, 2, 15, 0x4, 17),
	MUX_CLR_SET_UPD(CLK_TOP_ASM_H_SEL, "asm_h_sel",
	    asm_l_parents, 0x080, 0x084, 0x088, 16, 2, 23, 0x4, 18),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_SPDIF_SEL, "aud_spdif_sel",
	    aud_spdif_parents, 0x080, 0x084, 0x088, 24, 2, 31, 0x4, 19),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel",
	    aud_1_parents, 0x090, 0x094, 0x098, 0, 1, 7, 0x4, 20),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL, "aud_2_sel",
	    aud_2_parents, 0x090, 0x094, 0x098, 8, 1, 15, 0x4, 21),
	MUX_CLR_SET_UPD(CLK_TOP_SSUSB_SYS_SEL, "ssusb_sys_sel",
	    ssusb_sys_parents, 0x090, 0x094, 0x098, 16, 2, 23, 0x4, 22),
	MUX_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL, "ssusb_xhci_sel",
	    ssusb_sys_parents, 0x090, 0x094, 0x098, 24, 2, 31, 0x4, 23),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SPM_SEL, "spm_sel",
	    spm_parents, 0x0a0, 0x0a4, 0x0a8, 0, 1, 7,
	    0x4, 24, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel",
	    i2c_parents, 0x0a0, 0x0a4, 0x0a8, 8, 3, 15, 0x4, 25),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel",
	    pwm_parents, 0x0a0, 0x0a4, 0x0a8, 16, 3, 23, 0x4, 26),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL, "dsp_sel",
	    dsp_parents, 0x0a0, 0x0a4, 0x0a8, 24, 3, 31, 0x4, 27),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_NFI2X_SEL, "nfi2x_sel",
	    nfi2x_parents, 0x0b0, 0x0b4, 0x0b8, 0, 3, 7, 0x4, 28),
	MUX_CLR_SET_UPD(CLK_TOP_SPINFI_SEL, "spinfi_sel",
	    spinfi_parents, 0x0b0, 0x0b4, 0x0b8, 8, 3, 15, 0x4, 29),
	MUX_CLR_SET_UPD(CLK_TOP_ECC_SEL, "ecc_sel",
	    ecc_parents, 0x0b0, 0x0b4, 0x0b8, 16, 2, 23, 0x4, 30),
	MUX_CLR_SET_UPD(CLK_TOP_GCPU_SEL, "gcpu_sel",
	    gcpu_parents, 0x0b0, 0x0b4, 0x0b8, 24, 3, 31, 0x4, 31),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_GCPU_CPM_SEL, "gcpu_cpm_sel",
	    gcpu_cpm_parents, 0x0c0, 0x0c4, 0x0c8, 0, 2, 7, 0x8, 0),
	MUX_CLR_SET_UPD(CLK_TOP_MBIST_DIAG_SEL, "mbist_diag_sel",
	    mbist_diag_parents, 0x0c0, 0x0c4, 0x0c8, 8, 1, 15, 0x8, 1),
	MUX_CLR_SET_UPD(CLK_TOP_IP0_NNA_SEL, "ip0_nna_sel",
	    ip0_nna_parents, 0x0c0, 0x0c4, 0x0c8, 16, 3, 23, 0x8, 2),
	MUX_CLR_SET_UPD(CLK_TOP_IP1_NNA_SEL, "ip1_nna_sel",
	    ip0_nna_parents, 0x0c0, 0x0c4, 0x0c8, 24, 3, 31, 0x8, 3),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_IP2_WFST_SEL, "ip2_wfst_sel",
	    ip2_wfst_parents, 0x0d0, 0x0d4, 0x0d8, 0, 3, 7, 0x8, 4),
	MUX_CLR_SET_UPD(CLK_TOP_SFLASH_SEL, "sflash_sel",
	    sflash_parents, 0x0d0, 0x0d4, 0x0d8, 8, 3, 15, 0x8, 5),
	MUX_CLR_SET_UPD(CLK_TOP_SRAM_SEL, "sram_sel",
	    sram_parents, 0x0d0, 0x0d4, 0x0d8, 16, 3, 23, 0x8, 6),
	MUX_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel",
	    mm_parents, 0x0d0, 0x0d4, 0x0d8, 24, 3, 31, 0x8, 7),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_DPI0_SEL, "dpi0_sel",
	    dpi0_parents, 0x0e0, 0x0e4, 0x0e8, 0, 3, 7, 0x8, 8),
	MUX_CLR_SET_UPD(CLK_TOP_DBG_ATCLK_SEL, "dbg_atclk_sel",
	    dbg_atclk_parents, 0x0e0, 0x0e4, 0x0e8, 8, 2, 15, 0x8, 9),
	MUX_CLR_SET_UPD(CLK_TOP_OCC_104M_SEL, "occ_104m_sel",
	    occ_104m_parents, 0x0e0, 0x0e4, 0x0e8, 16, 1, 23, 0x8, 10),
	MUX_CLR_SET_UPD(CLK_TOP_OCC_68M_SEL, "occ_68m_sel",
	    occ_68m_parents, 0x0e0, 0x0e4, 0x0e8, 24, 1, 31, 0x8, 11),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_OCC_182M_SEL, "occ_182m_sel",
	    occ_182m_parents, 0x0ec, 0x0f0, 0x0f4, 0, 2, 7, 0x8, 12),
};

#define DIV_ADJ_F(_id, _name, _parent, _reg, _shift, _width, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.div_reg = _reg,				\
		.div_shift = _shift,				\
		.div_width = _width,				\
		.clk_divider_flags = _flags,			\
}

static const struct mtk_clk_divider top_adj_divs[] = {
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV7, "apll12_ck_div7", "apll_fi2si1_sel",
			0x344, 0, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV8, "apll12_ck_div8", "apll_ftdmin_sel",
			0x344, 8, 8, CLK_DIVIDER_ROUND_CLOSEST),
	DIV_ADJ_F(CLK_TOP_APLL12_CK_DIV9, "apll12_ck_div9", "apll_fi2so1_sel",
			0x344, 16, 8, CLK_DIVIDER_ROUND_CLOSEST),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x340,
	.clr_ofs = 0x340,
	.sta_ofs = 0x340,
};

#define GATE_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_TOP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate top_clks[] = {
	/* TOP0 */
	GATE_TOP0(CLK_TOP_CONN_32K, "conn_32k", "clk32k", 10),
	GATE_TOP0(CLK_TOP_CONN_26M, "conn_26m", "clk26m_ck", 11),
	GATE_TOP0(CLK_TOP_DSP_32K, "dsp_32k", "clk32k", 16),
	GATE_TOP0(CLK_TOP_DSP_26M, "dsp_26m", "clk26m_ck", 17),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_USB20_48M_EN, "usb20_48m_en", "usb20_192m_d4_t", 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M_EN, "univpll_48m_en",
		"usb20_192m_d4_t", 9),
	GATE_TOP1(CLK_TOP_SSUSB_TOP_CK_EN, "ssusb_top_ck_en", "clk_null", 22),
	GATE_TOP1(CLK_TOP_SSUSB_PHY_CK_EN, "ssusb_phy_ck_en", "clk_null", 23),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_I2SI1_MCK, "i2si1_mck", "apll12_ck_div7", 0),
	GATE_TOP2(CLK_TOP_TDMIN_MCK, "tdmin_mck", "apll12_ck_div8", 1),
	GATE_TOP2(CLK_TOP_I2SO1_MCK, "i2so1_mck", "apll12_ck_div9", 2),
};

static const struct mtk_gate_regs infra0_cg_regs = {
	.set_ofs = 0x294,
	.clr_ofs = 0x294,
	.sta_ofs = 0x294,
};

static const struct mtk_gate_regs infra1_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infra2_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra3_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infra4_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

static const struct mtk_gate_regs infra5_cg_regs = {
	.set_ofs = 0xd0,
	.clr_ofs = 0xd4,
	.sta_ofs = 0xd8,
};

#define GATE_INFRA0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_INFRA1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate infra_clks[] = {
	/* INFRA0 */
	GATE_INFRA0(CLK_INFRA_DSP_AXI, "infra_dsp_axi", "axi_sel", 8),
	/* INFRA1 */
	GATE_INFRA1(CLK_INFRA_ICUSB, "infra_icusb", "axi_sel", 8),
	GATE_INFRA1(CLK_INFRA_GCE, "infra_gce", "axi_sel", 9),
	GATE_INFRA1(CLK_INFRA_THERM, "infra_therm", "axi_sel", 10),
	GATE_INFRA1(CLK_INFRA_PWM_HCLK, "infra_pwm_hclk", "axi_sel", 15),
	GATE_INFRA1(CLK_INFRA_PWM1, "infra_pwm1", "pwm_sel", 16),
	GATE_INFRA1(CLK_INFRA_PWM2, "infra_pwm2", "pwm_sel", 17),
	GATE_INFRA1(CLK_INFRA_PWM3, "infra_pwm3", "pwm_sel", 18),
	GATE_INFRA1(CLK_INFRA_PWM4, "infra_pwm4", "pwm_sel", 19),
	GATE_INFRA1(CLK_INFRA_PWM5, "infra_pwm5", "pwm_sel", 20),
	GATE_INFRA1(CLK_INFRA_PWM, "infra_pwm", "pwm_sel", 21),
	GATE_INFRA1(CLK_INFRA_UART0, "infra_uart0", "uart_sel", 22),
	GATE_INFRA1(CLK_INFRA_UART1, "infra_uart1", "uart_sel", 23),
	GATE_INFRA1(CLK_INFRA_UART2, "infra_uart2", "uart_sel", 24),
	GATE_INFRA1(CLK_INFRA_DSP_UART, "infra_dsp_uart", "uart_sel", 26),
	GATE_INFRA1(CLK_INFRA_GCE_26M, "infra_gce_26m", "clk26m_ck", 27),
	GATE_INFRA1(CLK_INFRA_CQDMA_FPC, "infra_cqdma_fpc", "axi_sel", 28),
	GATE_INFRA1(CLK_INFRA_BTIF, "infra_btif", "axi_sel", 31),
	/* INFRA2 */
	GATE_INFRA2(CLK_INFRA_SPI, "infra_spi", "spi_sel", 1),
	GATE_INFRA2(CLK_INFRA_MSDC0, "infra_msdc0", "msdc50_0_hc_sel", 2),
	GATE_INFRA2(CLK_INFRA_MSDC1, "infra_msdc1", "axi_sel", 4),
	GATE_INFRA2(CLK_INFRA_DVFSRC, "infra_dvfsrc", "clk26m_ck", 7),
	GATE_INFRA2(CLK_INFRA_GCPU, "infra_gcpu", "axi_sel", 8),
	GATE_INFRA2(CLK_INFRA_TRNG, "infra_trng", "axi_sel", 9),
	GATE_INFRA2(CLK_INFRA_AUXADC, "infra_auxadc", "clk26m_ck", 10),
	GATE_INFRA2(CLK_INFRA_AUXADC_MD, "infra_auxadc_md", "clk26m_ck", 14),
	GATE_INFRA2(CLK_INFRA_AP_DMA, "infra_ap_dma", "axi_sel", 18),
	GATE_INFRA2(CLK_INFRA_DEBUGSYS, "infra_debugsys", "axi_sel", 24),
	GATE_INFRA2(CLK_INFRA_AUDIO, "infra_audio", "axi_sel", 25),
	GATE_INFRA2(CLK_INFRA_FLASHIF, "infra_flashif", "sflash_sel", 29),
	/* INFRA3 */
	GATE_INFRA3(CLK_INFRA_PWM_FB6, "infra_pwm_fb6", "pwm_sel", 0),
	GATE_INFRA3(CLK_INFRA_PWM_FB7, "infra_pwm_fb7", "pwm_sel", 1),
	GATE_INFRA3(CLK_INFRA_AUD_ASRC, "infra_aud_asrc", "axi_sel", 3),
	GATE_INFRA3(CLK_INFRA_AUD_26M, "infra_aud_26m", "clk26m_ck", 4),
	GATE_INFRA3(CLK_INFRA_SPIS, "infra_spis", "axi_sel", 6),
	GATE_INFRA3(CLK_INFRA_CQ_DMA, "infra_cq_dma", "axi_sel", 27),
	/* INFRA4 */
	GATE_INFRA4(CLK_INFRA_AP_MSDC0, "infra_ap_msdc0", "msdc50_0_sel", 7),
	GATE_INFRA4(CLK_INFRA_MD_MSDC0, "infra_md_msdc0", "msdc50_0_sel", 8),
	GATE_INFRA4(CLK_INFRA_MSDC0_SRC, "infra_msdc0_src", "msdc50_0_sel", 9),
	GATE_INFRA4(CLK_INFRA_MSDC1_SRC, "infra_msdc1_src", "msdc30_1_sel", 10),
	GATE_INFRA4(CLK_INFRA_IRRX_26M, "infra_irrx_26m", "axi_sel", 22),
	GATE_INFRA4(CLK_INFRA_IRRX_32K, "infra_irrx_32k", "clk32k", 23),
	GATE_INFRA4(CLK_INFRA_I2C0_AXI, "infra_i2c0_axi", "i2c_sel", 24),
	GATE_INFRA4(CLK_INFRA_I2C1_AXI, "infra_i2c1_axi", "i2c_sel", 25),
	GATE_INFRA4(CLK_INFRA_I2C2_AXI, "infra_i2c2_axi", "i2c_sel", 26),
	/* INFRA5 */
	GATE_INFRA5(CLK_INFRA_NFI, "infra_nfi", "nfi2x_ck_d2", 1),
	GATE_INFRA5(CLK_INFRA_NFIECC, "infra_nfiecc", "nfi2x_ck_d2", 2),
	GATE_INFRA5(CLK_INFRA_NFI_HCLK, "infra_nfi_hclk", "axi_sel", 3),
	GATE_INFRA5(CLK_INFRA_SUSB_133, "infra_susb_133", "axi_sel", 7),
	GATE_INFRA5(CLK_INFRA_USB_SYS, "infra_usb_sys", "ssusb_sys_sel", 9),
	GATE_INFRA5(CLK_INFRA_USB_XHCI, "infra_usb_xhci", "ssusb_xhci_sel", 11),
};

#define MT8512_PLL_FMAX		(3800UL * MHZ)
#define MT8512_PLL_FMIN		(1500UL * MHZ)

#define CON0_MT8512_RST_BAR	BIT(23)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
		_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,		\
		_tuner_en_bit,	_pcw_reg, _pcw_shift, _div_table,	\
		_rst_bar_mask, _pcw_chg_reg) {				\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT8512_PLL_FMAX,				\
		.fmin = MT8512_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = 8,						\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcw_chg_reg = _pcw_chg_reg,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit, _pcw_reg,		\
			_pcw_shift, _rst_bar_mask, _pcw_chg_reg)	\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_pcwbits, _pd_reg, _pd_shift,			\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, NULL, _rst_bar_mask,	\
			_pcw_chg_reg)					\

static const struct mtk_pll_div_table armpll_div_table[] = {
	{ .div = 0, .freq = MT8512_PLL_FMAX },
	{ .div = 1, .freq = 1850000000 },
	{ .div = 2, .freq = 750000000 },
	{ .div = 3, .freq = 375000000 },
	{ .div = 4, .freq = 182500000 },
	{ } /* sentinel */
};

static const struct mtk_pll_div_table dsppll_div_table[] = {
	{ .div = 0, .freq = MT8512_PLL_FMAX },
	{ .div = 1, .freq = 1600000000 },
	{ .div = 2, .freq = 750000000 },
	{ .div = 3, .freq = 400000000 },
	{ .div = 4, .freq = 200000000 },
	{ } /* sentinel */
};

static const struct mtk_pll_data plls[] = {
	PLL_B(CLK_APMIXED_ARMPLL, "armpll", 0x030C, 0x0318, 0x00000001,
	    0, 22, 0x0310, 24, 0, 0, 0, 0x0310, 0, armpll_div_table, 0, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0228, 0x0234, 0x00000001,
	    HAVE_RST_BAR, 22, 0x022C, 24, 0, 0, 0, 0x022C, 0,
	    CON0_MT8512_RST_BAR, 0),
	PLL(CLK_APMIXED_UNIVPLL2, "univpll2", 0x0208, 0x0214, 0x00000001,
	    HAVE_RST_BAR, 22, 0x020C, 24, 0, 0, 0, 0x020C, 0,
	    CON0_MT8512_RST_BAR, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x0350, 0x035C, 0x00000001,
	    0, 22, 0x0354, 24, 0, 0, 0, 0x0354, 0, 0, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x031C, 0x032C, 0x00000001,
	    0, 32, 0x0320, 24, 0x0040, 0x000C, 0, 0x0324, 0, 0, 0x0320),
	PLL(CLK_APMIXED_APLL2, "apll2", 0x0360, 0x0370, 0x00000001,
	    0, 32, 0x0364, 24, 0x004C, 0x000C, 5, 0x0368, 0, 0, 0x0364),
	PLL(CLK_APMIXED_IPPLL, "ippll", 0x0374, 0x0380, 0x00000001,
	    0, 22, 0x0378, 24, 0, 0, 0, 0x0378, 0, 0, 0),
	PLL_B(CLK_APMIXED_DSPPLL, "dsppll", 0x0390, 0x039C, 0x00000001,
	    0, 22, 0x0394, 24, 0, 0, 0, 0x0394, 0, dsppll_div_table, 0, 0),
	PLL(CLK_APMIXED_TCONPLL, "tconpll", 0x03A0, 0x03AC, 0x00000001,
	    0, 22, 0x03A4, 24, 0, 0, 0, 0x03A4, 0, 0, 0),
};

static const struct mtk_fixed_factor top_early_divs[] = {
	FACTOR(CLK_TOP_SYS_26M_D2, "sys_26m_d2", "clk26m", 1, 2),
};

static struct clk_onecell_data *top_clk_data;

static void clk_mt8512_top_init_early(struct device_node *node)
{
	int r, i;

	if (!top_clk_data) {
		top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

		for (i = 0; i < CLK_TOP_NR_CLK; i++)
			top_clk_data->clks[i] = ERR_PTR(-EPROBE_DEFER);
	}

	mtk_clk_register_factors(top_early_divs, ARRAY_SIZE(top_early_divs),
			top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, top_clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);
}

CLK_OF_DECLARE_DRIVER(mt8512_topckgen, "mediatek,mt8512-topckgen",
			clk_mt8512_top_init_early);


static int clk_mt8512_top_probe(struct platform_device *pdev)
{
	int r;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	if (!top_clk_data)
		top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_fixed_clks(top_fixed_clks,
		ARRAY_SIZE(top_fixed_clks), top_clk_data);
	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), top_clk_data);
	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes),
					  node, &mt8512_clk_lock, top_clk_data);
	mtk_clk_register_composites(top_misc_muxes, ARRAY_SIZE(top_misc_muxes),
				    base, &mt8512_clk_lock, top_clk_data);
	mtk_clk_register_dividers(top_adj_divs, ARRAY_SIZE(top_adj_divs), base,
				  &mt8512_clk_lock, top_clk_data);
	mtk_clk_register_gates(node, top_clks, ARRAY_SIZE(top_clks),
			       top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, top_clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8512_infra_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_INFRA_NR_CLK);

	mtk_clk_register_gates(node, infra_clks, ARRAY_SIZE(infra_clks),
		clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	mtk_register_reset_controller_set_clr(node, 4, 0x120);

	return r;
}

static int clk_mt8512_apmixed_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static int clk_mt8512_mcu_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_MCU_NR_CLK);

	mtk_clk_register_composites(mcu_muxes, ARRAY_SIZE(mcu_muxes),
	    base, &mt8512_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8512[] = {
	{
		.compatible = "mediatek,mt8512-apmixedsys",
		.data = clk_mt8512_apmixed_probe,
	}, {
		.compatible = "mediatek,mt8512-topckgen",
		.data = clk_mt8512_top_probe,
	}, {
		.compatible = "mediatek,mt8512-mcucfg",
		.data = clk_mt8512_mcu_probe,
	}, {
		.compatible = "mediatek,mt8512-infrasys",
		.data = clk_mt8512_infra_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt8512_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt8512_drv = {
	.probe = clk_mt8512_probe,
	.driver = {
		.name = "clk-mt8512",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt8512,
	},
};

static int __init clk_mt8512_init(void)
{
	return platform_driver_register(&clk_mt8512_drv);
}

arch_initcall(clk_mt8512_init);
