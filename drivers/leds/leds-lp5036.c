/*
 * leds-lp5036.c
 *
 * Copyright (c) 2018 Amazon.com, Inc. or its affiliates. All Rights
 * Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>

/* Config Registers */
#define REG_DEV_CONFIG0			0x00
#define DEV_CONFIG0_CHIP_EN		BIT(6)
#define DEV_CONFIG0_BITS		DEV_CONFIG0_CHIP_EN
#define REG_DEV_CONFIG1			0x01
#define DEV_CONFIG1_LOG_SCL_EN		BIT(5)
#define DEV_CONFIG1_PWR_SAVE_EN		BIT(4)
#define DEV_CONFIG1_AUTO_INC_EN		BIT(3)
#define DEV_CONFIG1_PWM_DITH_EN		BIT(2)
#define DEV_CONFIG1_MAX_CURR_OPT	BIT(1)
#define DEV_CONFIG1_LED_GLOBAL_OFF	BIT(0)
#define REG_LED_CONFIG0			0x02
#define LED_CONFIG0_LED7_BANK_EN	BIT(7)
#define LED_CONFIG0_LED6_BANK_EN	BIT(6)
#define LED_CONFIG0_LED5_BANK_EN	BIT(5)
#define LED_CONFIG0_LED4_BANK_EN	BIT(4)
#define LED_CONFIG0_LED3_BANK_EN	BIT(3)
#define LED_CONFIG0_LED2_BANK_EN	BIT(2)
#define LED_CONFIG0_LED1_BANK_EN	BIT(1)
#define LED_CONFIG0_LED0_BANK_EN	BIT(0)
#define REG_LED_CONFIG1			0x03
#define LED_CONFIG1_LED11_BANK_EN	BIT(3)
#define LED_CONFIG1_LED10_BANK_EN	BIT(2)
#define LED_CONFIG1_LED9_BANK_EN	BIT(1)
#define LED_CONFIG1_LED8_BANK_EN	BIT(0)

/* Bank Registers */
#define REG_BANK_BRIGHTNESS		0x04
#define REG_BANK_A_COLOR		0x05
#define REG_BANK_B_COLOR		0x06
#define REG_BANK_C_COLOR		0x07

/* Indiviual Brightness Registers Start */
#define REG_LED0_BRIGHTNESS		0x08

/* Indiviual Color Registers Start */
#define REG_LED0_G			0x14

/* Reset Register */
#define REG_RST				0x38
#define REG_RST_RST_VAL			0xFF

/* Other Values */
#define NUM_LEDS			12
#define NUM_COLOR_CHANNELS		3
#define NUM_LED_CHANNELS		(NUM_LEDS * NUM_COLOR_CHANNELS)
#define DEFAULT_ANIMATION_FRAME_DELAY_MS 88
#define DECIMAL_BASE			10
#define HEXADECIMAL_BASE		16
#define NUM_LED_CALIB_PARAMS		3
#define BYTE_MASK			0xFF
#define INDEX_LEDCALIBENABLE		0
#define INDEX_PWMSCALING		1
#define INDEX_PWMMAXLIMIT		2
#define LED_PWM_MAX_SCALING		0x7F
#define LED_PWM_SCALING_DEFAULT		0x7F7F7F
#define LED_PWM_MAX_LIMIT_DEFAULT	0xFFFFFF
#define LED_ENABLE			1

static int ledcalibparams[NUM_LED_CALIB_PARAMS];
static int led_params_count = NUM_LED_CALIB_PARAMS;

MODULE_PARM_DESC(ledcalibparams, "ledcalibparams=<enable/disable>, 0xRRGGBB(ledpwmscaling bitmap), 0xRRGGBB(ledpwmmaxlimit bitmap)");
module_param_array(ledcalibparams, int, &led_params_count, S_IRUGO);

static const uint8_t frames[][NUM_LED_CHANNELS] = {
	{0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0xff, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0xff, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0xff, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0xff, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0xff, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0xff, 0xff, 0x00, 0x00, 0xff},

	{0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
	 0x00, 0x00, 0xff, 0x00, 0xff, 0xff},
};

