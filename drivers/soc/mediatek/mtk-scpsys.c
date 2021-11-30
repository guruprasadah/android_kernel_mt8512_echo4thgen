/*
 * Copyright (c) 2015 Pengutronix, Sascha Hauer <kernel@pengutronix.de>
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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/infracfg.h>
#include <linux/soc/mediatek/scpsys-ext.h>

#ifdef CONFIG_COMMON_CLK_MT2701
#include <dt-bindings/power/mt2701-power.h>
#endif
#include <dt-bindings/power/mt2712-power.h>
#include <dt-bindings/power/mt8173-power.h>
#include <dt-bindings/power/mt8512-power.h>
#include <dt-bindings/power/mt8518-power.h>

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    (jiffies_to_usecs(HZ))

#define MTK_SCPD_ACTIVE_WAKEUP		BIT(0)
#define MTK_SCPD_FWAIT_SRAM		BIT(1)
#define MTK_SCPD_KEEP_DEFAULT_OFF	BIT(2)
#define MTK_SCPD_CAPS(_scpd, _x)	((_scpd)->data->caps & (_x))

#define SPM_VDE_PWR_CON			0x0210
#define SPM_MFG_PWR_CON			0x0214
#define SPM_VEN_PWR_CON			0x0230
#define SPM_ISP_PWR_CON			0x0238
#define SPM_DIS_PWR_CON			0x023c
#define SPM_CONN_PWR_CON		0x0280
#define SPM_VEN2_PWR_CON		0x0298
#define SPM_AUDIO_PWR_CON		0x029c	/* MT8173, MT2712 */
#define SPM_BDP_PWR_CON			0x029c	/* MT2701 */
#define SPM_ETH_PWR_CON			0x02a0
#define SPM_HIF_PWR_CON			0x02a4
#define SPM_IFR_MSC_PWR_CON		0x02a8
#define SPM_MFG_2D_PWR_CON		0x02c0
#define SPM_MFG_ASYNC_PWR_CON		0x02c4
#define SPM_USB_PWR_CON			0x02cc
#define SPM_USB2_PWR_CON		0x02d4	/* MT2712 */
#define SPM_AUDAFE_PWR_CON		0x0d10	/* MT8518 */
#define SPM_AUDSRC_PWR_CON		0x0d14	/* MT8518 */

#define SPM_PWR_STATUS			0x060c
#define SPM_PWR_STATUS_2ND		0x0610

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)
#define PWR_SRAM_CLKISO_BIT		BIT(5)
#define PWR_SRAM_ISOINT_B_BIT		BIT(6)

#define PWR_STATUS_CONN			BIT(1)
#define PWR_STATUS_DISP			BIT(3)
#define PWR_STATUS_MFG			BIT(4)
#define PWR_STATUS_ISP			BIT(5)
#define PWR_STATUS_VDEC			BIT(7)
#define PWR_STATUS_BDP			BIT(14)
#define PWR_STATUS_ETH			BIT(15)
#define PWR_STATUS_HIF			BIT(16)
#define PWR_STATUS_IFR_MSC		BIT(17)
#define PWR_STATUS_USB2			BIT(19)	/* MT2712 */
#define PWR_STATUS_VENC_LT		BIT(20)
#define PWR_STATUS_VENC			BIT(21)
#define PWR_STATUS_MFG_2D		BIT(22)	/* MT8173 */
#define PWR_STATUS_MFG_ASYNC		BIT(23)	/* MT8173 */
#define PWR_STATUS_AUDIO		BIT(24)	/* MT8173, MT2712 */
#define PWR_STATUS_USB			BIT(25)	/* MT8173, MT2712 */

enum clk_id {
	CLK_NONE,
	CLK_MM,
	CLK_MFG,
	CLK_VENC,
	CLK_VENC_LT,
	CLK_ETHIF,
	CLK_JPGDEC,
	CLK_AUDIO,
	CLK_MAX,
};

static const char * const clk_names[] = {
	NULL,
	"mm",
	"mfg",
	"venc",
	"venc_lt",
	"ethif",
	"jpgdec",
	"audio",
	NULL,
};

#define MAX_CLKS	3
#define MAX_SUBSYS_CLKS 10

