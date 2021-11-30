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

static const struct mtk_gate_regs img0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs img1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate img_clks[] = {
	/* IMG0 */
	GATE_IMG0(CLK_IMG_MDP_RDMA0, "img_mdp_rdma0", "mm_sel", 0),
	GATE_IMG0(CLK_IMG_MDP_RSZ0, "img_mdp_rsz0", "mm_sel", 2),
	GATE_IMG0(CLK_IMG_MDP_TDSHP0, "img_mdp_tdshp0", "mm_sel", 4),
	GATE_IMG0(CLK_IMG_MDP_WROT0, "img_mdp_wrot0", "mm_sel", 5),
	GATE_IMG0(CLK_IMG_DISP_OVL0_21, "img_disp_ovl0", "mm_sel", 8),
	GATE_IMG0(CLK_IMG_DISP_WDMA0, "img_disp_wdma0", "mm_sel", 11),
	GATE_IMG0(CLK_IMG_DISP_GAMMA0, "img_disp_gamma0", "mm_sel", 15),
	GATE_IMG0(CLK_IMG_DISP_DITHER0, "img_disp_di", "mm_sel", 16),
	GATE_IMG0(CLK_IMG_FAKE, "img_fake", "mm_sel", 21),
	GATE_IMG0(CLK_IMG_SMI_LARB1, "img_smi_larb1", "mm_sel", 23),
	/* IMG1 */
	GATE_IMG1(CLK_IMG_JPGDEC, "img_jpgdec", "mm_sel", 1),
	GATE_IMG1(CLK_IMG_PNGDEC, "img_pngdec", "mm_sel", 2),
	GATE_IMG1(CLK_IMG_IMGRZ, "img_imgrz", "mm_sel", 3),
};

static int clk_mt8512_img_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	return r;
}

static const struct of_device_id of_match_clk_mt8512_img[] = {
	{ .compatible = "mediatek,mt8512-imgsys", },
	{}
};

static struct platform_driver clk_mt8512_img_drv = {
	.probe = clk_mt8512_img_probe,
	.driver = {
		.name = "clk-mt8512-img",
		.of_match_table = of_match_clk_mt8512_img,
	},
};

builtin_platform_driver(clk_mt8512_img_drv);
