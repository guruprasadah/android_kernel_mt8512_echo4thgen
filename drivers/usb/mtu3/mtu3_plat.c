/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>

#include "mtu3.h"
#include "mtu3_dr.h"

void ssusb_set_force_vbus(struct ssusb_mtk *ssusb, bool vbus_on)
{
	u32 u2ctl;
	u32 misc;

	if (!ssusb->force_vbus)
		return;

	u2ctl = mtu3_readl(ssusb->ippc_base, SSUSB_U2_CTRL(0));
	misc = mtu3_readl(ssusb->mac_base, U3D_MISC_CTRL);
	if (vbus_on) {
		u2ctl &= ~SSUSB_U2_PORT_OTG_SEL;
		misc |= VBUS_FRC_EN | VBUS_ON;
	} else {
		u2ctl |= SSUSB_U2_PORT_OTG_SEL;
		misc &= ~(VBUS_FRC_EN | VBUS_ON);
	}
	mtu3_writel(ssusb->ippc_base, SSUSB_U2_CTRL(0), u2ctl);
	mtu3_writel(ssusb->mac_base, U3D_MISC_CTRL, misc);
}

/* u2-port0 should be powered on and enabled; */
int ssusb_check_clocks(struct ssusb_mtk *ssusb, u32 ex_clks)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value, check_val;
	int ret;

	check_val = ex_clks | SSUSB_SYS125_RST_B_STS | SSUSB_SYSPLL_STABLE |
			SSUSB_REF_RST_B_STS;

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			(check_val == (value & check_val)), 100, 20000);
	if (ret) {
		dev_err(ssusb->dev, "clks of sts1 are not stable!\n");
		return ret;
	}

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS2, value,
			(value & SSUSB_U2_MAC_SYS_RST_B_STS), 100, 10000);
	if (ret) {
		dev_err(ssusb->dev, "mac2 clock is not stable\n");
		return ret;
	}

	return 0;
}

static int ssusb_phy_init(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_init(ssusb->phys[i]);
		if (ret)
			goto exit_phy;
	}
	return 0;

exit_phy:
	for (; i > 0; i--)
		phy_exit(ssusb->phys[i - 1]);

	return ret;
}

static int ssusb_phy_exit(struct ssusb_mtk *ssusb)
{
	int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_exit(ssusb->phys[i]);

	return 0;
}

int ssusb_phy_power_on(struct ssusb_mtk *ssusb)
{
	int i;
	int ret;

	for (i = 0; i < ssusb->num_phys; i++) {
		ret = phy_power_on(ssusb->phys[i]);
		if (ret)
			goto power_off_phy;
	}
	return 0;

power_off_phy:
	for (; i > 0; i--)
		phy_power_off(ssusb->phys[i - 1]);

	return ret;
}

void ssusb_phy_power_off(struct ssusb_mtk *ssusb)
{
	unsigned int i;

	for (i = 0; i < ssusb->num_phys; i++)
		phy_power_off(ssusb->phys[i]);
}

int ssusb_clks_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	pm_runtime_get_sync(ssusb->dev);

	ret = clk_prepare_enable(ssusb->sys_clk);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable sys_clk\n");
		goto sys_clk_err;
	}

	ret = clk_prepare_enable(ssusb->ref_clk);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable ref_clk\n");
		goto ref_clk_err;
	}

	ret = clk_prepare_enable(ssusb->mcu_clk);
	if (ret) {
		dev_info(ssusb->dev, "failed to enable mcu_clk\n");
		goto mcu_clk_err;
	}

	ret = clk_prepare_enable(ssusb->xhci_clk);
	if (ret) {
		dev_info(ssusb->dev, "failed to enable xhci_clk\n");
		goto xhci_clk_err;
	}
	return 0;

xhci_clk_err:
	clk_disable_unprepare(ssusb->mcu_clk);
mcu_clk_err:
	clk_disable_unprepare(ssusb->ref_clk);
ref_clk_err:
	clk_disable_unprepare(ssusb->sys_clk);