/**
 * struct scp_domain_data - scp domain data for power on/off flow
 * @name: The domain name.
 * @sta_mask: The mask for power on/off status bit.
 * @ctl_offs: The offset for main power control register.
 * @sram_pdn_bits: The mask for sram power control bits.
 * @sram_pdn_ack_bits: The mask for sram power control acked bits.
 * @bus_prot_mask: The mask for single step bus protection.
 * @clk_id: The basic clock needs to be enabled before enabling certain
 *          power domains.
 * @basic_clk_name: provide the same purpose with field "clk_id"
 *                  by declaring basic clock prefix name rather than clk_id.
 * @subsys_clk_prefix: The prefix name of the clocks need to be enabled
 *                     before releasing bus protection.
 * @caps: The flag for active wake-up action.
 * @bp_table: The mask table for multiple step bus protection.
 */
struct scp_domain_data {
	const char *name;
	u32 sta_mask;
	int ctl_offs;
	bool sram_iso_ctrl;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	u32 bus_prot_mask;
	enum clk_id clk_id[MAX_CLKS];
	const char *basic_clk_name[MAX_CLKS];
	const char *subsys_clk_prefix;
	u8 caps;
	struct bus_prot bp_table[MAX_STEPS];
};

struct scp;

struct scp_domain {
	struct generic_pm_domain genpd;
	struct scp *scp;
	struct clk *clk[MAX_CLKS];
	struct clk *subsys_clk[MAX_SUBSYS_CLKS];
	const struct scp_domain_data *data;
	struct regulator *supply;
};

struct scp_ctrl_reg {
	int pwr_sta_offs;
	int pwr_sta2nd_offs;
};

struct scp {
	struct scp_domain *domains;
	struct genpd_onecell_data pd_data;
	struct device *dev;
	void __iomem *base;
	struct regmap *infracfg;
	struct regmap *smi_common;
	struct scp_ctrl_reg ctrl_reg;
	bool bus_prot_reg_update;
};

struct scp_subdomain {
	int origin;
	int subdomain;
};

struct scp_soc_data {
	const struct scp_domain_data *domains;
	int num_domains;
	const struct scp_subdomain *subdomains;
	int num_subdomains;
	const struct scp_ctrl_reg regs;
	bool bus_prot_reg_update;
};

static int scpsys_domain_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	u32 status = readl(scp->base + scp->ctrl_reg.pwr_sta_offs) &
						scpd->data->sta_mask;
	u32 status2 = readl(scp->base + scp->ctrl_reg.pwr_sta2nd_offs) &
						scpd->data->sta_mask;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */

	if (status && status2)
		return true;
	if (!status && !status2)
		return false;

	return -EINVAL;
}

static int scpsys_regulator_enable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;
	else
		return regulator_enable(scpd->supply);
}

static int scpsys_regulator_disable(struct scp_domain *scpd)
{
	if (!scpd->supply)
		return 0;
	else
		return regulator_disable(scpd->supply);
}

static int scpsys_clk_enable(struct clk *clk[], int max_num)
{
	int i, ret = 0;

	for (i = 0; i < max_num && clk[i]; i++) {
		ret = clk_prepare_enable(clk[i]);
		if (ret) {
			for (--i; i >= 0; i--)
				clk_disable_unprepare(clk[i]);

			break;
		}
	}

	return ret;
}

static int scpsys_clk_disable(struct clk *clk[], int max_num)
{
	int i;

	for (i = max_num - 1; i >= 0; i--) {
		if (clk[i])
			clk_disable_unprepare(clk[i]);
	}

	return 0;
}

