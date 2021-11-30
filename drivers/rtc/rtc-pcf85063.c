/*
 * An I2C driver for the PCF85063 RTC
 * Copyright 2014 Rose Technology
 *
 * Author: Søren Andersen <san@rosetechnology.dk>
 * Maintainers: http://www.nslu2-linux.org/
 *
 * based on the other drivers in this same directory.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/module.h>

/*
 * Information for this driver was pulled from the following datasheets.
 *
 *  http://www.nxp.com/documents/data_sheet/PCF85063A.pdf
 *  http://www.nxp.com/documents/data_sheet/PCF85063TP.pdf
 *
 *  PCF85063A -- Rev. 6 — 18 November 2015
 *  PCF85063TP -- Rev. 4 — 6 May 2015
*/

#define PCF85063_REG_CTRL1		0x00 /* status */
#define PCF85063_REG_CTRL1_STOP		BIT(5)
#define PCF85063_REG_CTRL2		0x01

#define PCF85063_REG_SC			0x04 /* datetime */
#define PCF85063_REG_SC_OS		0x80
#define PCF85063_REG_MN			0x05
#define PCF85063_REG_HR			0x06
#define PCF85063_REG_DM			0x07
#define PCF85063_REG_DW			0x08
#define PCF85063_REG_MO			0x09
#define PCF85063_REG_YR			0x0A
#define PCF85063_REG_ALRM_SC		0x0B /* ALARM */
#define PCF85063_REG_ALRM_MN		0x0C
#define PCF85063_REG_ALRM_HR		0x0D
#define PCF85063_REG_ALRM_DM		0x0E
#define PCF85063_REG_ALRM_DW		0x0F

#define PCF85063_AIE_MASK		BIT(7)
#define PCF85063_AF_MASK		BIT(6)
#define PCF85063_ALRM_SC_MASK		0x7F /* 0 - 59 */
#define PCF85063_ALRM_MN_MASK		0x7F /* 0 - 59 */
#define PCF85063_ALRM_HR_MASK		0x3F /* 24-hour mode */
#define PCF85063_ALRM_DM_MASK		0x3F /* 1 - 31 */
#define PCF85063_ALRM_DW_MASK		0x07 /* 0 - 6 */

static struct i2c_driver pcf85063_driver;

struct pcf85063 {
	struct rtc_device *rtc;
	struct i2c_client *client;
};

static int pcf85063_stop_clock(struct i2c_client *client, u8 *ctrl1)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, PCF85063_REG_CTRL1);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to stop the clock\n");
		return -EIO;
	}

	/* stop the clock */
	ret |= PCF85063_REG_CTRL1_STOP;

	ret = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL1, ret);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to stop the clock\n");
		return -EIO;
	}

	*ctrl1 = ret;

	return 0;
}

static int pcf85063_start_clock(struct i2c_client *client, u8 ctrl1)
{
	s32 ret;

	/* start the clock */
	ctrl1 &= PCF85063_REG_CTRL1_STOP;

	ret = i2c_smbus_write_byte_data(client, PCF85063_REG_CTRL1, ctrl1);
	if (ret < 0) {
		dev_err(&client->dev, "Failing to start the clock\n");
		return -EIO;
	}

	return 0;
}

static int pcf85063_get_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	int rc;
	u8 regs[7];

	/*
	 * while reading, the time/date registers are blocked and not updated
	 * anymore until the access is finished. To not lose a second
	 * event, the access must be finished within one second. So, read all
	 * time/date registers in one turn.
	 */
	rc = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_SC,
					   sizeof(regs), regs);
	if (rc != sizeof(regs)) {
		dev_err(&client->dev, "date/time register read error\n");
		return -EIO;
	}

	/* if the clock has lost its power it makes no sense to use its time */
	if (regs[0] & PCF85063_REG_SC_OS) {
		dev_warn(&client->dev, "Power loss detected, invalid time\n");
		return -EINVAL;
	}

	tm->tm_sec = bcd2bin(regs[0] & 0x7F);
	tm->tm_min = bcd2bin(regs[1] & 0x7F);
	tm->tm_hour = bcd2bin(regs[2] & 0x3F); /* rtc hr 0-23 */
	tm->tm_mday = bcd2bin(regs[3] & 0x3F);
	tm->tm_wday = regs[4] & 0x07;
	tm->tm_mon = bcd2bin(regs[5] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = bcd2bin(regs[6]);
	tm->tm_year += 100;

	return rtc_valid_tm(tm);
}

