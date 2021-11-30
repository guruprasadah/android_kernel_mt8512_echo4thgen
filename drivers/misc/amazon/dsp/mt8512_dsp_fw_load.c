/*******************************************************************************
 * DSP HAL device driver board-specific functions
 *
 * Copyright 2020 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/gpl-2.0.html>.
 *
 *******************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/printk.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/errno.h>
#include <adsp_ipi.h>

#include "mach/mtk_hifixdsp_common.h"

#include "amzn_dsp_fw_load.h"

#define MT8512_DSP_FW_LOAD_DEVICE "mt8512_dsp_fw_load"

int max_pll;
struct pm_qos_request pm_mt8512_dsp_fw_load;

static int g_dsp_fw_load_status = DSP_FW_LOAD_UNLOADED;

#define IPI_MSG_CNTL_MAGIC_NUMBER 7891

enum cntl_ipi_mode {
	SHUTDOWN_MODE_IPI = 0,
	STATUS_MODE_IPI,
};

struct cntl_ipi_msg_t {
	uint16_t magic;
	uint16_t cntl_mode;
	uint8_t write_buffer[4];
	uint8_t read_buffer[4];
	void *data;
};

struct completion msg_complete;

static int check_msg_format(const struct cntl_ipi_msg_t *ipi_msg,
				unsigned int len)
{
	if (ipi_msg->magic != IPI_MSG_CNTL_MAGIC_NUMBER) {
		pr_notice("magic 0x%x error!!", ipi_msg->magic);
		return -1;
	}

	if (sizeof(struct cntl_ipi_msg_t) > len) {
		pr_notice("len 0x%x error!!", len);
		return -1;
	}
	return 0;
}

static void ipi_callback(int id, void *data, unsigned int len)
{
	struct cntl_ipi_msg_t *ipi_msg = NULL;

	if (!data) {
		pr_info("%s: Invalid data\n", __func__);
		return;
	}

	ipi_msg = (struct cntl_ipi_msg_t *)data;
	if (check_msg_format(ipi_msg, len)) {
		pr_info("%s: Invalid format\n", __func__);
		return;
	}

	if (ipi_msg->cntl_mode == SHUTDOWN_MODE_IPI)
		complete(&msg_complete);
}

int fw_send_ipi_msg(enum cntl_ipi_mode mode)
{
	struct cntl_ipi_msg_t *ipi_msg;
	uint32_t ipi_msg_len = 0;
	int err = -1;

	ipi_msg  = kzalloc(sizeof(struct cntl_ipi_msg_t), GFP_KERNEL);
	if (ipi_msg == NULL)
		return -ENOMEM;

	ipi_msg->magic = IPI_MSG_CNTL_MAGIC_NUMBER;
	ipi_msg->cntl_mode = mode;
	ipi_msg_len = sizeof(struct cntl_ipi_msg_t);

	if (check_msg_format(ipi_msg, ipi_msg_len) != 0) {
		pr_err("[%s], drop msg due to ipi fmt err", __func__);
		return -1;
	}
	err = adsp_ipi_send(ADSP_IPI_CONTROL, ipi_msg, ipi_msg_len, true, 0);

	if (err == 0) {
		pr_info("[%s], set msg ok\n", __func__);
	} else {
		pr_err("[%s], gpio_ipi_send error %d",
				  __func__, err);
		return err;
	}

	if (ipi_msg->cntl_mode == SHUTDOWN_MODE_IPI) {
		err = wait_for_completion_timeout(&msg_complete,
					 msecs_to_jiffies(2000));
		if (err > 0) {
			err = 0;
		} else {
			pr_err("%s: dsp wait timed out!\n", __func__);
			err = -EAGAIN;
		}
	}

	return err;
}

amzn_dsp_fw_load_t amzn_dsp_fw_load_status(void)
{
	return g_dsp_fw_load_status;
}

EXPORT_SYMBOL(amzn_dsp_fw_load_status);

static int mt8512_adsp_low_power_init(void)
{

	if (max_pll == 400000000)
		pm_qos_update_request(&pm_mt8512_dsp_fw_load, 1); /* VCORE_OPP_1 */
	else if (max_pll == 600000000)
		pm_qos_update_request(&pm_mt8512_dsp_fw_load, 0); /* VCORE_OPP_0 */

	return 0;
}

static int mt8512_adsp_low_power_uninit(void)
{
	pm_qos_update_request(&pm_mt8512_dsp_fw_load, PM_QOS_VCORE_OPP_DEFAULT_VALUE);

	return 0;
}

static void load_hifi4dsp_callback(void *arg)
{
	if (!hifixdsp_run_status()) {
		pr_err("DSP FW load error!\n");
		g_dsp_fw_load_status = DSP_FW_LOAD_ERROR;
		return;
	}

	mt8512_adsp_low_power_init();
	g_dsp_fw_load_status = DSP_FW_LOAD_COMPLETE;

	pr_info("DSP FW Load Complete\n");
}