sys_clk_err:
	pm_runtime_put_sync(ssusb->dev);
	return ret;
}

void ssusb_clks_disable(struct ssusb_mtk *ssusb)
{
	clk_disable_unprepare(ssusb->xhci_clk);
	clk_disable_unprepare(ssusb->mcu_clk);
	clk_disable_unprepare(ssusb->ref_clk);
	clk_disable_unprepare(ssusb->sys_clk);
	pm_runtime_put_sync(ssusb->dev);
}


static int ssusb_rscs_init(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	ret = regulator_enable(ssusb->vusb33);
	if (ret) {
		dev_info(ssusb->dev, "failed to enable vusb33\n");
		goto vusb33_err;
	}

	ret = ssusb_clks_enable(ssusb);
	if (ret)
		goto clks_err;


	ret = ssusb_phy_init(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to init phy\n");
		goto phy_init_err;
	}

	ret = ssusb_phy_power_on(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to power on phy\n");
		goto phy_err;
	}

	return 0;

phy_err:
	ssusb_phy_exit(ssusb);
phy_init_err:
	ssusb_clks_disable(ssusb);
clks_err:
	regulator_disable(ssusb->vusb33);
vusb33_err:

	return ret;
}

static void ssusb_rscs_exit(struct ssusb_mtk *ssusb)
{
	ssusb_clks_disable(ssusb);
	regulator_disable(ssusb->vusb33);
	ssusb_phy_power_off(ssusb);
	ssusb_phy_exit(ssusb);
}

static void ssusb_ip_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & u3d) */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}

static int get_ssusb_rscs(struct platform_device *pdev, struct ssusb_mtk *ssusb)
{
	struct device_node *node = pdev->dev.of_node;
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	struct device *dev = &pdev->dev;
	struct regulator *vbus;
	struct resource *res;
	int i;
	int ret;

	ssusb->vusb33 = devm_regulator_get(&pdev->dev, "vusb33");
	if (IS_ERR(ssusb->vusb33)) {
		dev_err(dev, "failed to get vusb33\n");
		return PTR_ERR(ssusb->vusb33);
	}

	ssusb->sys_clk = devm_clk_get(dev, "sys_ck");
	if (IS_ERR(ssusb->sys_clk)) {
		dev_err(dev, "failed to get sys clock\n");
		return PTR_ERR(ssusb->sys_clk);
	}

	/*
	 * reference clock is usually a "fixed-clock", make it optional
	 * for backward compatibility and ignore the error if it does
	 * not exist.
	 */
	ssusb->ref_clk = devm_clk_get(dev, "ref_ck");
	if (IS_ERR(ssusb->ref_clk))
		return PTR_ERR(ssusb->ref_clk);

	ssusb->mcu_clk = devm_clk_get(dev, "mcu_ck");
	if (IS_ERR(ssusb->mcu_clk))
		return PTR_ERR(ssusb->mcu_clk);
	ssusb->xhci_clk = devm_clk_get(dev, "xhci_ck");
	if (IS_ERR(ssusb->xhci_clk))
		return PTR_ERR(ssusb->xhci_clk);

	ssusb->num_phys = of_count_phandle_with_args(node,
			"phys", "#phy-cells");
	if (ssusb->num_phys > 0) {
		ssusb->phys = devm_kcalloc(dev, ssusb->num_phys,
					sizeof(*ssusb->phys), GFP_KERNEL);
		if (!ssusb->phys)
			return -ENOMEM;
	} else {
		ssusb->num_phys = 0;
	}

	for (i = 0; i < ssusb->num_phys; i++) {
		ssusb->phys[i] = devm_of_phy_get_by_index(dev, node, i);
		if (IS_ERR(ssusb->phys[i])) {
			dev_err(dev, "failed to get phy-%d\n", i);
			return PTR_ERR(ssusb->phys[i]);
		}
	}

	if (ssusb->num_phys > 1)
		ssusb->keep_ao = true;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ippc");
	ssusb->ippc_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ssusb->ippc_base)) {
		dev_err(dev, "failed to map memory for ippc\n");
		return PTR_ERR(ssusb->ippc_base);
	}

	ssusb->dr_mode = usb_get_dr_mode(dev);
	if (ssusb->dr_mode == USB_DR_MODE_UNKNOWN) {
		dev_err(dev, "dr_mode is error\n");
		return -EINVAL;
	}

	if (ssusb->dr_mode == USB_DR_MODE_PERIPHERAL)
		return 0;

	/* if host role is supported */
	ret = ssusb_wakeup_of_property_parse(ssusb, node);
	if (ret)
		return ret;

	ssusb->u3ports_disable =
		of_property_read_bool(node, "mediatek,u3ports-disable");

	if (ssusb->dr_mode != USB_DR_MODE_OTG)
		return 0;

	/* if dual-role mode is supported */
	vbus = devm_regulator_get(&pdev->dev, "vbus");
	if (IS_ERR(vbus)) {
		dev_err(dev, "failed to get vbus\n");
		return PTR_ERR(vbus);
	}
	otg_sx->vbus = vbus;

	if (ssusb->dr_mode == USB_DR_MODE_HOST)
		return 0;
	otg_sx->is_u3_drd = of_property_read_bool(node, "mediatek,usb3-drd");
	otg_sx->manual_drd_enabled =
		of_property_read_bool(node, "enable-manual-drd");

	if (otg_sx->manual_drd_enabled)
		ssusb->is_host = of_property_read_bool(node, "manual-default-host");

	if (otg_sx->manual_drd_enabled)
		ssusb->keep_ao = true;
	if (of_property_read_bool(node, "extcon")) {
		otg_sx->edev = extcon_get_edev_by_phandle(ssusb->dev, 0);
		if (IS_ERR(otg_sx->edev)) {
			dev_err(ssusb->dev, "couldn't get extcon device\n");
			return -EPROBE_DEFER;
		}
	}

	dev_info(dev, "dr_mode: %d, is_u3_dr: %d keep-ao: %d\n",
		ssusb->dr_mode, otg_sx->is_u3_drd, ssusb->keep_ao);

	return 0;
}

