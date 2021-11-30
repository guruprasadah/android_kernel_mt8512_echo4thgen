/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
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
#ifndef MTK_IOMMU_SMI_H
#define MTK_IOMMU_SMI_H

#include <linux/bitops.h>
#include <linux/device.h>

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
#include <linux/platform_device.h>
#include <../drivers/misc/mediatek/smi/smi_public.h>

struct mtk_smi_pair {
	unsigned int	offset;
	unsigned int	value;
};

struct mtk_smi_dev {
	unsigned int	index;
	struct device	*dev;
	void __iomem	*base;
	unsigned int	*mmu;

	unsigned int	nr_clks;
	struct clk	**clks;
	atomic_t	clk_ref_cnts;

	unsigned int		nr_config_pairs;
	struct mtk_smi_pair	*config_pairs;

	unsigned int		nr_scens;
	unsigned int		nr_scen_pairs;
	struct mtk_smi_pair	**scen_pairs;

	unsigned int	nr_debugs;
	unsigned int	*debugs;
	unsigned int	busy_cnts;
};

extern struct mtk_smi_dev *common;
extern struct mtk_smi_dev **larbs;
int mtk_smi_clk_ref_cnts_read(struct mtk_smi_dev *smi);
int mtk_smi_dev_enable(struct mtk_smi_dev *smi);
int mtk_smi_dev_disable(struct mtk_smi_dev *smi);
int mtk_smi_config_set(struct mtk_smi_dev *smi,
	const unsigned int scen_indx, const bool mtcmos);

int smi_register(struct platform_driver *drv);
int smi_unregister(struct platform_driver *drv);
#endif
#if IS_ENABLED(CONFIG_MTK_SMI)

#define MTK_LARB_NR_MAX		16

#define MTK_SMI_MMU_EN(port)	BIT(port)

struct mtk_smi_larb_iommu {
	struct device *dev;
	unsigned int   mmu;
};

struct mtk_smi_iommu {
	unsigned int larb_nr;
	struct mtk_smi_larb_iommu larb_imu[MTK_LARB_NR_MAX];
};

/*
 * mtk_smi_larb_get: Enable the power domain and clocks for this local arbiter.
 *                   It also initialize some basic setting(like iommu).
 * mtk_smi_larb_put: Disable the power domain and clocks for this local arbiter.
 * Both should be called in non-atomic context.
 *
 * Returns 0 if successful, negative on failure.
 */
int mtk_smi_larb_get(struct device *larbdev);
void mtk_smi_larb_put(struct device *larbdev);

/* Only for MET */
int mtk_smi_larb_clock_enable(int larbid);
void mtk_smi_larb_clock_disable(int larbid);
void __iomem *mtk_smi_larb_get_common_base(void);
void __iomem *mtk_smi_larb_get_larb_base(int larbid);
#define mtk_smi_common_clock_enable  mtk_smi_larb_clock_enable(0)
#define mtk_smi_common_clock_disable  mtk_smi_larb_clock_disable(0)
/* End for MET. */

#else

static inline int mtk_smi_larb_get(struct device *larbdev)
{
	return 0;
}

static inline void mtk_smi_larb_put(struct device *larbdev) { }

#endif

#endif