static const uint8_t clear_frame[NUM_LED_CHANNELS];

struct lp5036_data {
	struct mutex lock;
	struct regulator *gpio_regulator;
	struct i2c_client *client;
	uint8_t channel_map[NUM_LED_CHANNELS];
	bool animation_play;
	struct task_struct *animation_task;
	uint32_t animation_frame_delay_ms;
	int ledcalibenable;
	int ledpwmscaling;
	int ledpwmmaxlimiter;
	uint8_t ledpwmmaxlimiterrgb[NUM_COLOR_CHANNELS];
	uint8_t ledpwmscalingrgb[NUM_COLOR_CHANNELS];
	struct gpio_desc *enable_gpio;
};

static int lp5036_read_reg(struct i2c_client *client,
			   uint32_t reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int lp5036_write_reg(struct i2c_client *client,
			    uint32_t reg,
			    uint8_t value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int lp5036_write_block(struct i2c_client *client,
		uint32_t reg, uint32_t len, uint8_t *values)
{
	int ret = 0;

	while (len > I2C_SMBUS_BLOCK_MAX) {
		ret = i2c_smbus_write_i2c_block_data(client, reg, I2C_SMBUS_BLOCK_MAX,
					values);
		if (ret)
			return ret;

		len = len - I2C_SMBUS_BLOCK_MAX;
		reg = reg + I2C_SMBUS_BLOCK_MAX;
		values = values + I2C_SMBUS_BLOCK_MAX;
	}

	if (len)
		ret = i2c_smbus_write_i2c_block_data(client, reg, len, values);

	return ret;
}

static int lp5036_read_led_color(struct i2c_client *client,
				      uint32_t channel)
{
	return lp5036_read_reg(client, REG_LED0_G + channel);
}

static int lp5036_read_led_brightness(struct i2c_client *client,
					uint32_t led)
{
	return lp5036_read_reg(client, REG_LED0_BRIGHTNESS + led);
}

static int lp5036_update_frame(struct i2c_client *client, uint8_t *frame)
{
	return lp5036_write_block(client, REG_LED0_G, NUM_LED_CHANNELS, frame);
}

static int lp5036_update_led_brightness(struct i2c_client *client,
					uint32_t led,
					uint8_t value)
{
	return lp5036_write_reg(client, REG_LED0_BRIGHTNESS + led, value);
}

static ssize_t brightness_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int i;
	int ret;
	int len = 0;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	mutex_lock(&pdata->lock);
	for (i = 0; i < NUM_LEDS; i++) {
		ret = lp5036_read_led_brightness(pdata->client, i);
		if (ret < 0)
			goto fail;
		len += scnprintf(buf + len, PAGE_SIZE - len, "%02x", ret);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	ret = len;
fail:
	mutex_unlock(&pdata->lock);
	return ret;
}

static ssize_t brightness_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t len)
{
	int i;
	int ret;
	int count = 0;
	char val[3] = {0,};
	uint8_t new_brightness[NUM_LEDS];
	struct lp5036_data *pdata = dev_get_platdata(dev);

	for (i = 0; i < NUM_LEDS * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &new_brightness[count]);
		if (ret) {
			dev_err(dev,
				"Invalid input for brightness_store, ret=%d\n",
				ret);
			return ret;
		}
		count++;
	}

	mutex_lock(&pdata->lock);
	for (i = 0; i < NUM_LEDS; i++) {
		ret = lp5036_update_led_brightness(pdata->client,
						   i,
						   new_brightness[i]);
		if (ret) {
			dev_err(dev, "Failed to update brightness, ret=%d\n",
				ret);
			goto fail;
		}
	}

	ret = len;
fail:
	mutex_unlock(&pdata->lock);
	return ret;

}
static DEVICE_ATTR_RW(brightness);