static int scpsys_sram_enable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	u32 val;
	u32 pdn_ack = scpd->data->sram_pdn_ack_bits;
	int tmp, ret = 0;

	val = readl(ctl_addr) & ~scpd->data->sram_pdn_bits;
	writel(val, ctl_addr);

	/* Either wait until SRAM_PDN_ACK all 0 or have a force wait */
	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_FWAIT_SRAM)) {
		/*
		 * Currently, MTK_SCPD_FWAIT_SRAM is necessary only for
		 * MT7622_POWER_DOMAIN_WB and thus just a trivial setup
		 * is applied here.
		 */
		usleep_range(12000, 12100);
	} else {
		/* Either wait until SRAM_PDN_ACK all 1 or 0 */
		ret = readl_poll_timeout(ctl_addr, tmp,
				(tmp & pdn_ack) == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	}

	if (scpd->data->sram_iso_ctrl)	{
		val = readl(ctl_addr) | PWR_SRAM_ISOINT_B_BIT;
		writel(val, ctl_addr);
		udelay(1);
		val &= ~PWR_SRAM_CLKISO_BIT;
		writel(val, ctl_addr);
	}

	return ret;
}

static int scpsys_sram_disable(struct scp_domain *scpd, void __iomem *ctl_addr)
{
	u32 val;
	u32 pdn_ack = scpd->data->sram_pdn_ack_bits;
	int tmp, ret = 0;

	if (scpd->data->sram_iso_ctrl)	{
		val = readl(ctl_addr);
		val |= PWR_SRAM_CLKISO_BIT;
		writel(val, ctl_addr);
		val &= ~PWR_SRAM_ISOINT_B_BIT;
		writel(val, ctl_addr);
		udelay(1);
	}

	val = readl(ctl_addr) | scpd->data->sram_pdn_bits;
	writel(val, ctl_addr);

	/* Either wait until SRAM_PDN_ACK all 1 or 0 */
	ret = readl_poll_timeout(ctl_addr, tmp,
			(tmp & pdn_ack) == pdn_ack,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	return ret;
}

static int scpsys_bus_protect_enable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	int ret = 0;

	if (scpd->data->bus_prot_mask) {
		ret = mtk_infracfg_set_bus_protection(scp->infracfg,
				scpd->data->bus_prot_mask,
				scp->bus_prot_reg_update);
	} else if (scpd->data->bp_table[0].mask) {
		ret = mtk_scpsys_ext_set_bus_protection(scpd->data->bp_table,
				scp->infracfg,
				scp->smi_common);
	}

	return ret;
}

static int scpsys_bus_protect_disable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	int ret = 0;

	if (scpd->data->bus_prot_mask) {
		ret = mtk_infracfg_clear_bus_protection(scp->infracfg,
				scpd->data->bus_prot_mask,
				scp->bus_prot_reg_update);
	} else if (scpd->data->bp_table[0].mask) {
		ret = mtk_scpsys_ext_clear_bus_protection(
				scpd->data->bp_table,
				scp->infracfg,
				scp->smi_common);
	}

	return ret;
}

static int scpsys_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp;

	ret = scpsys_regulator_enable(scpd);
	if (ret < 0)
		return ret;

	ret = scpsys_clk_enable(scpd->clk, MAX_CLKS);
	if (ret)
		goto err_clk;

	/* subsys power on */
	val = readl(ctl_addr);
	val |= PWR_ON_BIT;
	writel(val, ctl_addr);
	val |= PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 1 */
	ret = readx_poll_timeout(scpsys_domain_is_on, scpd, tmp, tmp > 0,
				 MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0) {
		dev_err(scp->dev, "pwr ack timeout %s\n", genpd->name);
		goto err_pwr_ack;
	}

	val &= ~PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ISO_BIT;
	writel(val, ctl_addr);

	val |= PWR_RST_B_BIT;
	writel(val, ctl_addr);

	ret = scpsys_clk_enable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
	if (ret < 0)
		goto err_pwr_ack;

	ret = scpsys_sram_enable(scpd, ctl_addr);
	if (ret < 0) {
		dev_err(scp->dev, "sram enable failed %s\n", genpd->name);
		goto err_sram;
	}

	ret = scpsys_bus_protect_disable(scpd);
	if (ret < 0) {
		dev_err(scp->dev, "bus protect failed %s\n", genpd->name);
		goto err_sram;
	}

	return 0;

err_sram:
	scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);
err_pwr_ack:
	scpsys_clk_disable(scpd->clk, MAX_CLKS);
err_clk:
	scpsys_regulator_disable(scpd);

	dev_err(scp->dev, "Failed to power on domain %s\n", genpd->name);

	return ret;
}

