/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/completion.h>
#include <linux/module.h>
//nclude <linux/sched/clock.h>
#include <linux/spi/spi.h>

#include <linux/random.h>

#include <linux/clk.h>

static void spi_slave_txbuf_malloc(struct spi_transfer *trans)
{
	int i; //random;

	trans->tx_buf = kzalloc(trans->len, GFP_KERNEL);

	//random = get_random_int() && 0xff;
	for (i = 0; i < trans->len; i++)
		*((char *)trans->tx_buf + i) = i + 1;//random + i;
}

static void spi_slave_rxbuf_malloc(struct spi_transfer *trans)
{
	trans->rx_buf = kzalloc(trans->len, GFP_KERNEL);
	memset(trans->rx_buf, 0, trans->len);
}

static void spi_slave_txbuf_free(struct spi_transfer *trans)
{
	kfree(trans->tx_buf);
}

static void spi_slave_rxbuf_free(struct spi_transfer *trans)
{
	kfree(trans->rx_buf);
}

static void spi_slave_dump_packet(char *name, u8 *ptr, int len)
{
	int i;

	pr_info("%s: ", name);
	for (i = 0; i < len; i++)
		pr_info(" %02x", ptr[i]);

	pr_info("\n");
}

int spis_loopback_check(struct spi_transfer *trans)
{
	int i, err = 0;

	for (i = 0; i < trans->len; i++) {
		if (*((u8 *) trans->tx_buf + i) != *((u8 *) trans->rx_buf + i))
			err++;
	}

	if (err) {
		pr_info("spis_len:%d, err %d\n", trans->len, err);
		spi_slave_dump_packet("spis tx",
			(char *)trans->tx_buf, trans->len);
		spi_slave_dump_packet("spis rx", trans->rx_buf, trans->len);
		pr_info("spis test fail.");
		return -1;
	}

	pr_info("spis_len:%d, err %d\n", trans->len, err);
	pr_info("spis test pass.");

	return 0;
}

static int spi_slave_txrx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_txbuf_malloc(&trans);
	spi_slave_rxbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spis_loopback_check(&trans);

	spi_slave_txbuf_free(&trans);
	spi_slave_rxbuf_free(&trans);

	return ret;
}

static int spi_slave_tx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_txbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spi_slave_dump_packet("spis tx", (char *)trans.tx_buf, len);

	spi_slave_txbuf_free(&trans);

	return ret;
}

static int spi_slave_rx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_rxbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spi_slave_dump_packet("spis rx", trans.rx_buf, len);

	spi_slave_rxbuf_free(&trans);

	return ret;
}

static ssize_t spis_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int len;//ret = 0;
//	int addr, old, new, reg_val;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);
//	struct mtk_spi_slave *mdata = spi_get_drvdata(spi);

	if (!strncmp(buf, "txrx", 4)) {
		buf += 5;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_txrx_transfer(spi, len);
	} else if (!strncmp(buf, "onlytx", 6)) {
		buf += 7;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_tx_transfer(spi, len);
	} else if (!strncmp(buf, "onlyrx", 6)) {
		buf += 7;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_rx_transfer(spi, len);
	}
	//else if (!strncmp(buf, "abort", 5)) {
	//	spi_slave_abort(spi);
	//}

	return count;
}

static DEVICE_ATTR(spi, 0200, NULL, spis_store);

static struct device_attribute *spi_attribute[] = {
	&dev_attr_spi,
};
static int spi_create_attribute(struct device *dev)
{
	int size, idx;
	int err = 0;

	size = ARRAY_SIZE(spi_attribute);
	for (idx = 0; idx < size; idx++) {
		err = device_create_file(dev, spi_attribute[idx]);
		if (err)
			break;
	}
	return err;
}

static int spi_slave_mt27xx_test_probe(struct spi_device *spi)
{
	spi_create_attribute(&spi->dev);
	return 0;
}

static int spi_slave_mt27xx_test_remove(struct spi_device *spi)
{
	//spi_slave_abort(spi);
	return 0;
}

static struct spi_driver spi_slave_mt27xx_test_driver = {
	.driver = {
		.name	= "spi-slave-mt27xx-test",
	},
	.probe		= spi_slave_mt27xx_test_probe,
	.remove		= spi_slave_mt27xx_test_remove,
};
module_spi_driver(spi_slave_mt27xx_test_driver);

MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");
