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
#include <linux/syscore_ops.h>
#include <linux/version.h>

#define WARN_ON_CHECK_PLL_FAIL		0
#define CLKDBG_CCF_API_4_4	1

#define TAG	"[clkchk] "

#define clk_warn(fmt, args...)	pr_notice(TAG fmt, ##args)

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

#endif /* !CLKDBG_CCF_API_4_4 */

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

static const char *ccf_state(struct clk_hw *hw)
{
	if (__clk_get_enable_count(hw->clk))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = get_all_clk_names();

	clk_warn("enabled clks:\n");

	for (; *cn; cn++) {
		struct clk *c = __clk_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || !c_hw)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (!p_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !__clk_get_enable_count(c))
			continue;

		pr_notice_once(TAG "[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			__clk_get_enable_count(c),
			clk_hw_get_rate(c_hw),
			p_hw ? clk_hw_get_name(p_hw) : "- ");
	}
}

static void check_pll_off(void)
{
	static const char * const off_pll_names[] = {
		"univpll2",
		"msdcpll",
		"apll1",
		"apll2",
		"ippll",
		"dsppll",
		"tconpll",
		NULL
	};

	static struct clk *off_plls[ARRAY_SIZE(off_pll_names)];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

	if (!off_plls[0]) {
		const char * const *pn;

		for (pn = off_pll_names, c = off_plls; *pn; pn++, c++)
			*c = __clk_lookup(*pn);
	}

	for (c = off_plls; *c; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (!c_hw)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid) {
		clk_warn("unexpected unclosed PLL: %s\n", buf);
		print_enabled_clks();

#if WARN_ON_CHECK_PLL_FAIL
		WARN_ON(1);
#endif
	}
}

static int clkchk_syscore_suspend(void)
{
	check_pll_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

static int __init clkchk_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8512"))
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
subsys_initcall(clkchk_init);