static int scpsys_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp;

	ret = scpsys_bus_protect_enable(scpd);
	if (ret < 0) {
		dev_err(scp->dev, "bus protect failed %s\n", genpd->name);
		goto out;
	}

	ret = scpsys_sram_disable(scpd, ctl_addr);
	if (ret < 0) {
		dev_err(scp->dev, "sram disable failed %s\n", genpd->name);
		goto out;
	}

	ret = scpsys_clk_disable(scpd->subsys_clk, MAX_SUBSYS_CLKS);

	/* subsys power off */
	val = readl(ctl_addr);
	val |= PWR_ISO_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val |= PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 0 */
	ret = readx_poll_timeout(scpsys_domain_is_on, scpd, tmp, tmp == 0,
				 MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0) {
		dev_err(scp->dev, "pwr ack timeout %s\n", genpd->name);
		goto out;
	}

	scpsys_clk_disable(scpd->clk, MAX_CLKS);

	scpsys_regulator_disable(scpd);

	return 0;

out:
	dev_err(scp->dev, "Failed to power off domain %s\n", genpd->name);

	return ret;
}

static int init_subsys_clks(struct platform_device *pdev,
		const char *prefix, struct clk **clk)
{
	struct device_node *node = pdev->dev.of_node;
	u32 prefix_len, sub_clk_cnt = 0;
	int str_sz, clk_idx, ret;

	if (!node) {
		dev_err(&pdev->dev, "Cannot find scpsys node: %ld\n",
			PTR_ERR(node));
		return PTR_ERR(node);
	}

	str_sz = of_property_count_strings(node, "clock-names");
	if (str_sz < 0) {
		dev_err(&pdev->dev, "Cannot get any subsys strings: %d\n",
				str_sz);
		return str_sz;
	}

	prefix_len = strlen(prefix);

	for (clk_idx = 0; clk_idx < str_sz; clk_idx++) {
		const char *clk_name;

		ret = of_property_read_string_index(node, "clock-names",
					clk_idx, &clk_name);
		if (ret < 0) {
			dev_err(&pdev->dev,
					"Cannot read subsys string[%d]: %d\n",
					clk_idx, ret);
			return ret;
		}

		if (!strncmp(clk_name, prefix, prefix_len) &&
				(clk_name[prefix_len] == '-')) {
			if (sub_clk_cnt >= MAX_SUBSYS_CLKS) {
				dev_err(&pdev->dev,
					"subsys clk out of range %d\n",
					sub_clk_cnt);
				return -ENOMEM;
			}

			clk[sub_clk_cnt] = devm_clk_get(&pdev->dev,
						clk_name);

			if (IS_ERR(clk[sub_clk_cnt])) {
				dev_err(&pdev->dev,
					"Subsys clk read fail %ld\n",
					PTR_ERR(clk[sub_clk_cnt]));
				return PTR_ERR(clk[sub_clk_cnt]);
			}
			sub_clk_cnt++;
		}
	}

	return sub_clk_cnt;
}

static bool scpsys_active_wakeup(struct device *dev)
{
	struct generic_pm_domain *genpd;
	struct scp_domain *scpd;

	genpd = pd_to_genpd(dev->pm_domain);
	scpd = container_of(genpd, struct scp_domain, genpd);

	if (MTK_SCPD_CAPS(scpd, MTK_SCPD_ACTIVE_WAKEUP))
		return true;
	else
		return false;
}

static void init_clks(struct platform_device *pdev, struct clk **clk)
{
	int i;

	for (i = CLK_NONE + 1; i < CLK_MAX; i++)
		clk[i] = devm_clk_get(&pdev->dev, clk_names[i]);
}