static int pcf85063_set_datetime(struct i2c_client *client, struct rtc_time *tm)
{
	int rc;
	u8 regs[7];
	u8 ctrl1;

	if ((tm->tm_year < 100) || (tm->tm_year > 199))
		return -EINVAL;

	/*
	 * to accurately set the time, reset the divider chain and keep it in
	 * reset state until all time/date registers are written
	 */
	rc = pcf85063_stop_clock(client, &ctrl1);
	if (rc != 0)
		return rc;

	/* hours, minutes and seconds */
	regs[0] = bin2bcd(tm->tm_sec) & 0x7F; /* clear OS flag */

	regs[1] = bin2bcd(tm->tm_min);
	regs[2] = bin2bcd(tm->tm_hour);

	/* Day of month, 1 - 31 */
	regs[3] = bin2bcd(tm->tm_mday);

	/* Day, 0 - 6 */
	regs[4] = tm->tm_wday & 0x07;

	/* month, 1 - 12 */
	regs[5] = bin2bcd(tm->tm_mon + 1);

	/* year and century */
	regs[6] = bin2bcd(tm->tm_year - 100);

	/* write all registers at once */
	rc = i2c_smbus_write_i2c_block_data(client, PCF85063_REG_SC,
					    sizeof(regs), regs);
	if (rc < 0) {
		dev_err(&client->dev, "date/time register write error\n");
		return rc;
	}

	/*
	 * Write the control register as a separate action since the size of
	 * the register space is different between the PCF85063TP and
	 * PCF85063A devices.  The rollover point can not be used.
	 */
	rc = pcf85063_start_clock(client, ctrl1);
	if (rc != 0)
		return rc;

	return 0;
}

static int pcf85063_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return pcf85063_get_datetime(to_i2c_client(dev), tm);
}

static int pcf85063_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return pcf85063_set_datetime(to_i2c_client(dev), tm);
}

static int pcf85063_irq_enable(struct device *dev, unsigned int enabled)
{
	unsigned char regs;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);

	ret = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_CTRL2,
					   sizeof(regs), &regs);
	if (ret != sizeof(regs)) {
		dev_err(&client->dev, "Config register read failed\n");
		return -EIO;
	}

	/* Config Alarm interrupt */
	if (enabled)
		regs |= PCF85063_AIE_MASK;
	else
		regs &= ~PCF85063_AIE_MASK;

	/* Clear Alarm flag */
	regs &= ~(PCF85063_AF_MASK);

	ret = i2c_smbus_write_i2c_block_data(client, PCF85063_REG_CTRL2,
					   sizeof(regs), &regs);
	if (ret < 0) {
		dev_err(&client->dev, "Config register write failed\n");
		return ret;
	}

	return 0;
}

static int pcf85063_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char regs[5];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_ALRM_SC,
					   sizeof(regs), regs);
	if (ret != sizeof(regs)) {
		dev_err(&client->dev, "Alarm registers read failed\n");
		return -EIO;
	}

	dev_dbg(&client->dev,
		"%s: raw data is min=%02x, hr=%02x, mday=%02x, wday=%02x\n",
		__func__, regs[0], regs[1], regs[2], regs[3]);

	tm->time.tm_sec = bcd2bin(regs[0] & PCF85063_ALRM_SC_MASK);
	tm->time.tm_min = bcd2bin(regs[1] & PCF85063_ALRM_MN_MASK);
	tm->time.tm_hour = bcd2bin(regs[2] & PCF85063_ALRM_HR_MASK);
	tm->time.tm_mday = bcd2bin(regs[3] & PCF85063_ALRM_DM_MASK);
	tm->time.tm_wday = bcd2bin(regs[4] & PCF85063_ALRM_DW_MASK);

	ret = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_CTRL2,
					    1, regs);
	if (ret != 1) {
		dev_err(&client->dev, "Config register read failed\n");
		return -EIO;
	}

	tm->enabled = regs[0] & PCF85063_AIE_MASK;
	tm->pending = regs[0] & PCF85063_AF_MASK;

	dev_dbg(&client->dev, "%s: tm is mins=%d, hours=%d, mday=%d, wday=%d,"
		" enabled=%d, pending=%d\n", __func__, tm->time.tm_min,
		tm->time.tm_hour, tm->time.tm_mday, tm->time.tm_wday,
		tm->enabled, tm->pending);

	return 0;
}