static int update_frame(struct lp5036_data *pdata,
			const uint8_t *buf)
{
	uint8_t value;
	int i, ret, channel_index, scaled_value, numerator, denominator, pwm;
	u8 frame[NUM_LED_CHANNELS];

	mutex_lock(&pdata->lock);

	for (i = 0; i < NUM_LED_CHANNELS; i++) {
		if (pdata->ledcalibenable) {
			pwm = pdata->ledpwmscalingrgb[i % NUM_COLOR_CHANNELS];

			numerator = (int) buf[i] * pwm;
			denominator = LED_PWM_MAX_SCALING;
			scaled_value = numerator / denominator;

			value = min_t(int,
				      scaled_value,
				      (int) pdata->ledpwmmaxlimiterrgb[i %
				      NUM_COLOR_CHANNELS]);
		} else {
			value = buf[i];
		}

		channel_index = pdata->channel_map[i];

		frame[channel_index] = value;
	}

	ret = lp5036_update_frame(pdata->client, frame);
	if (ret) {
		dev_err(&pdata->client->dev,
			"Failed to update frame, ret=%d\n", ret);
	}

	mutex_unlock(&pdata->lock);
	return ret;
}

static ssize_t frame_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int i;
	int ret;
	int len = 0;
	int channel_index;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	mutex_lock(&pdata->lock);
	for (i = 0; i < NUM_LED_CHANNELS; i++) {
		channel_index = pdata->channel_map[i];
		ret = lp5036_read_led_color(pdata->client, channel_index);
		if (ret < 0)
			goto fail;
		len += scnprintf(buf + len, PAGE_SIZE - len, "%02x", ret);
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	ret = len;
fail:
	mutex_unlock(&pdata->lock);
	return ret;
}

static ssize_t frame_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t len)
{
	int i;
	int ret;
	int count = 0;
	char val[3] = {0,};
	uint8_t new_frame[NUM_LED_CHANNELS];
	struct lp5036_data *pdata = dev_get_platdata(dev);

	for (i = 0; i < NUM_LED_CHANNELS * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &new_frame[count]);
		if (ret) {
			dev_err(dev, "Invalid input for frame_store, ret=%d\n",
				ret);
			return ret;
		}
		count++;
	}

	ret = update_frame(pdata, new_frame);
	if (ret)
		goto fail;

	ret = len;
fail:
	return ret;
}
static DEVICE_ATTR_RW(frame);

static void lp5036_convert_to_rgb(struct lp5036_data *pdata,
				  uint8_t *rgb_array,
				  int bitmap)
{
	int i, j;

	for (i = NUM_COLOR_CHANNELS - 1, j = 0; i >= 0; i--, j++)
		rgb_array[j] = (bitmap & (BYTE_MASK << 8 * i)) >> 8 * i;
}

static ssize_t ledcalibenable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	int ret, temp;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	ret = kstrtoint(buf, DECIMAL_BASE, &temp);
	if (ret) {
		dev_err(dev,
			"Invalid input for ledcalibenable_store, ret=%d\n",
			ret);
		return ret;
	}

	mutex_lock(&pdata->lock);
	pdata->ledcalibenable = temp;
	mutex_unlock(&pdata->lock);

	return len;
}

static ssize_t ledcalibenable_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->ledcalibenable);
	mutex_unlock(&pdata->lock);

	return ret;
}

static DEVICE_ATTR_RW(ledcalibenable);

static ssize_t ledpwmscaling_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t len)
{
	int ret, temp;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	ret = kstrtoint(buf, HEXADECIMAL_BASE, &temp);
	if (ret) {
		dev_err(dev,
			"Invalid input for ledpwmscaling_store, ret=%d\n",
			ret);
		return ret;
	}

	mutex_lock(&pdata->lock);

	pdata->ledpwmscaling = temp;
	lp5036_convert_to_rgb(pdata, pdata->ledpwmscalingrgb, temp);

	mutex_unlock(&pdata->lock);

	dev_dbg(&pdata->client->dev, "ledpwmscaling = %x\n",
			pdata->ledpwmscaling);

	return len;
}

static ssize_t ledpwmscaling_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%x\n", pdata->ledpwmscaling);
	mutex_unlock(&pdata->lock);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmscaling);

