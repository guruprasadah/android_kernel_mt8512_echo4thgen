/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.
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

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <adsp_ipi.h>
#include <amzn_dsp_fw_load.h>

#define I2C_DRV_NAME			"adsp-i2c"
#define I2C_DEFAULT_SPEED		100000	/* Hz */
#define IPI_MSG_I2C_MAGIC_NUMBER	0x9321
#define I2C_BUS				2
#define I2C_BUF_SIZE  I2C_SMBUS_BLOCK_MAX + 1

enum i2c_ipi_mode {
	WRITE_MODE_IPI = 0,
	READ_MODE_IPI,
	WRITE_READ_MODE_IPI,
	STATUS_MODE_IPI,
};

enum i2c_ipi_status {
	NO_RESPONSE = 0,
	READY,
	ERROR,
};

struct adsp_ipi_i2c_platdata {
	const struct i2c_adapter_quirks *quirks;
};

struct i2c_ipi_msg_t {
	uint16_t magic;
	uint16_t i2c_ipi_mode;
	uint8_t bus_num;
	uint8_t device_addr;
	uint32_t speed_hz;
	uint16_t write_len;
	uint16_t read_len;
	uint8_t write_buffer[I2C_BUF_SIZE];
	uint8_t read_buffer[I2C_BUF_SIZE];
	void *data;
};

struct adsp_ipi_i2c {
	struct i2c_adapter adap;
	struct device *dev;
	struct completion msg_complete;
	unsigned int speed_hz;
	const struct adsp_ipi_i2c_platdata *pdata;
	struct i2c_ipi_msg_t ipi_msg;
	struct mutex ipi_mutex;
	struct clk *clk_top;
	struct clk *clk_parent;
};

static const struct i2c_adapter_quirks ipi_quirks = {
	.flags = I2C_AQ_COMB_WRITE_THEN_READ,
	.max_num_msgs = 16,
	.max_write_len = I2C_BUF_SIZE,
	.max_read_len = I2C_BUF_SIZE,
	.max_comb_1st_msg_len = I2C_BUF_SIZE,
	.max_comb_2nd_msg_len = I2C_BUF_SIZE,
};

static const struct adsp_ipi_i2c_platdata ipi_pdata = {
	.quirks = &ipi_quirks,
};

static const struct of_device_id adsp_ipi_i2c_of_match[] = {
	{ .compatible = "amazon,ipi-i2c", .data = &ipi_pdata },
	{}
};

static int check_msg_format(const struct i2c_ipi_msg_t *ipi_msg,
			    unsigned int len)
{
	if (ipi_msg->magic != IPI_MSG_I2C_MAGIC_NUMBER) {
		pr_err("magic 0x%x error!!\n", ipi_msg->magic);
		return -EINVAL;
	}

	if (sizeof(struct i2c_ipi_msg_t) > len) {
		pr_err("len 0x%x error!!\n", len);
		return -EINVAL;
	}

	return 0;
}

static void ipi_callback(int id, void *data, unsigned int len)
{
	struct i2c_ipi_msg_t *ipi_msg = NULL;
	struct adsp_ipi_i2c *i2c;
	int i;

	if (!data) {
		pr_info("%s: Invalid data\n", __func__);
		return;
	}

	ipi_msg = (struct i2c_ipi_msg_t *)data;
	if (check_msg_format(ipi_msg, len)) {
		pr_info("%s: Invalid format\n", __func__);
		return;
	}

	if (ipi_msg->i2c_ipi_mode == WRITE_MODE_IPI)
		return;

	i2c = ipi_msg->data;

	for (i = 0; i < ipi_msg->read_len; i++)
		i2c->ipi_msg.read_buffer[i] = ipi_msg->read_buffer[i];

	complete(&i2c->msg_complete);
}

static int ipi_transfer(struct adsp_ipi_i2c *i2c, struct i2c_ipi_msg_t *ipi_msg )
{
	int ret;
	uint32_t ipi_msg_len = sizeof(struct i2c_ipi_msg_t);

	ret = check_msg_format(ipi_msg, ipi_msg_len);

	if (ret)
		goto out;

	if (ipi_msg->i2c_ipi_mode != WRITE_MODE_IPI)
		init_completion(&i2c->msg_complete);

	ret = adsp_ipi_send(ADSP_IPI_I2C, ipi_msg, ipi_msg_len, true, 0);
	if (ret) {
		dev_err(i2c->dev, "Invalid I2C IPI message, ret: %d\n", ret);
		ret = -EAGAIN;
		goto out;
	}

	if (ipi_msg->i2c_ipi_mode != WRITE_MODE_IPI) {
		ret = wait_for_completion_timeout(&i2c->msg_complete,
						 msecs_to_jiffies(1000));
		if (ret > 0) {
			ret = 0;
		} else {
			dev_err(i2c->dev, "%s: dsp wait timed out!\n", __func__);
			ret = -EAGAIN;
		}
	}

out:
	return ret;
}

