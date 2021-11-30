/*
 * Copyright (C) 2019 Mediatek Inc.
 * Author: Joe Yang <joe.yang@mediatek.com>
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
#define GPT_RTC_DRVNAME		"gpt-rtc"

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/delay.h>
#include <linux/pm_wakeirq.h>

#define GPT_IRQ_EN_REG		0x00
#define GPT_IRQ_ENABLE(val)	BIT((val) - 1)
#define GPT_IRQ_ACK_REG		0x08
#define GPT_IRQ_ACK(val)	BIT((val) - 1)

#define TIMER_CTRL_REG(val)	(0x10 * (val))
#define TIMER_CTRL_OP(val)	(((val) & 0x3) << 4)
#define TIMER_CTRL_OP_ONESHOT	(0)
#define TIMER_CTRL_OP_REPEAT	(1)
#define TIMER_CTRL_OP_FREERUN	(3)
#define TIMER_CTRL_CLEAR	(2)
#define TIMER_CTRL_ENABLE	(1)
#define TIMER_CTRL_DISABLE	(0)

#define TIMER_CLK_REG(val)	(0x04 + (0x10 * (val)))
#define TIMER_CLK_SRC(val)	(((val) & 0x1) << 4)
#define TIMER_CLK_SRC_SYS13M	(0)
#define TIMER_CLK_SRC_RTC32K	(1)

#define TIMER_CNT_REG(val)	(0x08 + (0x10 * (val)))
#define TIMER_CMP_REG(val)	(0x0C + (0x10 * (val)))

/* (0xffffffff / 32768)s, clk is 32768Hz */
#define GTP_RTC_ALARM_MAX	131071

struct gpt_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	int			irq;
	void __iomem		*gpt_base;
	int			gpt_idx; /* start from 1 */
	struct rtc_time		alarm_time;
	bool			alarm_en;
};

static irqreturn_t gpt_rtc_irq_handler(int irq, void *data)
{
	struct gpt_rtc *rtc = data;

	/* Acknowledge irq */
	writel(GPT_IRQ_ACK(rtc->gpt_idx), rtc->gpt_base + GPT_IRQ_ACK_REG);
	dev_dbg(rtc->dev, "%s: ack=0x%x, ctrl=0x%x, count=0x%x, compare=0x%x, clk=0x%x, irq=0x%x\n",
		__func__,
		readl(rtc->gpt_base + GPT_IRQ_ACK_REG),
		readl(rtc->gpt_base + TIMER_CTRL_REG(rtc->gpt_idx)),
		readl(rtc->gpt_base + TIMER_CNT_REG(rtc->gpt_idx)),
		readl(rtc->gpt_base + TIMER_CMP_REG(rtc->gpt_idx)),
		readl(rtc->gpt_base + TIMER_CLK_REG(rtc->gpt_idx)),
		readl(rtc->gpt_base + GPT_IRQ_EN_REG));
	return IRQ_HANDLED;
}

static int gpt_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct timespec64 tv_now;

	getnstimeofday64(&tv_now);
	rtc_time64_to_tm(tv_now.tv_sec, tm);
	return 0;
}

static int gpt_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	dev_notice(dev, "gpt-as-rtc cannot support set time\n");
	return 0;
}

static int gpt_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct gpt_rtc *rtc = dev_get_drvdata(dev);
	u32 gpt_en;

	gpt_en = readl(rtc->gpt_base + TIMER_CTRL_REG(rtc->gpt_idx))
		& TIMER_CTRL_ENABLE;

	alm->enabled = !!gpt_en;
	tm->tm_sec = rtc->alarm_time.tm_sec;
	tm->tm_min = rtc->alarm_time.tm_min;
	tm->tm_hour = rtc->alarm_time.tm_hour;
	tm->tm_mday = rtc->alarm_time.tm_mday;
	tm->tm_mon = rtc->alarm_time.tm_mon;
	tm->tm_year = rtc->alarm_time.tm_year;

	return 0;
}