static ssize_t ledpwmmaxlimit_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	int ret, temp;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	ret = kstrtoint(buf, HEXADECIMAL_BASE, &temp);
	if (ret) {
		dev_err(dev,
			"Invalid input for ledpwmmaxlimit_store, ret=%d\n",
			ret);
		return ret;
	}

	mutex_lock(&pdata->lock);
	pdata->ledpwmmaxlimiter = temp;
	lp5036_convert_to_rgb(pdata, pdata->ledpwmmaxlimiterrgb, temp);
	mutex_unlock(&pdata->lock);

	return len;
}

static ssize_t ledpwmmaxlimit_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%x\n", pdata->ledpwmmaxlimiter);
	mutex_unlock(&pdata->lock);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmmaxlimit);

static int animation_thread(void *data)
{
	int ret;
	int i = 0;
	struct lp5036_data *pdata = (struct lp5036_data *) data;

	while (!kthread_should_stop()) {
		ret = update_frame(pdata, &frames[i][0]);
		if (ret)
			goto fail;
		msleep(pdata->animation_frame_delay_ms);
		i = (i + 1) % ARRAY_SIZE(frames);
	}
	ret = update_frame(pdata, clear_frame);
fail:
	return ret;
}

static void animation_thread_run(struct lp5036_data *pdata)
{
	mutex_lock(&pdata->lock);
	if (!pdata->animation_task) {
		pdata->animation_task = kthread_run(animation_thread,
						    (void *) pdata,
						    "animation_thread");
		if (IS_ERR(pdata->animation_task)) {
			dev_err(&pdata->client->dev,
				"Could not start animation thread\n");
			pdata->animation_task = NULL;
		}
	}
	mutex_unlock(&pdata->lock);
}

static void animation_thread_stop(struct lp5036_data *pdata)
{
	struct task_struct *stop_struct = NULL;

	mutex_lock(&pdata->lock);
	if (pdata->animation_task) {
		stop_struct = pdata->animation_task;
		pdata->animation_task = NULL;
	}
	mutex_unlock(&pdata->lock);

	if (stop_struct)
		kthread_stop(stop_struct);
}

static ssize_t animation_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t len)
{
	int ret;
	uint8_t val;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		dev_err(dev, "Invalid input to animation_store, ret=%d\n",
			ret);
		return ret;
	}

	if (!val)
		animation_thread_stop(pdata);
	else
		animation_thread_run(pdata);

	return len;
}

static ssize_t animation_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(dev);

	ret = sprintf(buf, "%d\n", pdata->animation_task != NULL);

	return ret;
}
static DEVICE_ATTR_RW(animation);

static int lp5036_disable(struct i2c_client *client)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(&client->dev);

	ret = lp5036_write_reg(client, REG_DEV_CONFIG0,
			       (~DEV_CONFIG0_CHIP_EN & DEV_CONFIG0_BITS));
	if (ret)
		dev_err(&client->dev, "Failed to disable device, ret=%d\n",
			ret);

	gpiod_set_value_cansleep(pdata->enable_gpio, !LED_ENABLE);

	ret = regulator_disable(pdata->gpio_regulator);
	if (ret)
		dev_err(&client->dev,
			"Failed to disable gpio regulator, ret=%d\n", ret);

	return ret;
}

static void lp5036_parse_ledcalibparams_from_dt(struct device *dev,
						struct device_node *node)
{
	const char *ledcalibparams_prop;
	int ledcalibparams_enable;
	int ledcalibparams_pwmscaling;
	int ledcalibparams_pwmmaxlimit;
	int ret;

	ret = of_property_read_string(node, "ledcalibparams",
					&ledcalibparams_prop);
	if (ret) {
		dev_err(dev,
			"Error reading ledcalibparams property from dt, ret=%d\n",
			ret);
		return;
	}

	ret = sscanf(ledcalibparams_prop, "%d %x %x", &ledcalibparams_enable,
		&ledcalibparams_pwmscaling, &ledcalibparams_pwmmaxlimit);
	if (ret == NUM_LED_CALIB_PARAMS) {
		ledcalibparams[INDEX_LEDCALIBENABLE] = ledcalibparams_enable;
		ledcalibparams[INDEX_PWMSCALING] = ledcalibparams_pwmscaling;
		ledcalibparams[INDEX_PWMMAXLIMIT] = ledcalibparams_pwmmaxlimit;
	} else {
		dev_err(dev, "Error scanning ledcalibparams, ret=%d\n", ret);
	}
}

