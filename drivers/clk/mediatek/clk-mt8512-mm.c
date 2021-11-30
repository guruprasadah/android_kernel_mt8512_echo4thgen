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

static const struct mtk_gate_regs mm_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	GATE_MM(CLK_MM_PIPELINE0, "mm_pipeline0", "mm_sel", 0),
	GATE_MM(CLK_MM_PIPELINE1, "mm_pipeline1", "mm_sel", 1),
	GATE_MM(CLK_MM_PIPELINE2, "mm_pipeline2", "mm_sel", 2),
	GATE_MM(CLK_MM_PIPELINE3, "mm_pipeline3", "mm_sel", 3),
	GATE_MM(CLK_MM_PIPELINE4, "mm_pipeline4", "mm_sel", 4),
	GATE_MM(CLK_MM_PIPELINE5, "mm_pipeline5", "mm_sel", 5),
	GATE_MM(CLK_MM_PIPELINE7, "mm_pipeline7", "mm_sel", 7),
	GATE_MM(CLK_MM_DPI0_DPI_TMP0, "mm_dpi_tmp0", "dpi0_sel", 19),
	GATE_MM(CLK_MM_DPI0_DPI_TMP1, "mm_dpi_tmp1", "dpi0_sel", 20),
	GATE_MM(CLK_MM_DISP_FAKE, "mm_disp_fake", "mm_sel", 21),
	GATE_MM(CLK_MM_SMI_COMMON, "mm_smi_common", "mm_sel", 22),
	GATE_MM(CLK_MM_SMI_LARB0, "mm_smi_larb0", "mm_sel", 23),
	GATE_MM(CLK_MM_SMI_COMM0, "mm_smi_comm0", "mm_sel", 24),
	GATE_MM(CLK_MM_SMI_COMM1, "mm_smi_comm1", "mm_sel", 25),
};

static int clk_mt8512_mm_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8512_mm[] = {
	{ .compatible = "mediatek,mt8512-mmsys", },
	{}
};

static struct platform_driver clk_mt8512_mm_drv = {
	.probe = clk_mt8512_mm_probe,
	.driver = {
		.name = "clk-mt8512-mm",
		.of_match_table = of_match_clk_mt8512_mm,
	},
};

builtin_platform_driver(clk_mt8512_mm_drv);