static int gpt_rtc_setup(struct gpt_rtc *rtc, u32 compare)
{
	u32 val = 0;
	u32 retry_cnt = 0, retry_max_cnt = 100;

	/* Swtich 13M to clear counter and set compare value */
	writel(TIMER_CLK_SRC(TIMER_CLK_SRC_SYS13M),
		rtc->gpt_base + TIMER_CLK_REG(rtc->gpt_idx));

	/* disable & clear counter */
	writel(TIMER_CTRL_CLEAR | TIMER_CTRL_DISABLE,
		rtc->gpt_base + TIMER_CTRL_REG(rtc->gpt_idx));

	while (1) {
		val = readl(rtc->gpt_base + TIMER_CNT_REG(rtc->gpt_idx));
		if (!val)
			break;

		if (retry_cnt >= retry_max_cnt) {
			dev_err(rtc->dev, "clear counter fail, val=%u compare=%u\n",
				val, compare);
			break;
		}

		udelay(10);
		retry_cnt++;
	};

	dev_dbg(rtc->dev, "%s: retry_cnt=%d val=%u\n", __func__, retry_cnt, val);

	writel(compare, rtc->gpt_base + TIMER_CMP_REG(rtc->gpt_idx));
	/* add delay to write compare value into register */
	usleep_range(1000, 1100);

	/* Switch 32K */
	writel(TIMER_CLK_SRC(TIMER_CLK_SRC_RTC32K),
		rtc->gpt_base + TIMER_CLK_REG(rtc->gpt_idx));

	/* enable irq */
	val = readl(rtc->gpt_base + GPT_IRQ_EN_REG);
	writel(val | GPT_IRQ_ENABLE(rtc->gpt_idx),
		rtc->gpt_base + GPT_IRQ_EN_REG);

	/* enable timer */
	val = TIMER_CTRL_ENABLE | TIMER_CTRL_OP(TIMER_CTRL_OP_ONESHOT);
	writel(val, rtc->gpt_base + TIMER_CTRL_REG(rtc->gpt_idx));

	return 0;
}

static int gpt_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct gpt_rtc *rtc = dev_get_drvdata(dev);
	u32 val;
	u32 compare;
	struct timespec64 tv_now, tv_alarm;

	mutex_lock(&rtc->lock);

	if (alm->enabled && device_may_wakeup(dev)) {
		rtc->alarm_time.tm_sec = tm->tm_sec;
		rtc->alarm_time.tm_min = tm->tm_min;
		rtc->alarm_time.tm_hour = tm->tm_hour;
		rtc->alarm_time.tm_mday = tm->tm_mday;
		rtc->alarm_time.tm_mon = tm->tm_mon;
		rtc->alarm_time.tm_year = tm->tm_year;

		tv_alarm.tv_sec = rtc_tm_to_time64(tm);
		getnstimeofday64(&tv_now);
		if (tv_alarm.tv_sec <= tv_now.tv_sec) {
			dev_info(dev, "alarm time(%ld) is less than now time(%ld)\n",
				(long)tv_alarm.tv_sec, (long)tv_now.tv_sec);
			goto _end;
		}
		if (tv_alarm.tv_sec - tv_now.tv_sec > GTP_RTC_ALARM_MAX) {
			dev_err(dev, "alarm time(%ld) exceeds max value(%d)\n",
				(long)(tv_alarm.tv_sec - tv_now.tv_sec),
				GTP_RTC_ALARM_MAX);
			mutex_unlock(&rtc->lock);
			return -EINVAL;
		}

		/* clk is 32768Hz, 32768 = 1 << 15,
		 * use left shirt to calculate
		 */
		compare = (tv_alarm.tv_sec - tv_now.tv_sec) << 15;
		dev_info(dev, "alarm: %lds, gpt compare: %u\n",
			(long)(tv_alarm.tv_sec - tv_now.tv_sec), compare);

		/* enable gpt */
		gpt_rtc_setup(rtc, compare);
	} else {
		val = readl(rtc->gpt_base + GPT_IRQ_EN_REG);
		val &= ~(GPT_IRQ_ENABLE(rtc->gpt_idx));
		writel(val, rtc->gpt_base + GPT_IRQ_EN_REG);
		writel(TIMER_CTRL_CLEAR | TIMER_CTRL_DISABLE,
			rtc->gpt_base + TIMER_CTRL_REG(rtc->gpt_idx));
	}