/*
 * This method will make the channel-map default to a one-to-one mapping if the
 * node does not exist or contains an invalid value.
 */
static void lp5036_parse_dt(struct lp5036_data *pdata,
			    struct device_node *node)
{
	int i;
	int ret;
	uint8_t channel_map[NUM_LED_CHANNELS];

	pdata->animation_play = of_property_read_bool(node, "animation-play");

	ret = of_property_read_u32(node, "animation-frame-delay-ms",
				   &pdata->animation_frame_delay_ms);
	if (ret)
		pdata->animation_frame_delay_ms = DEFAULT_ANIMATION_FRAME_DELAY_MS;

	ret = of_property_read_u8_array(node, "channel-map", channel_map,
					NUM_LED_CHANNELS);

	for (i = 0; i < NUM_LED_CHANNELS; i++) {
		if (ret) {
			pdata->channel_map[i] = i;
		} else if (channel_map[i] < 0 || channel_map[i] >= NUM_LED_CHANNELS) {
			dev_warn(&pdata->client->dev,
				"Invalid channel_map element %d at index %d\n",
				(int) channel_map[i], i);
			pdata->channel_map[i] = i;
		} else {
			pdata->channel_map[i] = channel_map[i];
		}
	}

	lp5036_parse_ledcalibparams_from_dt(&pdata->client->dev, node);
}

static int lp5036_enable(struct i2c_client *client, bool reset)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(&client->dev);

	ret = regulator_enable(pdata->gpio_regulator);
	if (ret) {
		dev_err(&client->dev,
			"Failed to enable gpio regulator, ret=%d\n", ret);
		return ret;
	}

	gpiod_set_value_cansleep(pdata->enable_gpio, LED_ENABLE);

	/* tEN_H = 500 us*/
	usleep_range(500, 700);

	if (reset) {
		ret = lp5036_write_reg(client, REG_RST, REG_RST_RST_VAL);
		if (ret) {
			dev_err(&client->dev, "Failed to reset registers, ret=%d\n",
				ret);
			goto fail;
		}
	}

	ret = lp5036_write_reg(client, REG_DEV_CONFIG0, DEV_CONFIG0_CHIP_EN);
	if (ret) {
		dev_err(&client->dev, "Failed to start device, ret=%d\n", ret);
		goto fail;
	}

fail:
	if (ret)
		lp5036_disable(client);
	return ret;
}

static int lp5036_gpio_regulator_probe(struct i2c_client *client)
{
	int ret = 0;
	struct lp5036_data *pdata = dev_get_platdata(&client->dev);

	pdata->gpio_regulator = devm_regulator_get(&client->dev,
						   "led-regulator");
	if (IS_ERR(pdata->gpio_regulator))
		ret = PTR_ERR(pdata->gpio_regulator);

	return ret;
}

static int lp5036_power_on_device(struct i2c_client *client)
{
	int ret;

	ret = lp5036_gpio_regulator_probe(client);
	if (ret == -EPROBE_DEFER) {
		dev_info(&client->dev,
			 "Failed to probe gpio regulator, ret=%d\n", ret);
		goto fail;
	}
	if (ret) {
		dev_err(&client->dev,
			"Failed to probe gpio regulator, ret=%d\n", ret);
		goto fail;
	}

	ret = lp5036_enable(client, true);
	if (ret) {
		dev_err(&client->dev, "Failed to enable device, ret=%d\n", ret);
		goto fail;
	}

fail:
	return ret;
}