static struct scp *init_scp(struct platform_device *pdev,
			const struct scp_domain_data *scp_domain_data, int num,
			const struct scp_ctrl_reg *scp_ctrl_reg,
			bool bus_prot_reg_update)
{
	struct genpd_onecell_data *pd_data;
	struct resource *res;
	int i, j;
	struct scp *scp;
	struct clk *clk[CLK_MAX];

	scp = devm_kzalloc(&pdev->dev, sizeof(*scp), GFP_KERNEL);
	if (!scp)
		return ERR_PTR(-ENOMEM);

	scp->ctrl_reg.pwr_sta_offs = scp_ctrl_reg->pwr_sta_offs;
	scp->ctrl_reg.pwr_sta2nd_offs = scp_ctrl_reg->pwr_sta2nd_offs;

	scp->bus_prot_reg_update = bus_prot_reg_update;

	scp->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scp->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(scp->base))
		return ERR_CAST(scp->base);

	scp->domains = devm_kcalloc(&pdev->dev,
				num, sizeof(*scp->domains), GFP_KERNEL);
	if (!scp->domains)
		return ERR_PTR(-ENOMEM);

	pd_data = &scp->pd_data;

	pd_data->domains = devm_kcalloc(&pdev->dev,
			num, sizeof(*pd_data->domains), GFP_KERNEL);
	if (!pd_data->domains)
		return ERR_PTR(-ENOMEM);

	scp->infracfg = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"infracfg");
	if (IS_ERR(scp->infracfg)) {
		dev_err(&pdev->dev, "Cannot find infracfg controller: %ld\n",
				PTR_ERR(scp->infracfg));
		return ERR_CAST(scp->infracfg);
	}

	scp->smi_common = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"smi_comm");

	if (scp->smi_common == ERR_PTR(-ENODEV)) {
		scp->smi_common = NULL;
	} else if (IS_ERR(scp->smi_common)) {
		dev_err(&pdev->dev, "Cannot find smi_common controller: %ld\n",
				PTR_ERR(scp->smi_common));
		return ERR_CAST(scp->smi_common);
	}

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		const struct scp_domain_data *data = &scp_domain_data[i];

		scpd->supply = devm_regulator_get_optional(&pdev->dev, data->name);
		if (IS_ERR(scpd->supply)) {
			if (PTR_ERR(scpd->supply) == -ENODEV)
				scpd->supply = NULL;
			else
				return ERR_CAST(scpd->supply);
		}
	}

	pd_data->num_domains = num;

	init_clks(pdev, clk);

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;
		const struct scp_domain_data *data = &scp_domain_data[i];
		int clk_cnt;

		pd_data->domains[i] = genpd;
		scpd->scp = scp;

		scpd->data = data;

		if (data->clk_id[0]) {
			for (j = 0; j < MAX_CLKS && data->clk_id[j]; j++) {
				struct clk *c = clk[data->clk_id[j]];

				if (IS_ERR(c)) {
					dev_err(&pdev->dev,
						"%s: clk unavailable\n",
						data->name);
					return ERR_CAST(c);
				}

				scpd->clk[j] = c;
			}
		} else if (data->basic_clk_name[0]) {
			int ret;

			for (j = 0; j < MAX_CLKS &&
					data->basic_clk_name[j]; j++) {
				scpd->clk[j] = devm_clk_get(&pdev->dev,
						data->basic_clk_name[j]);
				if (IS_ERR(scpd->clk[j])) {
					ret = PTR_ERR(scpd->clk[j]);
					dev_err(&pdev->dev,
						"%s: Unable to get clk(%d)\n",
						__func__, ret);
					return ERR_PTR(ret);
				}
			}
		}

		if (data->subsys_clk_prefix) {
			clk_cnt = init_subsys_clks(pdev,
					data->subsys_clk_prefix,
					scpd->subsys_clk);
			if (clk_cnt < 0) {
				dev_err(&pdev->dev,
					"%s: subsys clk unavailable\n",
					data->name);
				return ERR_PTR(clk_cnt);
			}
		}

		genpd->name = data->name;
		genpd->power_off = scpsys_power_off;
		genpd->power_on = scpsys_power_on;
		genpd->dev_ops.active_wakeup = scpsys_active_wakeup;
	}

	return scp;
}

static void mtk_register_power_domains(struct platform_device *pdev,
				struct scp *scp, int num)
{
	struct genpd_onecell_data *pd_data;
	int i, ret;