_end:
	mutex_unlock(&rtc->lock);
	return 0;
}

static const struct rtc_class_ops gpt_rtc_ops = {
	.read_time  = gpt_rtc_read_time,
	.set_time   = gpt_rtc_set_time,
	.read_alarm = gpt_rtc_read_alarm,
	.set_alarm  = gpt_rtc_set_alarm,
};

static int gpt_rtc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct gpt_rtc *rtc;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct gpt_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->gpt_base = of_iomap(np, 0);
	if (IS_ERR(rtc->gpt_base)) {
		dev_err(&pdev->dev, "Cannot map register base\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "gpt-index", &rtc->gpt_idx);
	if (ret) {
		dev_err(&pdev->dev, "Cannot read gpt-index\n");
		return -EINVAL;
	}
	dev_info(&pdev->dev, "use gpt%d as rtc\n", rtc->gpt_idx);

	/* Select GPT clk source */
	writel(TIMER_CLK_SRC(TIMER_CLK_SRC_RTC32K),
		rtc->gpt_base + TIMER_CLK_REG(rtc->gpt_idx));

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return -EINVAL;

	rtc->dev = &pdev->dev;
	mutex_init(&rtc->lock);

	/* initialize alarm time: 2010-01-01 0:0:0 */
	rtc->alarm_time.tm_sec = 0;
	rtc->alarm_time.tm_min = 0;
	rtc->alarm_time.tm_hour = 0;
	rtc->alarm_time.tm_mday = 1;
	rtc->alarm_time.tm_mon = 0;
	rtc->alarm_time.tm_year = 2010 - 1900;

	platform_set_drvdata(pdev, rtc);

	ret = request_threaded_irq(rtc->irq, NULL,
				   gpt_rtc_irq_handler, IRQF_ONESHOT,
				   GPT_RTC_DRVNAME, rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request gpt-rtc IRQ: %d: %d\n",
			rtc->irq, ret);
		goto out_dispose_irq;
	}

	device_init_wakeup(&pdev->dev, 1);
	dev_pm_set_wake_irq(&pdev->dev, rtc->irq);

	rtc->rtc_dev = rtc_device_register(GPT_RTC_DRVNAME, &pdev->dev,
					   &gpt_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_err(&pdev->dev, "register gpt rtc device failed\n");
		ret = PTR_ERR(rtc->rtc_dev);
		goto out_free_irq;
	}

	return 0;

out_free_irq:
	free_irq(rtc->irq, rtc->rtc_dev);
out_dispose_irq:
	irq_dispose_mapping(rtc->irq);
	return ret;
}

static int gpt_rtc_remove(struct platform_device *pdev)
{
	struct gpt_rtc *rtc = platform_get_drvdata(pdev);

	free_irq(rtc->irq, rtc);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gpt_rtc_of_match[] = {
	{ .compatible = "mediatek,mt8512-gpt-rtc", },
	{ }
};

MODULE_DEVICE_TABLE(of, gpt_rtc_of_match);
#endif

static struct platform_driver gpt_rtc_drv = {
	.driver	= {
		.name		= GPT_RTC_DRVNAME,
#ifdef CONFIG_OF
		.of_match_table	= gpt_rtc_of_match,
#endif
	},
	.probe	= gpt_rtc_probe,
	.remove	= gpt_rtc_remove,
};

module_platform_driver(gpt_rtc_drv);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Joe Yang <joe.yang@mediatek.com>");
MODULE_DESCRIPTION("GPT as RTC driver");