static void lp5036_initialize_led_calibrations(struct lp5036_data *pdata)
{
	if (ledcalibparams[INDEX_LEDCALIBENABLE]) {
		pdata->ledpwmmaxlimiter = ledcalibparams[INDEX_PWMMAXLIMIT];
		pdata->ledpwmscaling = ledcalibparams[INDEX_PWMSCALING];
	} else {
		pdata->ledpwmmaxlimiter = LED_PWM_MAX_LIMIT_DEFAULT;
		pdata->ledpwmscaling = LED_PWM_SCALING_DEFAULT;
	}

	pdata->ledcalibenable = ledcalibparams[INDEX_LEDCALIBENABLE];

	lp5036_convert_to_rgb(pdata,
				pdata->ledpwmscalingrgb,
				pdata->ledpwmscaling);

	lp5036_convert_to_rgb(pdata,
				pdata->ledpwmmaxlimiterrgb,
				pdata->ledpwmmaxlimiter);
}

static int lp5036_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct lp5036_data *pdata = devm_kzalloc(&client->dev,
						 sizeof(struct lp5036_data),
						 GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev,
			"Failed to allocate memory for platform data, ret=%d\n",
			ret);
		ret = -ENOMEM;
		goto fail;
	}

	pdata->client = client;
	mutex_init(&pdata->lock);
	lp5036_parse_dt(pdata, client->dev.of_node);

	pdata->enable_gpio = devm_gpiod_get_optional(&client->dev, "enable",
							GPIOD_OUT_HIGH);
	if (IS_ERR(pdata->enable_gpio)) {
		ret = PTR_ERR(pdata->enable_gpio);
		dev_err(&client->dev, "enable gpio request failed %d\n", ret);
		goto fail;
	}

	client->dev.platform_data = pdata;
	ret = lp5036_power_on_device(client);
	if (ret) {
		dev_err(&client->dev, "Failed to power on device, ret=%d\n",
			ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_frame);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create frame sysfs entry, ret=%d\n", ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_brightness);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create brightness sysfs entry, ret=%d\n",
			ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_animation);
	if (ret) {
		dev_err(&client->dev,
			"Could not create animation sysfs entry, ret=%d\n",
			ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_ledcalibenable);
	if (ret) {
		dev_err(&client->dev,
			"Could not create ledcalibenable sysfs entry, ret=%d\n",
			ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_ledpwmmaxlimit);
	if (ret) {
		dev_err(&client->dev,
			"Could not create ledpwmmaxlimit sysfs entry, ret=%d\n",
			ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_ledpwmscaling);
	if (ret) {
		dev_err(&client->dev,
			"Could not create ledpwmscaling sysfs entry, ret=%d\n",
			ret);
		goto fail;
	}

	lp5036_initialize_led_calibrations(pdata);

	if (pdata->animation_play)
		animation_thread_run(pdata);

fail:
	return ret;
}

static int lp5036_remove(struct i2c_client *client)
{
	int ret;
	struct lp5036_data *pdata = dev_get_platdata(&client->dev);

	animation_thread_stop(pdata);

	device_remove_file(&client->dev, &dev_attr_frame);
	device_remove_file(&client->dev, &dev_attr_brightness);
	device_remove_file(&client->dev, &dev_attr_animation);
	device_remove_file(&client->dev, &dev_attr_ledcalibenable);
	device_remove_file(&client->dev, &dev_attr_ledpwmmaxlimit);
	device_remove_file(&client->dev, &dev_attr_ledpwmscaling);

	ret = lp5036_disable(client);
	mutex_destroy(&pdata->lock);

	return ret;
}

static struct i2c_device_id lp5036_i2c_match[] = {
	{"ti,lp5036-frame", 0},
};
MODULE_DEVICE_TABLE(i2c, lp5036_i2c_match);

static const struct of_device_id lp5036_of_match[] = {
	{.compatible = "ti,lp5036-frame"},
	{},
};
MODULE_DEVICE_TABLE(of, lp5036_of_match);

static struct i2c_driver lp5036_driver = {
	.driver = {
		.name = "lp5036",
		.of_match_table = of_match_ptr(lp5036_of_match),
	},
	.probe = lp5036_probe,
	.remove = lp5036_remove,
	.id_table = lp5036_i2c_match,
};

module_i2c_driver(lp5036_driver);

MODULE_DESCRIPTION("Texas Instruments LP5036 LED Driver");
MODULE_AUTHOR("Amazon.com");
MODULE_LICENSE("GPL");
