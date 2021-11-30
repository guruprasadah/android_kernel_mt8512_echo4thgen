/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include "adsp_clk.h"
#include "adsp_helper.h"


struct clk *clk_handle[ADSP_CLK_NUM];


int platform_parse_clock(struct device *dev, void *data)
{
	clk_handle[CLK_TOP_SRAM_SEL] = devm_clk_get(dev, "sram_sel");
	if (IS_ERR(clk_handle[CLK_TOP_SRAM_SEL])) {
		dev_err(dev, "clk_get(\"sram_sel\") failed\n");
		return PTR_ERR(clk_handle[CLK_TOP_SRAM_SEL]);
	}

	clk_handle[CLK_TOP_DSP_SEL] = devm_clk_get(dev, "dsp_sel");
	if (IS_ERR(clk_handle[CLK_TOP_DSP_SEL])) {
		dev_err(dev, "clk_get(\"dsp_sel\") failed\n");
		return PTR_ERR(clk_handle[CLK_TOP_DSP_SEL]);
	}

	clk_handle[CLK_TOP_CLK26M] = devm_clk_get(dev, "clk26m_ck");
	if (IS_ERR(clk_handle[CLK_TOP_CLK26M])) {
		dev_err(dev, "clk_get(\"clk26m_ck\") failed\n");
		return PTR_ERR(clk_handle[CLK_TOP_CLK26M]);
	}

	clk_handle[CLK_TOP_DSP_26M] = devm_clk_get(dev, "dsp_26m");
	if (IS_ERR(clk_handle[CLK_TOP_DSP_26M])) {
		dev_err(dev, "clk_get(\"dsp_26m\") failed\n");
		return PTR_ERR(clk_handle[CLK_TOP_DSP_26M]);
	}
	clk_handle[CLK_TOP_DSP_32K] = devm_clk_get(dev, "dsp_32k");
	if (IS_ERR(clk_handle[CLK_TOP_DSP_32K])) {
		dev_err(dev, "clk_get(\"dsp_32k\") failed\n");
		return PTR_ERR(clk_handle[CLK_TOP_DSP_32K]);
	}

	clk_handle[CLK_IFRA_DSP_AXI] = devm_clk_get(dev, "infra_dsp_axi");
	if (IS_ERR(clk_handle[CLK_IFRA_DSP_AXI])) {
		dev_err(dev, "clk_get(\"infra_dsp_axi\") failed\n");
		return PTR_ERR(clk_handle[CLK_IFRA_DSP_AXI]);
	}

	clk_handle[CLK_IFRA_DSP_UART] = devm_clk_get(dev, "infra_dsp_uart");
	if (IS_ERR(clk_handle[CLK_IFRA_DSP_UART])) {
		dev_err(dev, "clk_get(\"infra_dsp_uart\") failed\n");
		return PTR_ERR(clk_handle[CLK_IFRA_DSP_UART]);
	}

	return 0;
}

int adsp_enable_clock(struct device *dev)
{
	int ret = 0;

	ret = clk_prepare_enable(clk_handle[CLK_TOP_SRAM_SEL]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(sram_sel) fail %d\n",
			__func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(clk_handle[CLK_TOP_DSP_SEL]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(dsp_sel) fail %d\n",
		       __func__, ret);
		return ret;
	}
	ret = clk_prepare_enable(clk_handle[CLK_TOP_DSP_26M]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(dsp_26m) fail %d\n",
		       __func__, ret);
		return ret;
	}
	ret = clk_prepare_enable(clk_handle[CLK_TOP_DSP_32K]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(dsp_32k) fail %d\n",
		       __func__, ret);
		return ret;
	}
	ret = clk_prepare_enable(clk_handle[CLK_IFRA_DSP_AXI]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(infra_dsp_axi) fail %d\n",
		       __func__, ret);
		return ret;
	}
	ret = clk_prepare_enable(clk_handle[CLK_IFRA_DSP_UART]);
	if (ret) {
		dev_err(dev, "%s clk_prepare_enable(infra_dsp_uart) fail %d\n",
		       __func__, ret);
		return ret;
	}

	return ret;
}

void adsp_disable_clock(struct device *dev)
{
	clk_disable_unprepare(clk_handle[CLK_IFRA_DSP_UART]);
	clk_disable_unprepare(clk_handle[CLK_IFRA_DSP_AXI]);
	clk_disable_unprepare(clk_handle[CLK_TOP_DSP_32K]);
	clk_disable_unprepare(clk_handle[CLK_TOP_DSP_26M]);
	clk_disable_unprepare(clk_handle[CLK_TOP_DSP_SEL]);
	clk_disable_unprepare(clk_handle[CLK_TOP_SRAM_SEL]);
}

int adsp_default_clk_init(struct device *dev, int enable)
{
	int ret = 0;

	pr_debug("+%s (%x)\n", __func__, enable);

	if (enable) {
		ret = clk_set_parent(clk_handle[CLK_TOP_DSP_SEL],
					     clk_handle[CLK_TOP_CLK26M]);
		if (ret) {
			dev_err(dev, "failed to adsp_set_top_mux: %d\n", ret);
			goto TAIL;
		}
		ret = adsp_enable_clock(dev);
		if (ret) {
			dev_err(dev, "failed to adsp_enable_clock: %d\n", ret);
			goto TAIL;
		}
	} else
		adsp_disable_clock(dev);

	pr_debug("-%s (%x)\n", __func__, enable);

TAIL:
	return ret;
}

int adsp_pm_register_early(struct device *dev)
{
	int ret;

	/* Fix DSPPLL switch-issue before pm_runtime_enable */
	ret = clk_set_parent(clk_handle[CLK_TOP_DSP_SEL],
					     clk_handle[CLK_TOP_CLK26M]);
	if (ret)
		goto TAIL;
	pm_runtime_enable(dev);
TAIL:
	return ret;
}

void adsp_pm_unregister_last(struct device *dev)
{
	pm_runtime_disable(dev);
}

int adsp_power_enable(struct device *dev)
{
	int ret;

	ret = pm_runtime_get_sync(dev);

	return ret;
}

void adsp_power_disable(struct device *dev)
{
	pm_runtime_put_sync(dev);
}