static int pcf85063_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char regs[5];
	int ret;

	dev_dbg(dev, "%s, sec=%d min=%d hour=%d wday=%d mday=%d "
		"enabled=%d pending=%d\n", __func__, tm->time.tm_sec,
		tm->time.tm_min, tm->time.tm_hour, tm->time.tm_wday,
		tm->time.tm_mday, tm->enabled, tm->pending);

	regs[0] = bin2bcd(tm->time.tm_sec) & PCF85063_ALRM_SC_MASK;
	regs[1] = bin2bcd(tm->time.tm_min) & PCF85063_ALRM_MN_MASK;
	regs[2] = bin2bcd(tm->time.tm_hour) & PCF85063_ALRM_HR_MASK;
	regs[3] = bin2bcd(tm->time.tm_mday) & PCF85063_ALRM_DM_MASK;
	regs[4] = tm->time.tm_wday & PCF85063_ALRM_DW_MASK;

	ret = i2c_smbus_write_i2c_block_data(client, PCF85063_REG_ALRM_SC,
					    sizeof(regs), regs);
	if (ret < 0) {
		dev_err(&client->dev, "Alarm registers write error\n");
		return ret;
	}

	return pcf85063_irq_enable(dev, 1);
}

static const struct rtc_class_ops pcf85063_rtc_ops = {
	.read_time	= pcf85063_rtc_read_time,
	.set_time	= pcf85063_rtc_set_time,
	.read_alarm	= pcf85063_rtc_read_alarm,
	.set_alarm	= pcf85063_rtc_set_alarm,
	.alarm_irq_enable = pcf85063_irq_enable
};

static irqreturn_t pcf85063_irq(int irq, void *dev_id)
{
	int ret;
	char pending, regs;
	struct pcf85063 *pcf85063 = i2c_get_clientdata(dev_id);
	struct i2c_client *client = pcf85063->client;

	ret = i2c_smbus_read_i2c_block_data(client, PCF85063_REG_CTRL2, 1,
						&regs);
	if (ret != 1) {
		dev_err(&client->dev, "Status register read failed\n");
		return IRQ_NONE;
	}

	pending = regs & PCF85063_AF_MASK;

	if (pending) {
		rtc_update_irq(pcf85063->rtc, 1, RTC_IRQF | RTC_AF);
		pcf85063_irq_enable(&client->dev, 1);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int pcf85063_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct rtc_device *rtc;
	struct pcf85063 *pcf85063;
	int ret;

	dev_dbg(&client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	pcf85063 = devm_kzalloc(&client->dev, sizeof(struct pcf85063),
				GFP_KERNEL);
	if (!pcf85063)
		return -ENOMEM;

	i2c_set_clientdata(client, pcf85063);
	pcf85063->client = client;

	device_set_wakeup_capable(&client->dev, 1);

	rtc = devm_rtc_device_register(&client->dev,
				       pcf85063_driver.driver.name,
				       &pcf85063_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	pcf85063->rtc = rtc;

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, pcf85063_irq, IRQF_SHARED|IRQF_ONESHOT,
				rtc->name, client);
		if (ret) {
			dev_err(&client->dev, "unable to request IRQ %d\n",
								client->irq);
			return ret;
		}
	}

	return 0;
}

static const struct i2c_device_id pcf85063_id[] = {
	{ "pcf85063", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcf85063_id);

#ifdef CONFIG_OF
static const struct of_device_id pcf85063_of_match[] = {
	{ .compatible = "nxp,pcf85063" },
	{}
};
MODULE_DEVICE_TABLE(of, pcf85063_of_match);
#endif

static struct i2c_driver pcf85063_driver = {
	.driver		= {
		.name	= "rtc-pcf85063",
		.of_match_table = of_match_ptr(pcf85063_of_match),
	},
	.probe		= pcf85063_probe,
	.id_table	= pcf85063_id,
};

module_i2c_driver(pcf85063_driver);

MODULE_AUTHOR("Søren Andersen <san@rosetechnology.dk>");
MODULE_DESCRIPTION("PCF85063 RTC driver");
MODULE_LICENSE("GPL");
