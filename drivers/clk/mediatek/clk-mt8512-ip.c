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
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8512-clk.h>

static const struct mtk_gate_regs ip0_cg_regs = {
	.set_ofs = 0x100,
	.clr_ofs = 0x100,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs ip1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs ip2_cg_regs = {
	.set_ofs = 0x108,
	.clr_ofs = 0x108,
	.sta_ofs = 0x108,
};

static const struct mtk_gate_regs ip3_cg_regs = {
	.set_ofs = 0x110,
	.clr_ofs = 0x110,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs ip4_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x114,
	.sta_ofs = 0x114,
};

static const struct mtk_gate_regs ip5_cg_regs = {
	.set_ofs = 0x118,
	.clr_ofs = 0x118,
	.sta_ofs = 0x118,
};

static const struct mtk_gate_regs ip6_cg_regs = {
	.set_ofs = 0x98,
	.clr_ofs = 0x98,
	.sta_ofs = 0x98,
};

static const struct mtk_gate_regs ip7_cg_regs = {
	.set_ofs = 0x9c,
	.clr_ofs = 0x9c,
	.sta_ofs = 0x9c,
};

static const struct mtk_gate_regs ip8_cg_regs = {
	.set_ofs = 0xa0,
	.clr_ofs = 0xa0,
	.sta_ofs = 0xa0,
};

static const struct mtk_gate_regs ip9_cg_regs = {
	.set_ofs = 0xfc,
	.clr_ofs = 0xfc,
	.sta_ofs = 0xfc,
};

#define GATE_IP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP6(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip6_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP7(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip7_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP8(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip8_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IP9(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ip9_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate ip_clks[] = {
	/* IP0 */
	GATE_IP0(CLK_IP_EMI_CK_GATE, "ip_emi_ck_gate", "clk_null", 0),
	/* IP1 */
	GATE_IP1(CLK_IP_SRAM_OCC_GATE, "ip_sram_occ", "sram_sel", 0),
	/* IP2 */
	GATE_IP2(CLK_IP_HD_FAXI_CK, "ip_hd_faxi_ck", "axi_sel", 0),
	/* IP3 */
	GATE_IP3(CLK_IP_NNA0_PWR_GATE, "ip_clk_nna0_pwr", "ip0_nna_sel", 0),
	/* IP4 */
	GATE_IP4(CLK_IP_NNA1_PWR_GATE, "ip_clk_nna1_pwr", "ip1_nna_sel", 0),
	/* IP5 */
	GATE_IP5(CLK_IP_WFST_PWR_GATE, "ip_clk_wfst_pwr", "ip2_wfst_sel", 0),
	/* IP6 */
	GATE_IP6(CLK_IP_NNA0_OCC_GATE, "ip_nna0_occ", "ip0_nna_sel", 0),
	/* IP7 */
	GATE_IP7(CLK_IP_NNA1_OCC_GATE, "ip_nna1_occ", "ip1_nna_sel", 0),
	/* IP8 */
	GATE_IP8(CLK_IP_WFST_OCC_GATE, "ip_wfst_occ", "ip2_wfst_sel", 0),
	/* IP9 */
	GATE_IP9(CLK_IP_TEST_26M, "ip_test_26m_ck", "clk26m_ck", 0),
};

static int clk_mt8512_ip_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IP_NR_CLK);

	mtk_clk_register_gates(node, ip_clks, ARRAY_SIZE(ip_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8512_ip[] = {
	{ .compatible = "mediatek,mt8512-ipsys", },
	{}
};

static struct platform_driver clk_mt8512_ip_drv = {
	.probe = clk_mt8512_ip_probe,
	.driver = {
		.name = "clk-mt8512-ip",
		.of_match_table = of_match_clk_mt8512_ip,
	},
};

builtin_platform_driver(clk_mt8512_ip_drv);