	for (i = 0; i < num; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *genpd = &scpd->genpd;

		if (MTK_SCPD_CAPS(scpd, MTK_SCPD_KEEP_DEFAULT_OFF)) {
			if (scpsys_domain_is_on(scpd)) {
				/* In order to balance the reference count */
				genpd->power_on(genpd);
				genpd->power_off(genpd);
			}
			pm_genpd_init(genpd, NULL, true);
		} else {
		/*
		 * Initially turn on all domains to make the domains usable
		 * with !CONFIG_PM and to get the hardware in sync with the
		 * software.  The unused domains will be switched off during
		 * late_init time.
		 */
			genpd->power_on(genpd);

			pm_genpd_init(genpd, NULL, false);
		}
	}

	/*
	 * We are not allowed to fail here since there is no way to unregister
	 * a power domain. Once registered above we have to keep the domains
	 * valid.
	 */

	pd_data = &scp->pd_data;

	ret = of_genpd_add_provider_onecell(pdev->dev.of_node, pd_data);
	if (ret)
		dev_err(&pdev->dev, "Failed to add OF provider: %d\n", ret);
}

#ifdef CONFIG_COMMON_CLK_MT2701
/*
 * MT2701 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt2701[] = {
	[MT2701_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = SPM_CONN_PWR_CON,
		.bus_prot_mask = 0x0104,
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.clk_id = {CLK_MM},
		.bus_prot_mask = 0x0002,
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MFG},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_BDP] = {
		.name = "bdp",
		.sta_mask = PWR_STATUS_BDP,
		.ctl_offs = SPM_BDP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_ETH] = {
		.name = "eth",
		.sta_mask = PWR_STATUS_ETH,
		.ctl_offs = SPM_ETH_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_ETHIF},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_HIF] = {
		.name = "hif",
		.sta_mask = PWR_STATUS_HIF,
		.ctl_offs = SPM_HIF_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_ETHIF},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2701_POWER_DOMAIN_IFR_MSC] = {
		.name = "ifr_msc",
		.sta_mask = PWR_STATUS_IFR_MSC,
		.ctl_offs = SPM_IFR_MSC_PWR_CON,
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};
#endif

/*
 * MT2712 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt2712[] = {
	[MT2712_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = PWR_STATUS_VENC,
		.ctl_offs = SPM_VEN_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_MM, CLK_VENC, CLK_JPGDEC},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = PWR_STATUS_AUDIO,
		.ctl_offs = SPM_AUDIO_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_AUDIO},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_USB] = {
		.name = "usb",
		.sta_mask = PWR_STATUS_USB,
		.ctl_offs = SPM_USB_PWR_CON,
		.sram_pdn_bits = GENMASK(10, 8),
		.sram_pdn_ack_bits = GENMASK(14, 12),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_USB2] = {
		.name = "usb2",
		.sta_mask = PWR_STATUS_USB2,
		.ctl_offs = SPM_USB2_PWR_CON,
		.sram_pdn_bits = GENMASK(10, 8),
		.sram_pdn_ack_bits = GENMASK(14, 12),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.clk_id = {CLK_MFG},
		.bus_prot_mask = BIT(14) | BIT(21) | BIT(23),
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC1] = {
		.name = "mfg_sc1",
		.sta_mask = BIT(22),
		.ctl_offs = 0x02c0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC2] = {
		.name = "mfg_sc2",
		.sta_mask = BIT(23),
		.ctl_offs = 0x02c4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT2712_POWER_DOMAIN_MFG_SC3] = {
		.name = "mfg_sc3",
		.sta_mask = BIT(30),
		.ctl_offs = 0x01f8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(16, 16),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

static const struct scp_subdomain scp_subdomain_mt2712[] = {
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_VDEC},
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_VENC},
	{MT2712_POWER_DOMAIN_MM, MT2712_POWER_DOMAIN_ISP},
	{MT2712_POWER_DOMAIN_MFG, MT2712_POWER_DOMAIN_MFG_SC1},
	{MT2712_POWER_DOMAIN_MFG_SC1, MT2712_POWER_DOMAIN_MFG_SC2},
	{MT2712_POWER_DOMAIN_MFG_SC2, MT2712_POWER_DOMAIN_MFG_SC3},
};

/*
 * MT8173 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt8173[] = {
	[MT8173_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MM},
	},
	[MT8173_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = PWR_STATUS_VENC,
		.ctl_offs = SPM_VEN_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_MM, CLK_VENC},
	},
	[MT8173_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.clk_id = {CLK_MM},
	},
	[MT8173_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_id = {CLK_MM},
		.bus_prot_mask = MT8173_TOP_AXI_PROT_EN_MM_M0 |
			MT8173_TOP_AXI_PROT_EN_MM_M1,
	},
	[MT8173_POWER_DOMAIN_VENC_LT] = {
		.name = "venc_lt",
		.sta_mask = PWR_STATUS_VENC_LT,
		.ctl_offs = SPM_VEN2_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_MM, CLK_VENC_LT},
	},
	[MT8173_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = PWR_STATUS_AUDIO,
		.ctl_offs = SPM_AUDIO_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_NONE},
	},
	[MT8173_POWER_DOMAIN_USB] = {
		.name = "usb",
		.sta_mask = PWR_STATUS_USB,
		.ctl_offs = SPM_USB_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT8173_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = PWR_STATUS_MFG_ASYNC,
		.ctl_offs = SPM_MFG_ASYNC_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0,
		.clk_id = {CLK_MFG},
	},
	[MT8173_POWER_DOMAIN_MFG_2D] = {
		.name = "mfg_2d",
		.sta_mask = PWR_STATUS_MFG_2D,
		.ctl_offs = SPM_MFG_2D_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.clk_id = {CLK_NONE},
	},
	[MT8173_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(21, 16),
		.clk_id = {CLK_NONE},
		.bus_prot_mask = MT8173_TOP_AXI_PROT_EN_MFG_S |
			MT8173_TOP_AXI_PROT_EN_MFG_M0 |
			MT8173_TOP_AXI_PROT_EN_MFG_M1 |
			MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT,
	},
};

static const struct scp_subdomain scp_subdomain_mt8173[] = {
	{MT8173_POWER_DOMAIN_MFG_ASYNC, MT8173_POWER_DOMAIN_MFG_2D},
	{MT8173_POWER_DOMAIN_MFG_2D, MT8173_POWER_DOMAIN_MFG},
};

/*
 * MT8512 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt8512[] = {
	[MT8512_POWER_DOMAIN_MM] = {
		.name = "mm",
		.sta_mask = BIT(15),
		.ctl_offs = 0x374,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.subsys_clk_prefix = "mm",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(16) | BIT(17), BIT(16) | BIT(17)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(8) | BIT(9) | BIT(10) | BIT(11),
				BIT(8) | BIT(9) | BIT(10) | BIT(11)),
		},
	},
	[MT8512_POWER_DOMAIN_IMG] = {
		.name = "img",
		.sta_mask = BIT(16),
		.ctl_offs = 0x38C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
	},
	[MT8512_POWER_DOMAIN_IP0] = {
		.name = "ip0",
		.sta_mask = BIT(25),
		.ctl_offs = 0x39C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
		.subsys_clk_prefix = "ip0",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(12), BIT(12)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(5) | BIT(6) | BIT(7),
				BIT(5) | BIT(6) | BIT(7)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(14) | BIT(16) | BIT(18),
				BIT(14) | BIT(16) | BIT(18)),
		},
	},
	[MT8512_POWER_DOMAIN_IP1] = {
		.name = "ip1",
		.sta_mask = BIT(26),
		.ctl_offs = 0x384,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
		.subsys_clk_prefix = "ip1",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(4), BIT(4)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(2), BIT(2)),
		},
	},
	[MT8512_POWER_DOMAIN_IP2] = {
		.name = "ip2",
		.sta_mask = BIT(27),
		.ctl_offs = 0x388,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.caps = MTK_SCPD_KEEP_DEFAULT_OFF,
		.subsys_clk_prefix = "ip2",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(5), BIT(5)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(3), BIT(3)),
		},
	},
	[MT8512_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x32C,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.basic_clk_name = {"conn", "conn1"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_KEEP_DEFAULT_OFF,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(13), BIT(13)),
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(18), BIT(18)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(14), BIT(14)),
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(21), BIT(21)),
		},
	},
	[MT8512_POWER_DOMAIN_DSP] = {
		.name = "dsp",
		.sta_mask = BIT(17),
		.ctl_offs = 0x37C,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"dsp", "dsp1"},
		.subsys_clk_prefix = "dsp",
		.caps = MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_KEEP_DEFAULT_OFF,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(7) | BIT(10) | BIT(11),
				BIT(7) | BIT(10) | BIT(11)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(1) | BIT(24), BIT(1) | BIT(24)),
		},
	},
	[MT8512_POWER_DOMAIN_AUDAFE] = {
		.name = "audafe",
		.sta_mask = BIT(24),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(16, 13),
		.basic_clk_name = {"audafe"},
		.subsys_clk_prefix = "audafe",
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(28), BIT(28)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(22), BIT(22)),
		},
	},
	[MT8512_POWER_DOMAIN_AUDSRC] = {
		.name = "audsrc",
		.sta_mask = BIT(23),
		.ctl_offs = 0x328,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audsrc"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP | MTK_SCPD_KEEP_DEFAULT_OFF,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(21), BIT(21)),
		},
	},
	[MT8512_POWER_DOMAIN_USB] = {
		.name = "usb",
		.sta_mask = BIT(20),
		.ctl_offs = 0x3A4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"usb"},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(0) | BIT(1), BIT(0) | BIT(1)),
		},
	},
};

/*
 * MT8518 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt8518[] = {
	[MT8518_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bus_prot_mask = BIT(1),
		.clk_id = {CLK_MM},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT8518_POWER_DOMAIN_AUDAFE] = {
		.name = "audafe",
		.sta_mask = BIT(21),
		.ctl_offs = SPM_AUDAFE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = BIT(25) | BIT(26) | BIT(28),
		.clk_id = {CLK_AUDIO},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
	[MT8518_POWER_DOMAIN_AUDSRC] = {
		.name = "audsrc",
		.sta_mask = BIT(20),
		.ctl_offs = SPM_AUDSRC_PWR_CON,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(9, 9),
		.bus_prot_mask = BIT(27),
		.clk_id = {CLK_NONE},
		.caps = MTK_SCPD_ACTIVE_WAKEUP,
	},
};

#ifdef CONFIG_COMMON_CLK_MT2701
static const struct scp_soc_data mt2701_data = {
	.domains = scp_domain_data_mt2701,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt2701),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};
#endif

static const struct scp_soc_data mt2712_data = {
	.domains = scp_domain_data_mt2712,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt2712),
	.subdomains = scp_subdomain_mt2712,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt2712),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = false,
};

static const struct scp_soc_data mt8173_data = {
	.domains = scp_domain_data_mt8173,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt8173),
	.subdomains = scp_subdomain_mt8173,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt8173),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

static const struct scp_soc_data mt8512_data = {
	.domains = scp_domain_data_mt8512,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt8512),
	.regs = {
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184
	}
};

static const struct scp_soc_data mt8518_data = {
	.domains = scp_domain_data_mt8518,
	.num_domains = ARRAY_SIZE(scp_domain_data_mt8518),
	.regs = {
		.pwr_sta_offs = SPM_PWR_STATUS,
		.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND
	},
	.bus_prot_reg_update = true,
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
#ifdef CONFIG_COMMON_CLK_MT2701
	{
		.compatible = "mediatek,mt2701-scpsys",
		.data = &mt2701_data,
	},
#endif
	{
		.compatible = "mediatek,mt2712-scpsys",
		.data = &mt2712_data,
	}, {
		.compatible = "mediatek,mt8173-scpsys",
		.data = &mt8173_data,
	}, {
		.compatible = "mediatek,mt8512-scpsys",
		.data = &mt8512_data,
	}, {
		.compatible = "mediatek,mt8518-scpsys",
		.data = &mt8518_data,
	}, {
		/* sentinel */
	}
};

static int scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs,
			soc->bus_prot_reg_update);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	mtk_register_power_domains(pdev, scp, soc->num_domains);

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM))
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
	}

	return 0;
}

static struct platform_driver scpsys_drv = {
	.probe = scpsys_probe,
	.driver = {
		.name = "mtk-scpsys",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};
builtin_platform_driver(scpsys_drv);