static int adsp_ipi_i2c_transfer(struct i2c_adapter *adap,
			    struct i2c_msg msgs[], int num)
{
	int ret;
	int left_num = num;
	struct adsp_ipi_i2c *i2c = i2c_get_adapdata(adap);
	struct i2c_ipi_msg_t *ipi_msg = &i2c->ipi_msg;
	uint8_t i = 0;

	mutex_lock(&i2c->ipi_mutex);

	while (left_num) {
		struct i2c_msg *msg = &msgs[num - left_num];

		if (!msg->buf) {
			dev_dbg(i2c->dev, "data buffer is absent.\n");
			ret = -EINVAL;
			goto err_exit;
		}

		ipi_msg->device_addr = msg[0].addr;

		if (msg->flags & I2C_M_RD) {
			ipi_msg->i2c_ipi_mode = READ_MODE_IPI;
			ipi_msg->read_len = msg[0].len;
		} else {
			ipi_msg->i2c_ipi_mode = WRITE_MODE_IPI;
			ipi_msg->write_len = msg[0].len;
			for (i = 0; i < msg[0].len; i++)
		                ipi_msg->write_buffer[i] = msg[0].buf[i];
		}

		if (!(msg[0].flags & I2C_M_RD) && left_num > 1 &&
				(msg[1].flags & I2C_M_RD) &&
				msg[0].addr == msg[1].addr && msg[1].buf) {
			/* combined two messages into one transaction */
			ipi_msg->i2c_ipi_mode = WRITE_READ_MODE_IPI;
			ipi_msg->read_len = msg[1].len;

			left_num--;
		}

		ret = ipi_transfer(i2c, ipi_msg);
		if (ret < 0)
			goto err_exit;

		if (ipi_msg->i2c_ipi_mode != WRITE_MODE_IPI) {
			if (ipi_msg->i2c_ipi_mode == WRITE_READ_MODE_IPI)
				msg++;
			for (i = 0; i < msg[0].len; i++) {
				msg[0].buf[i] = ipi_msg->read_buffer[i];
				dev_dbg(i2c->dev,"ipi_msg->read_buffer[%d]: %d ", i,
					ipi_msg->read_buffer[i]);
			}
		}

		left_num--;
	}
	/* the return value is number of executed messages */
	ret = num;

err_exit:
	mutex_unlock(&i2c->ipi_mutex);
	return ret;
}

static bool check_dsp_status(struct adsp_ipi_i2c *i2c)
{
	int ret;
	struct i2c_ipi_msg_t *ipi_msg = &i2c->ipi_msg;

	mutex_lock(&i2c->ipi_mutex);
	ipi_msg->i2c_ipi_mode = STATUS_MODE_IPI;
	ipi_msg->read_len = 1;
	ret = ipi_transfer(i2c, ipi_msg);

	mutex_unlock(&i2c->ipi_mutex);

	if (ret == 0 && ipi_msg->read_buffer[0] != READY)
		ret = -EIO;

	return ret;
}

static u32 adsp_ipi_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm adsp_ipi_i2c_algorithm = {
	.master_xfer = adsp_ipi_i2c_transfer,
	.functionality = adsp_ipi_i2c_functionality,
};

static int adsp_ipi_i2c_parse_dt(struct device_node *np, struct adsp_ipi_i2c *i2c)
{
	int ret;

	ret = of_property_read_u32(np, "clock-frequency", &i2c->speed_hz);
	if (ret < 0)
		i2c->speed_hz = I2C_DEFAULT_SPEED;

	return 0;
}