static int mtu3_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct ssusb_mtk *ssusb;
	struct otg_switch_mtk *otg_sx;
	int ret = -ENOMEM;

	/* all elements are set to ZERO as default value */
	ssusb = devm_kzalloc(dev, sizeof(*ssusb), GFP_KERNEL);
	if (!ssusb)
		return -ENOMEM;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "No suitable DMA config available\n");
		return -ENOTSUPP;
	}

	platform_set_drvdata(pdev, ssusb);
	ssusb->dev = dev;

	ret = get_ssusb_rscs(pdev, ssusb);
	if (ret)
		return ret;

	/* enable power domain */
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	device_enable_async_suspend(dev);

	ret = ssusb_rscs_init(ssusb);
	if (ret)
		goto comm_init_err;

	ssusb_ip_sw_reset(ssusb);

	if (IS_ENABLED(CONFIG_USB_MTU3_HOST))
		ssusb->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_MTU3_GADGET))
		ssusb->dr_mode = USB_DR_MODE_PERIPHERAL;

	otg_sx = &ssusb->otg_switch;
	/* default as idle */
	if (!otg_sx->manual_drd_enabled)
		ssusb->is_host = false;

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_HOST:
		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_OTG:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}

		if (ssusb->keep_ao) {
		ret = ssusb_host_init(ssusb, node);
		if (ret) {
			dev_info(dev, "failed to initialize host\n");
			goto gadget_exit;
			}
		}

		if (!ssusb->keep_ao) {
			ssusb_clks_disable(ssusb);
			ssusb_phy_power_off(ssusb);
		}
		ssusb_otg_switch_init(ssusb);
		break;
	default:
		dev_err(dev, "unsupported mode: %d\n", ssusb->dr_mode);
		ret = -EINVAL;
		goto comm_exit;
	}
	if (!ssusb->keep_ao)
		pm_runtime_put_sync(dev);
	return 0;