static ssize_t mt8512_dsp_fw_reload(struct device *dev, struct device_attribute *attr, char *buf)
{
	int err;

	err = hifixdsp_stop_run();
	if (err) {
		pr_err("%s: Stop DSP FW fail: %d\n", __func__, err);
		return err;
	}

	g_dsp_fw_load_status = DSP_FW_LOAD_UNLOADED;

	err = async_load_hifixdsp_bin_and_run(load_hifi4dsp_callback, NULL);
	if (err != 0) {
		pr_err("%s: Load DSP FW fail: %d\n", __func__, err);
		g_dsp_fw_load_status = DSP_FW_LOAD_ERROR;
		return err;
	}

	g_dsp_fw_load_status = DSP_FW_LOAD_IN_PROCESS;

	return 0;
}

static DEVICE_ATTR(reload, S_IRUSR, mt8512_dsp_fw_reload, NULL);

static struct clk *clk_dma;

static int configure_clks(struct platform_device *pdev)
{
	int ret = 0;

	clk_dma = devm_clk_get(&pdev->dev, "dma");
	if (IS_ERR(clk_dma)) {
		dev_err(&pdev->dev, "cannot get infra AP dma clock\n");
	} else {
	/* Leave DMA clks enabled so CCF won't turn it off ever */
		ret = clk_prepare_enable(clk_dma);
		if (ret) {
			dev_err(&pdev->dev, "DMA clock enable failed!\n");
			return ret;
		}
	}

	return ret;
}

static int mt8512_dsp_fw_load_probe(struct platform_device *pdev)
{
	int32_t err;
	struct device_node *dt = pdev->dev.of_node;

	pm_qos_add_request(&pm_mt8512_dsp_fw_load,
	                   PM_QOS_VCORE_OPP, PM_QOS_VCORE_OPP_DEFAULT_VALUE);


	if (of_find_property(dt, "clocks", NULL)) {
		err = configure_clks(pdev);
		if (err)
			return err;
	}

	/* Start async FW load */
	err = async_load_hifixdsp_bin_and_run(load_hifi4dsp_callback, NULL);
	if (err != 0) {
		pr_err("%s: Load DSP FW fail: %d\n", __func__, err);
		g_dsp_fw_load_status = DSP_FW_LOAD_ERROR;
		return err;
	}

	err = device_create_file(&pdev->dev, &dev_attr_reload);
	if (err != 0) {
		pr_err("%s: Failed to create sysfs entry: %d\n", __func__, err);
		return err;
	}

	g_dsp_fw_load_status = DSP_FW_LOAD_IN_PROCESS;

	err = adsp_ipi_registration(ADSP_IPI_CONTROL,
			(ipi_handler_t)ipi_callback, "cntl_ipi");
	if (err) {
		pr_err("Failed to register cntl ipi : %d\n", err);
	}

	return 0;
}

static void mt8512_dsp_fw_load_shutdown(struct platform_device *pdev)
{
	int32_t err;
	bool skip_ipi = false;

	if (g_dsp_fw_load_status != DSP_FW_LOAD_COMPLETE)
		skip_ipi = true;

	g_dsp_fw_load_status = DSP_FW_LOAD_UNLOADED;

	device_remove_file(&pdev->dev, &dev_attr_reload);

	if (!skip_ipi) {
		init_completion(&msg_complete);
		err = fw_send_ipi_msg(SHUTDOWN_MODE_IPI);
		if (err) {
			pr_err("Shutdown DSP fail: %d\n", err);
			return;
		}
	}

	err = hifixdsp_stop_run();
	if (err) {
		pr_err("Stop DSP fail: %d\n", err);
		return;
	}

	if (clk_dma)
		clk_disable_unprepare(clk_dma);

	mt8512_adsp_low_power_uninit();

	pm_qos_remove_request(&pm_mt8512_dsp_fw_load);
}

static const struct of_device_id mt8512_dsp_fw_load_of_ids[] = {
	{ .compatible = "amazon,dsp_fw_load", },
	{}
};

static struct platform_driver mt8512_dsp_fw_load_driver = {
	.probe = mt8512_dsp_fw_load_probe,
	.shutdown = mt8512_dsp_fw_load_shutdown,
	.driver = {
		.name = MT8512_DSP_FW_LOAD_DEVICE,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt8512_dsp_fw_load_of_ids,
#endif
	},
};

static int mt8512_dsp_fw_load_mod_init(void)
{
	int32_t ret;

	ret = platform_driver_register(&mt8512_dsp_fw_load_driver);
	if (ret) {
		pr_err("Unable to register driver (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void mt8512_dsp_fw_load_mod_exit(void)
{
	platform_driver_unregister(&mt8512_dsp_fw_load_driver);
}

module_init(mt8512_dsp_fw_load_mod_init);
module_exit(mt8512_dsp_fw_load_mod_exit);

MODULE_DESCRIPTION("Amazon MT8512 DSP FW Load driver");
MODULE_LICENSE("GPL");