static int adsp_ipi_i2c_probe(struct platform_device *pdev)
{
	struct adsp_ipi_i2c *i2c;
	int i, ret;

	/* Make sure DSP FW load has completed */
	switch (amzn_dsp_fw_load_status()) {
	case DSP_FW_LOAD_ERROR:
		return -ENOENT;
	case DSP_FW_LOAD_UNLOADED:
	case DSP_FW_LOAD_IN_PROCESS:
		return -EPROBE_DEFER;
	case DSP_FW_LOAD_COMPLETE:
	/* Fallthrough */
		break;
	}

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	i2c->ipi_msg.magic = IPI_MSG_I2C_MAGIC_NUMBER;
	i2c->ipi_msg.bus_num = I2C_BUS;
	i2c->ipi_msg.data = i2c;
	mutex_init(&i2c->ipi_mutex);
	init_completion(&i2c->msg_complete);

	i2c->pdata = of_device_get_match_data(&pdev->dev);
	i2c->adap.algo = &adsp_ipi_i2c_algorithm;
	i2c->adap.quirks = i2c->pdata->quirks;
	i2c->adap.timeout = msecs_to_jiffies(3000);
	i2c->adap.retries = 3;		/* Default retry value. */

	i2c->adap.dev.of_node = pdev->dev.of_node;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.owner = THIS_MODULE;
	strlcpy(i2c->adap.name, I2C_DRV_NAME, sizeof(i2c->adap.name));

	i2c->dev = &pdev->dev;
	i2c->pdata = of_device_get_match_data(&pdev->dev);

	ret = adsp_ipi_i2c_parse_dt(pdev->dev.of_node, i2c);
	if (ret)
		dev_info(&pdev->dev, "clock-frequency not found\n");

	i2c->ipi_msg.speed_hz = i2c->speed_hz;

	i2c->clk_top = devm_clk_get(&pdev->dev, "top");
	if (IS_ERR(i2c->clk_top)) {
		dev_err(&pdev->dev, "cannot get top clock\n");
		return PTR_ERR(i2c->clk_top);
	}

	i2c->clk_parent = devm_clk_get(&pdev->dev, "parent");
	if (IS_ERR(i2c->clk_parent)) {
		dev_info(&pdev->dev, "cannot get parents clock\n");
	} else {
		ret = clk_set_parent(i2c->clk_top, i2c->clk_parent);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to clk_set_parent (%d)\n", ret);
			return ret;
		}
	}

	/* Leave TOP clk enabled so CCF won't turn it off when other child clks
	   are disabled */
	ret = clk_prepare_enable(i2c->clk_top);
	if (ret) {
		dev_err(&pdev->dev, "dma clock main enable failed!\n");
		return ret;
	}

	i2c->adap.dev.of_node = pdev->dev.of_node;

	ret = adsp_ipi_registration(ADSP_IPI_I2C,
			(ipi_handler_t)ipi_callback, "i2c_ipi");
	if (ret) {
		dev_err(&pdev->dev, "Failed to register i2c ipi : %d\n", ret);
		goto err;
	}

	/* Check and wait for DSP firmware */
	for (i = 0; i < i2c->adap.retries; i++)
	{
		ret = check_dsp_status(i2c);
		if (!ret)
			break;
	}
	if (ret) {
		goto out;
	}

	i2c_set_adapdata(&i2c->adap, i2c);

	ret = i2c_add_adapter(&i2c->adap);
	if (!ret) {
		platform_set_drvdata(pdev, i2c);
		return ret;
	}
out:
	adsp_ipi_unregistration(ADSP_IPI_I2C);
err:
	clk_disable_unprepare(i2c->clk_top);

	return ret;
}

static int adsp_ipi_i2c_remove(struct platform_device *pdev)
{
	struct adsp_ipi_i2c *i2c = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c->adap);
	adsp_ipi_unregistration(ADSP_IPI_I2C);

	return 0;
}

static struct platform_driver adsp_ipi_i2c_driver = {
	.probe = adsp_ipi_i2c_probe,
	.remove = adsp_ipi_i2c_remove,
	.driver = {
		.name = I2C_DRV_NAME,
		.of_match_table = of_match_ptr(adsp_ipi_i2c_of_match),
	},
};

static int __init adsp_ipi_i2c_init(void)
{
	return platform_driver_register(&adsp_ipi_i2c_driver);
}

static void __exit adsp_ipi_i2c_exit(void)
{
	platform_driver_unregister(&adsp_ipi_i2c_driver);
}
module_init(adsp_ipi_i2c_init);
module_exit(adsp_ipi_i2c_exit);

MODULE_DEVICE_TABLE(of, adsp_ipi_i2c_of_match);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Pseudo I2C over IPI Bus Driver");