gadget_exit:
	ssusb_gadget_exit(ssusb);
comm_exit:
	ssusb_rscs_exit(ssusb);
comm_init_err:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	return ret;
}

static int mtu3_remove(struct platform_device *pdev)
{
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		ssusb_gadget_exit(ssusb);
		break;
	case USB_DR_MODE_HOST:
		ssusb_host_exit(ssusb);
		break;
	case USB_DR_MODE_OTG:
		ssusb_otg_switch_exit(ssusb);
		ssusb_gadget_exit(ssusb);
		ssusb_host_exit(ssusb);
		break;
	default:
		return -EINVAL;
	}

	ssusb_rscs_exit(ssusb);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

/*
 * when support dual-role mode, we reject suspend when
 * it works as device mode;
 */
static int __maybe_unused mtu3_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	/* REVISIT: disconnect it for only device mode? */
	if (!ssusb->is_host)
		return 0;

	ssusb_host_disable(ssusb, true);
	ssusb_phy_power_off(ssusb);
	clk_disable_unprepare(ssusb->sys_clk);
	clk_disable_unprepare(ssusb->ref_clk);
	clk_disable_unprepare(ssusb->mcu_clk);
	ssusb_wakeup_enable(ssusb);

	return 0;
}

static int __maybe_unused mtu3_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);

	if (!ssusb->is_host)
		return 0;

	ssusb_wakeup_disable(ssusb);
	clk_prepare_enable(ssusb->sys_clk);
	clk_prepare_enable(ssusb->ref_clk);
	clk_prepare_enable(ssusb->mcu_clk);
	ssusb_phy_power_on(ssusb);
	ssusb_host_enable(ssusb);

	return 0;
}
#if 0
static int __maybe_unused mtu3_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_err(dev, "%s\n", __func__);

	/* REVISIT: disconnect it for only device mode? */
	if (!ssusb->is_host)
		return 0;

	ssusb_host_disable(ssusb, true);
	ssusb_phy_power_off(ssusb);
	clk_disable_unprepare(ssusb->sys_clk);
	clk_disable_unprepare(ssusb->ref_clk);
	clk_disable_unprepare(ssusb->mcu_clk);
	ssusb_wakeup_enable(ssusb);

	return 0;
}

static int __maybe_unused mtu3_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ssusb_mtk *ssusb = platform_get_drvdata(pdev);

	dev_err(dev, "%s\n", __func__);

	if (!ssusb->is_host)
		return 0;

	ssusb_wakeup_disable(ssusb);
	clk_prepare_enable(ssusb->sys_clk);
	clk_prepare_enable(ssusb->ref_clk);
	clk_prepare_enable(ssusb->mcu_clk);
	ssusb_phy_power_on(ssusb);
	ssusb_host_enable(ssusb);

	return 0;
}
#endif
static const struct dev_pm_ops mtu3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtu3_suspend, mtu3_resume)
	//SET_RUNTIME_PM_OPS(mtu3_runtime_suspend, mtu3_runtime_resume, NULL)
};

#define DEV_PM_OPS (IS_ENABLED(CONFIG_PM) ? &mtu3_pm_ops : NULL)

#ifdef CONFIG_OF

static const struct of_device_id mtu3_of_match[] = {
	{.compatible = "mediatek,mt8173-mtu3",},
	{.compatible = "mediatek,mtu3",},
	{},
};

MODULE_DEVICE_TABLE(of, mtu3_of_match);

#endif

static struct platform_driver mtu3_driver = {
	.probe = mtu3_probe,
	.remove = mtu3_remove,
	.driver = {
		.name = MTU3_DRIVER_NAME,
		.pm = DEV_PM_OPS,
		.of_match_table = of_match_ptr(mtu3_of_match),
	},
};
module_platform_driver(mtu3_driver);

MODULE_AUTHOR("Chunfeng Yun <chunfeng.yun@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek USB3 DRD Controller Driver");
