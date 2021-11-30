/*
 * leds-aw21036.c
 *
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates. All Rights
 * Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/led-class-rgb.h>
#include <linux/led-class-frame.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <uapi/linux/uleds.h>

#define AW21036_I2C_NAME "aw21036_led"

#define REG_GCR       0x00
#define REG_BR_BASE   0x01
#define REG_UPDATE    0x49
#define REG_COL_BASE  0x4A
#define REG_GCCR      0x6E
#define REG_PHCR      0x70
#define REG_OSDCR     0x71
#define REG_OSST_BASE 0x72
#define REG_OTCR      0x77
#define REG_SSCR      0x78
#define REG_UVCR      0x79
#define REG_GCR2      0x7A
#define REG_GCR4      0x7C
#define REG_VER       0x7E
#define REG_RST       0x7F
#define REG_WBR       0x90
#define REG_WBG       0x91
#define REG_WBB       0x92
#define REG_PATCFG    0xA0
#define REG_PATGO     0xA1
#define REG_PATT_BASE 0xA2
#define REG_FADEH     0xA6
#define REG_FADEL     0xA7
#define REG_GCOLR     0xA8
#define REG_GCOLG     0xA9
#define REG_GCOLB     0xAA
#define REG_GCFG0     0xAB
#define REG_GCFG1     0xAC

#define AW21036_SW_RESET          0x00
#define AW21036_UPDATE_TRIGGER    0x00
#define AW21036_CHIP_ENABLE       BIT(0)
#define AW21036_RGBMD_ENABLE      BIT(0)
#define AW21036_STARTUP_BR        0xFF

#define AW21036_GCFG0_BR_GROUP    0xFF
#define AW21036_GCFG1_BR_GROUP    0x1F

#define MAX_LED_STRINGS           12

#define NUM_LED_CALIB_PARAMS      3
#define BYTE_MASK                 0xFF
#define INDEX_LEDCALIBENABLE      0
#define INDEX_PWMSCALING          1
#define INDEX_PWMMAXLIMIT         2
#define LED_PWM_MAX_SCALING       0x7F
#define LED_PWM_SCALING_DEFAULT   0x7F7F7F
#define LED_PWM_MAX_LIMIT_DEFAULT 0xFFFFFF

#define LED_CALIB_COLOR_COUNT     3
#define LED_CALIB_RED_INDEX       0
#define LED_CALIB_GREEN_INDEX     1
#define LED_CALIB_BLUE_INDEX      2

#define NUM_LED_CHANNELS          36

enum e_led_type {
	e_led_rgb = 0,
	e_led_frame,
};

static int ledcalibparams[NUM_LED_CALIB_PARAMS];
static int led_params_count = NUM_LED_CALIB_PARAMS;

MODULE_PARM_DESC(ledcalibparams, "ledcalibparams=<enable/disable>, 0xRRGGBB(ledpwmscaling bitmap), 0xRRGGBB(ledpwmmaxlimit bitmap)");
module_param_array(ledcalibparams, int, &led_params_count, S_IRUGO);

enum e_rgb_map_idx {
	e_rgb_map_idx_red = 0,
	e_rgb_map_idx_green,
	e_rgb_map_idx_blue,
	e_rgb_map_idx_count,
};
enum e_ctrl_bank {
	e_ctrl_bank_disabled = 0,
	e_ctrl_bank_brightness,
	e_ctrl_bank_full,
	e_ctrl_bank_count,
};

struct aw21036_led {
	char label[LED_MAX_NAME_SIZE];
	struct led_classdev_rgb rgb_cdev;
	struct led_classdev_frame frame_cdev;
	struct aw21036 *priv;
	int led_number;
	u8 ctrl_bank_enabled;
	u8 rgb_map[e_rgb_map_idx_count];
	u8 led_type;
};

/**
 * struct aw21036 -
 * @enable_gpio: Hardware enable gpio
 * @regulator: LED supply regulator pointer
 * @client: Pointer to the I2C client
 * @regmap: Devices register map
 * @dev: Pointer to the devices device struct
 * @lock: Lock for reading/writing the device
 * @model_id: ID of the device
 * @leds: Array of LED strings
 */
struct aw21036 {
	struct gpio_desc *enable_gpio;
	struct regulator *regulator;
	struct i2c_client *client;
	struct regmap *regmap;
	struct device *dev;
	struct mutex lock;
	int max_leds;
	int num_of_leds;

	int ledcalibenable;
	u8 ledpwmmaxlimiterrgb[LED_CALIB_COLOR_COUNT];
	u8 ledpwmscalingrgb[LED_CALIB_COLOR_COUNT];
	/* This needs to be at the end of the struct */
	struct aw21036_led leds[];
};

static const struct reg_default aw21036_reg_defs[] = {
	{REG_GCR, 0x00},
	{REG_BR_BASE, 0x00},
	{REG_UPDATE, 0x00},
	{REG_COL_BASE, 0x00},
	{REG_GCCR, 0x00},
	{REG_PHCR, 0x00},
	{REG_OSDCR, 0x00},
	{REG_OSST_BASE, 0x00},
	{REG_OTCR, 0x00},
	{REG_SSCR, 0x00},
	{REG_UVCR, 0x00},
	{REG_GCR2, 0x00},
	{REG_GCR4, 0x00},
	{REG_VER, 0xA8},
	{REG_RST, 0x18},
	{REG_WBR, 0xFF},
	{REG_WBG, 0xFF},
	{REG_WBB, 0xFF},
	{REG_PATCFG, 0x00},
	{REG_PATGO, 0x00},
	{REG_PATT_BASE, 0x00},
	{REG_FADEH, 0x00},
	{REG_FADEL, 0x00},
	{REG_GCOLR, 0x00},
	{REG_GCOLG, 0x00},
	{REG_GCOLB, 0x00},
	{REG_GCFG0, 0x00},
	{REG_GCFG1, 0x00},
};

static const struct regmap_config aw21036_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = REG_GCFG1,
	.reg_defaults = aw21036_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(aw21036_reg_defs),
	.cache_type = REGCACHE_RBTREE,
};

static struct aw21036_led *rgbled_cdev_to_led(struct led_classdev_rgb *rgbled_cdev)
{
	return container_of(rgbled_cdev, struct aw21036_led, rgb_cdev);
}

static struct aw21036_led *ledframe_cdev_to_led(struct led_classdev_frame *ledframe_cdev)
{
	return container_of(ledframe_cdev, struct aw21036_led, frame_cdev);
}

static int aw21036_set_color(struct led_classdev_rgb *rgbled_cdev)
{
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct aw21036 *priv = led->priv;
	u8 bulk_write_offset = 0;
	u8 led_offset = 0;
	int ret, scaled_value;
	u8 colorArray[e_rgb_map_idx_count] = {0};

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		bulk_write_offset = REG_GCOLR;
	} else {
		led_offset = (led->led_number * 3);
		bulk_write_offset = REG_COL_BASE + led_offset;
	}

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_RED_INDEX] *
				colors->red / LED_PWM_MAX_SCALING;
		colors->red = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_RED_INDEX]);
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_GREEN_INDEX] *
				colors->green / LED_PWM_MAX_SCALING;

		colors->green = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_GREEN_INDEX]);
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_BLUE_INDEX] *
				colors->blue / LED_PWM_MAX_SCALING;
		colors->blue = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_BLUE_INDEX]);
	}
	colorArray[led->rgb_map[e_rgb_map_idx_red]] = colors->red;
	colorArray[led->rgb_map[e_rgb_map_idx_green]] = colors->green;
	colorArray[led->rgb_map[e_rgb_map_idx_blue]] = colors->blue;

	mutex_lock(&led->priv->lock);
	ret = regmap_bulk_write(priv->regmap, bulk_write_offset, colorArray,
			e_rgb_map_idx_count);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot write LED value\n");
	}
	mutex_unlock(&led->priv->lock);

	return ret;
}

static int aw21036_set_red(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct aw21036 *priv = led->priv;
	u8 led_offset;
	u8 red_reg;
	int ret, scaled_value;

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		red_reg = REG_GCOLR + led->rgb_map[e_rgb_map_idx_red];
	} else {
		led_offset = (led->led_number * 3);
		red_reg = REG_COL_BASE + led_offset + led->rgb_map[e_rgb_map_idx_red];
	}

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_RED_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_RED_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, red_reg, brightness);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write LED value\n");
	mutex_unlock(&led->priv->lock);

	return ret;
}

static int aw21036_set_green(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct aw21036 *priv = led->priv;
	u8 led_offset;
	u8 green_reg;
	int ret, scaled_value;

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		green_reg = REG_GCOLR + led->rgb_map[e_rgb_map_idx_green];
	} else {
		led_offset = (led->led_number * 3);
		green_reg = REG_COL_BASE + led_offset + led->rgb_map[e_rgb_map_idx_green];
	}

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_GREEN_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_GREEN_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, green_reg, brightness);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write LED value\n");
	mutex_unlock(&led->priv->lock);

	return ret;
}

static int aw21036_set_blue(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct aw21036 *priv = led->priv;
	u8 led_offset;
	u8 blue_reg;
	int ret, scaled_value;

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		blue_reg = REG_GCOLR + led->rgb_map[e_rgb_map_idx_blue];
	} else {
		led_offset = (led->led_number * 3);
		blue_reg = REG_COL_BASE + led_offset + led->rgb_map[e_rgb_map_idx_blue];
	}

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_BLUE_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_BLUE_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, blue_reg, brightness);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write LED value\n");
	mutex_unlock(&led->priv->lock);

	return ret;
}

static int aw21036_brightness_set(struct aw21036_led *led,
				enum led_brightness brt_val)
{
	int ret = 0;
	u8 reg_val;

	mutex_lock(&led->priv->lock);

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		reg_val = REG_FADEL;
	} else if (led->ctrl_bank_enabled == e_ctrl_bank_brightness) {
		reg_val = REG_BR_BASE + led->led_number;
	} else {
		// TODO: Update to bulk write for 3x led number
		reg_val = REG_BR_BASE + led->led_number;
	}

	ret = regmap_write(led->priv->regmap, reg_val, brt_val);
	if (ret)
		dev_err(&led->priv->client->dev, "Cannot write brightness register\n");

	ret = regmap_write(led->priv->regmap, REG_UPDATE, AW21036_UPDATE_TRIGGER);
	if (ret)
		dev_err(&led->priv->client->dev, "Cannot write update register\n");

	mutex_unlock(&led->priv->lock);

	return ret;
}

static int aw21036_rgb_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brt_val)
{
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);

	return aw21036_brightness_set(led, brt_val);
}

static int aw21036_frame_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brt_val)
{
	struct led_classdev_frame *ledframe_cdev = lcdev_to_framecdev(led_cdev);
	struct aw21036_led *led = ledframe_cdev_to_led(ledframe_cdev);

	return aw21036_brightness_set(led, brt_val);
}

static enum led_brightness aw21036_brightness_get(struct aw21036_led *led)
{
	unsigned int brt_val = 0;
	u8 reg_val;
	int ret;

	mutex_lock(&led->priv->lock);

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		reg_val = REG_FADEL;
	} else if (led->ctrl_bank_enabled == e_ctrl_bank_brightness) {
		reg_val = REG_BR_BASE + led->led_number;
	} else {
		// TODO: update to bulk read for 3x led number
		reg_val = REG_BR_BASE + led->led_number;
	}

	ret = regmap_read(led->priv->regmap, reg_val, &brt_val);

	mutex_unlock(&led->priv->lock);

	return brt_val;
}

static enum led_brightness aw21036_rgb_brightness_get(struct led_classdev *led_cdev)
{
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct aw21036_led *led = rgbled_cdev_to_led(rgbled_cdev);

	return aw21036_brightness_get(led);
}

static enum led_brightness aw21036_frame_brightness_get(struct led_classdev *led_cdev)
{
	struct led_classdev_frame *ledframe_cdev = lcdev_to_framecdev(led_cdev);
	struct aw21036_led *led = ledframe_cdev_to_led(ledframe_cdev);

	return aw21036_brightness_get(led);
}

static int aw21036_set_frame(struct led_classdev_frame *ledframe_cdev)
{
	struct aw21036_led *led = ledframe_cdev_to_led(ledframe_cdev);
	struct aw21036 *priv = led->priv;
	int ret, scaled_value;
	int i, c_index;
	u8 frame[NUM_LED_CHANNELS] = {0};

	for(i = 0; i < NUM_LED_CHANNELS; i++) {
		if (priv->ledcalibenable) {
			scaled_value = (int) priv->ledpwmscalingrgb[i % LED_CALIB_COLOR_COUNT] *
				ledframe_cdev->new_frame[i] / LED_PWM_MAX_SCALING;
			ledframe_cdev->new_frame[i] = min_t(int, scaled_value,
					(int) priv->ledpwmmaxlimiterrgb[i % LED_CALIB_COLOR_COUNT]);
		}

		c_index = ledframe_cdev->channel_map[i];
		frame[c_index] = ledframe_cdev->new_frame[i];
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_bulk_write(priv->regmap, REG_COL_BASE, frame, NUM_LED_CHANNELS);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot update LED Frame\n");
	}
	mutex_unlock(&led->priv->lock);

	return ret;
}

static struct led_frame_ops aw21036_frame_ops = {
	.set_frame =  aw21036_set_frame,
};

static int aw21036_enable(struct aw21036 *priv)
{
	int ret;
	if (priv->regulator) {
		ret = regulator_enable(priv->regulator);
		if (ret) {
			dev_err(&priv->client->dev, "Failed to enable regulator\n");
			return ret;
		}
	}

	if (priv->enable_gpio) {
		ret = gpiod_direction_output(priv->enable_gpio, 1);
		/* tEN_H = 500 us*/
		usleep_range(500, 700);
		if (ret) {
			dev_err(&priv->client->dev, "Unable to set enable gpio\n");
			return ret;
		}
	} else {
		ret = regmap_write(priv->regmap, REG_RST, AW21036_SW_RESET);
		if (ret) {
			dev_err(&priv->client->dev, "Cannot reset the device\n");
			return ret;
		}
	}

	ret = regmap_write(priv->regmap, REG_GCR, AW21036_CHIP_ENABLE);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write ctrl enable\n");


	ret = regmap_write(priv->regmap, REG_GCCR, AW21036_STARTUP_BR);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write global current control\n");


	ret = regmap_write(priv->regmap, REG_GCR2, AW21036_RGBMD_ENABLE);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write brightness group\n");

	return ret;
}

static struct led_rgb_ops aw21036_rgb_ops = {
	.set_color = aw21036_set_color,
	.set_red_brightness = aw21036_set_red,
	.set_green_brightness = aw21036_set_green,
	.set_blue_brightness = aw21036_set_blue,
};

static int aw21036_init(struct aw21036 *priv)
{
	int i;
	int ret;
	struct aw21036_led *led;
	struct led_classdev *led_cdev;

	ret = aw21036_enable(priv);
	if (ret)
		return ret;

	for (i = 0; i < priv->num_of_leds; i++) {
		led = &priv->leds[i];
		led->priv = priv;

		if (led->led_type == e_led_frame) {
			led->frame_cdev.ops = &aw21036_frame_ops;
			led_cdev = &led->frame_cdev.led_cdev;
			led_cdev->name = led->label;
			led_cdev->brightness_set_blocking = aw21036_frame_brightness_set;
			led_cdev->brightness_get = aw21036_frame_brightness_get;
			ret = led_classdev_frame_register(&priv->client->dev, &led->frame_cdev);
			if (ret) {
				dev_err(&priv->client->dev, "led frame register err: %d\n", ret);
				while (--i >= 0) {
					led = &priv->leds[i];
					led_classdev_frame_unregister(&led->frame_cdev);
				}
				return ret;
			}

			ret = regmap_write(priv->regmap, REG_GCFG0, AW21036_GCFG0_BR_GROUP);
			if (ret)
				dev_err(&priv->client->dev, "Cannot write REG_GCFG0\n");
			ret = regmap_write(priv->regmap, REG_GCFG1, AW21036_GCFG1_BR_GROUP);
			if (ret)
				dev_err(&priv->client->dev, "Cannot write REG_GCFG1\n");

			led->ctrl_bank_enabled = e_ctrl_bank_full;
			aw21036_brightness_set(led, AW21036_STARTUP_BR);

			continue;
		}

		led->rgb_cdev.ops = &aw21036_rgb_ops;
		led_cdev = &led->rgb_cdev.led_cdev;
		led_cdev->name = led->label;
		led_cdev->brightness_set_blocking = aw21036_rgb_brightness_set;
		led_cdev->brightness_get = aw21036_rgb_brightness_get;

		ret = led_classdev_rgb_register(&priv->client->dev, &led->rgb_cdev);
		if (ret) {
			dev_err(&priv->client->dev, "led register err: %d\n", ret);
			while (--i >= 0) {
				led = &priv->leds[i];
				led_classdev_rgb_unregister(&led->rgb_cdev);
			}
			return ret;
		}
		aw21036_brightness_set(led, AW21036_STARTUP_BR);
		led->ctrl_bank_enabled = e_ctrl_bank_brightness;
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, AW21036_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

	return ret;
}

static void aw21036_convert_to_rgb(u8 *rgb_array, int bitmap)
{
	int i;
	for (i = 0; i < LED_CALIB_COLOR_COUNT; i++)
		/*
		 * extract R, G, B value from bitmap(0xRRGGBB) and assign
		 * to rgb_array {R, G, B}
		 */
		rgb_array[i] = (bitmap & (BYTE_MASK << 8 *
				(LED_CALIB_COLOR_COUNT - i - 1))) >>
				8 * (LED_CALIB_COLOR_COUNT - i - 1);
}

static void aw21036_initialize_led_calibrations(struct aw21036 *priv)
{
	priv->ledcalibenable = ledcalibparams[INDEX_LEDCALIBENABLE];

	if (priv->ledcalibenable) {
		aw21036_convert_to_rgb(priv->ledpwmscalingrgb,
					ledcalibparams[INDEX_PWMSCALING]);
		aw21036_convert_to_rgb(priv->ledpwmmaxlimiterrgb,
					ledcalibparams[INDEX_PWMMAXLIMIT]);
	} else {
		aw21036_convert_to_rgb(priv->ledpwmscalingrgb,
					LED_PWM_SCALING_DEFAULT);
		aw21036_convert_to_rgb(priv->ledpwmmaxlimiterrgb,
					LED_PWM_MAX_LIMIT_DEFAULT);
	}
}

static ssize_t global_current_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));
	uint8_t val;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		dev_err(dev, "AW21036: Invalid input to store GCCR\n");
		return ret;
	}
	if (val < 0 || val > 255) {
		dev_err(dev, "AW21036: Invalid input to store GCCR\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(&priv->lock);

	ret = regmap_write(priv->regmap, REG_GCCR, val);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write GCCR\n");

	mutex_unlock(&priv->lock);

	return len;
}

static ssize_t global_current_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));
	int val;
	ssize_t ret;

	mutex_lock(&priv->lock);

	ret = regmap_read(priv->regmap, REG_GCCR, &val);
	if (ret) {
		dev_err(dev, "Cannot read GCCR\n");
		mutex_unlock(&priv->lock);
		return ret;
	}

	mutex_unlock(&priv->lock);

	ret = sprintf(buf, "%d\n", val);

	return ret;
}

static DEVICE_ATTR_RW(global_current);

static ssize_t whitebalance_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int red, green, blue;
	u8 wb_array[e_rgb_map_idx_count] = {0};

	if (sscanf(buf, "%d %d %d", &red, &green, &blue) != 3) {
		dev_err(dev, "AW21036: unable to parse whitebalance input\n");
		ret = -EINVAL;
		return ret;
	}

	if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
		dev_err(dev, "AW21036: Invalid input to store white balance.\n");
		ret = -EINVAL;
		return ret;
	}

	wb_array[0] = red;
	wb_array[1] = green;
	wb_array[2] = blue;

	mutex_lock(&priv->lock);

	ret = regmap_bulk_write(priv->regmap, REG_WBR, wb_array,
							e_rgb_map_idx_count);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write whitebalance reg\n");

	mutex_unlock(&priv->lock);

	return len;
}

static ssize_t whitebalance_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	u8 wb_array[e_rgb_map_idx_count] = {0};

	mutex_lock(&priv->lock);

	ret = regmap_bulk_read(priv->regmap, REG_WBR, wb_array,
						   e_rgb_map_idx_count);
	if (ret) {
		dev_err(dev, "Cannot bulk read REG_WBR\n");
	}

	mutex_unlock(&priv->lock);

	ret = sprintf(buf + ret, "%d %d %d\n", wb_array[0], wb_array[1],
				  wb_array[2]);

	return ret;
}

static DEVICE_ATTR_RW(whitebalance);

static ssize_t ledcalibenable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	int ret, temp;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = kstrtoint(buf, 10, &temp);
	if (ret) {
		dev_err(dev, "Invalid input for ledcalibenable_store, ret=%d\n", ret);
		return ret;
	}

	priv->ledcalibenable = temp;

	return len;
}

static ssize_t ledcalibenable_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = sprintf(buf, "%d\n", priv->ledcalibenable);

	return ret;
}

static DEVICE_ATTR_RW(ledcalibenable);

static ssize_t ledpwmscaling_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t len)
{
	int ret, temp;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		dev_err(dev, "Invalid input for ledpwmscaling_store, ret=%d\n", ret);
		return ret;
	}

	aw21036_convert_to_rgb(priv->ledpwmscalingrgb, temp);

	dev_dbg(&priv->client->dev, "ledpwmscaling = %x\n", temp);

	return len;
}

static ssize_t ledpwmscaling_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int ret = 0, i;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	for (i = 0; i < e_rgb_map_idx_count; i++)
		ret += sprintf(buf + ret, "%x", priv->ledpwmscalingrgb[i]);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmscaling);

static ssize_t ledpwmmaxlimit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	int ret, temp;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		dev_err(dev, "Invalid input for ledpwmmaxlimit_store, ret=%d\n", ret);
		return ret;
	}

	aw21036_convert_to_rgb(priv->ledpwmmaxlimiterrgb, temp);

	return len;
}

static ssize_t ledpwmmaxlimit_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0, i;
	struct aw21036 *priv = i2c_get_clientdata(to_i2c_client(dev));

	for (i = 0; i < e_rgb_map_idx_count; i++)
		ret += sprintf(buf + ret, "%x",
				priv->ledpwmmaxlimiterrgb[i]);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmmaxlimit);

static void aw21036_parse_ledcalibparams_from_dt(struct device *dev)
{
	const char *ledcalibparams_prop;
	int ledcalibparams_enable;
	int ledcalibparams_pwmscaling;
	int ledcalibparams_pwmmaxlimit;
	int ret;
	struct device_node *node = dev->of_node;
	ret = of_property_read_string(node, "ledcalibparams",
					&ledcalibparams_prop);
	if (ret) {
		dev_info(dev,
			"Error reading ledcalibparams property from dt, ret=%d\n",
			ret);
		ledcalibparams[INDEX_LEDCALIBENABLE] = 0;
		ledcalibparams[INDEX_PWMSCALING] = LED_PWM_SCALING_DEFAULT;
		ledcalibparams[INDEX_PWMMAXLIMIT] = LED_PWM_MAX_LIMIT_DEFAULT;
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

static int parse_frame_from_dt(struct device *dev, struct device_node *node, struct aw21036_led *led)
{
	struct led_classdev_frame *frame_cdev;
	int i , ret;
	uint8_t channel_map[NUM_LED_CHANNELS];

	frame_cdev = &led->frame_cdev;

	ret = of_property_count_elems_of_size(node, "frame-channel-map", sizeof(uint8_t));
	if (ret > 0)
		frame_cdev->nr_channels = ret;
	else
		return ret;

		ret = of_property_read_u8_array(node, "frame-channel-map", channel_map,
					frame_cdev->nr_channels);
	if (ret)
		return ret;

	frame_cdev->channel_map = devm_kzalloc(dev, frame_cdev->nr_channels * sizeof(uint8_t),
				GFP_KERNEL);
	if (!frame_cdev->channel_map)
		return -ENOMEM;

	for (i = 0; i < frame_cdev->nr_channels; i++) {
		if (channel_map[i] < 0 || channel_map[i] >= NUM_LED_CHANNELS) {
			dev_warn(dev, "Invalid channel_map element %d at index %d\n",
					(int) channel_map[i], i);
			frame_cdev->channel_map[i] = i;
		} else {
			frame_cdev->channel_map[i] = channel_map[i];
		}
	}

	return 0;
}

static int aw21036_probe_dt(struct aw21036 *priv)
{
	struct fwnode_handle *child = NULL;
	struct led_classdev *led_cdev;
	struct aw21036_led *led;
	const char *name;
	int led_number;
	size_t i = 0;
	int rgb_map_count = 0, j = 0;
	int ret = 0;
	struct device_node *node;

	priv->enable_gpio = devm_gpiod_get_optional(&priv->client->dev, "enable",
												GPIOD_OUT_LOW);
	if (IS_ERR(priv->enable_gpio)) {
		ret = PTR_ERR(priv->enable_gpio);
		dev_err(&priv->client->dev, "Failed to get enable gpio: %d\n",
			ret);
		return ret;
	}

	priv->regulator = devm_regulator_get(&priv->client->dev, "vled");
	if (IS_ERR(priv->regulator))
		priv->regulator = NULL;

	priv->max_leds = MAX_LED_STRINGS;

	device_for_each_child_node(&priv->client->dev, child) {
		led = &priv->leds[i];

		ret = fwnode_property_read_string(child, "label", &name);
		if (ret)
			snprintf(led->label, sizeof(led->label),
				"%s::", priv->client->name);
		else
			snprintf(led->label, sizeof(led->label),
				 "%s:%s", priv->client->name, name);

		node = to_of_node(child);

		if (of_find_property(node, "frame-channel-map", NULL)) {
			led->led_type = e_led_frame;
			ret = parse_frame_from_dt(&priv->client->dev, node, led);
			if (ret)
				dev_err(&priv->client->dev, "Failed to read frame channel map %d\n", ret);
			i++;
			continue;
		}

		led->led_type = e_led_rgb;

		ret = fwnode_property_read_u32(child, "aw,led-module", &led_number);
		led->led_number = led_number;
		if (ret) {
			dev_err(&priv->client->dev,
				"led sourcing property missing\n");
			fwnode_handle_put(child);
			goto child_out;
		}

		if (led_number > priv->max_leds) {
			dev_err(&priv->client->dev,
				"led-sources property is invalid\n");
			ret = -EINVAL;
			fwnode_handle_put(child);
			goto child_out;
		}

		rgb_map_count = fwnode_property_read_u8_array(child, "rgb-map", NULL, 0);
		if (rgb_map_count < 0) {
			/* Assign rgb_map to default value if not in device tree */
			dev_info(&priv->client->dev,
					 "Assign rgb_map to default value <0, 1, 2>\n");
			for (j = 0; j < e_rgb_map_idx_count; j++)
				led->rgb_map[j] = j;
		} else if (rgb_map_count == e_rgb_map_idx_count) {
			ret = fwnode_property_read_u8_array(child, "rgb-map",
							led->rgb_map, rgb_map_count);
			for (j = 0; !ret && j < e_rgb_map_idx_count; j++) {
				if (led->rgb_map[j] < 0 || led->rgb_map[j] >= e_rgb_map_idx_count) {
					dev_err(&priv->client->dev,
							"Invalid rgb_map element %d at index %d",
							led->rgb_map[j], j);
					ret = -EINVAL;
				}
			}
		} else {
			dev_err(&priv->client->dev,
					"Invalid reg_map elment count %d\n", rgb_map_count);
			ret = -EINVAL;
		}

		if (ret) {
			fwnode_handle_put(child);
			goto child_out;
		}

		led_cdev = &led->rgb_cdev.led_cdev;
		fwnode_property_read_string(child, "linux,default-trigger",
					&led_cdev->default_trigger);

		i++;
	}
	priv->num_of_leds = i;

	return 0;

child_out:
	return ret;
}

static int aw21036_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct aw21036 *led;
	int count;
	int ret;

	count = device_get_child_node_count(&client->dev);
	if (!count) {
		dev_err(&client->dev, "LEDs are not defined in device tree!");
		return -ENODEV;
	}

	led = devm_kzalloc(&client->dev,
			   sizeof(*led) + sizeof(*led->leds) * count,
			   GFP_KERNEL);
	if (!led) {
		devm_kfree(&client->dev, led);
		return -ENOMEM;
	}

	mutex_init(&led->lock);
	led->client = client;
	led->dev = &client->dev;
	i2c_set_clientdata(client, led);

	led->regmap = devm_regmap_init_i2c(client, &aw21036_regmap_config);

	if (IS_ERR(led->regmap)) {
		ret = PTR_ERR(led->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n", ret);
		devm_kfree(&client->dev, led);
		return ret;
	}

	ret = aw21036_probe_dt(led);
	if (ret) {
		devm_kfree(&client->dev, led);
		return ret;
	}

	aw21036_parse_ledcalibparams_from_dt(&client->dev);

	ret = device_create_file(&client->dev, &dev_attr_ledcalibenable);
	if (ret) {
		dev_err(&client->dev,
				"Could not create ledcalibenable sysfs entry\n");
		goto err_ledcalibenable;
	}
	ret = device_create_file(&client->dev, &dev_attr_ledpwmmaxlimit);
	if (ret) {
		dev_err(&client->dev,
				"Could not create ledpwmmaxlimit sysfs entry\n");
		goto err_ledpwmmaxlimit;
	}
	ret = device_create_file(&client->dev, &dev_attr_ledpwmscaling);
	if (ret) {
		dev_err(&client->dev,
				"Could not create ledpwmscaling sysfs entry\n");
		goto err_ledpwmscaling;
	}
	ret = device_create_file(&client->dev, &dev_attr_global_current);
	if (ret) {
		dev_err(&client->dev,
				"Could not create global_current sysfs entry\n");
		goto err_global_current;
	}
	ret = device_create_file(&client->dev, &dev_attr_whitebalance);
	if (ret) {
		dev_err(&client->dev,
				"Could not create whitebalance sysfs entry\n");
		goto err_whitebalance;
	}

	aw21036_initialize_led_calibrations(led);

	ret = aw21036_init(led);
	if (ret)
		return ret;

	return 0;

err_whitebalance:
	device_remove_file(&client->dev, &dev_attr_global_current);

err_global_current:
	device_remove_file(&client->dev, &dev_attr_ledpwmscaling);

err_ledpwmscaling:
	device_remove_file(&client->dev, &dev_attr_ledpwmmaxlimit);

err_ledpwmmaxlimit:
	device_remove_file(&client->dev, &dev_attr_ledcalibenable);

err_ledcalibenable:
	devm_kfree(&client->dev, led);
	return ret;

}

static void aw21036_led_rgb_deregister(struct aw21036 *priv)
{
	int i;
	struct aw21036_led *led;

	for (i = 0; i < priv->num_of_leds; i++) {
		led = &priv->leds[i];

		dev_dbg(&priv->client->dev, "deregister\n");
		led_classdev_rgb_unregister(&led->rgb_cdev);
	}
}

static int aw21036_disable(struct aw21036 *priv)
{
	int ret;

	ret = regmap_update_bits(priv->regmap, REG_GCR, AW21036_CHIP_ENABLE, 0);
	if (ret) {
		dev_err(&priv->client->dev, "Failed to enter standby\n");
		return ret;
	}

	if (priv->enable_gpio)
		gpiod_direction_output(priv->enable_gpio, 0);

	if (priv->regulator) {
		ret = regulator_disable(priv->regulator);
		if (ret)
			dev_err(&priv->client->dev,
				"Failed to disable regulator\n");
	}

	return ret;
}

static int aw21036_remove(struct i2c_client *client)
{
	struct aw21036 *led = i2c_get_clientdata(client);
	int ret;

	device_remove_file(&client->dev, &dev_attr_ledcalibenable);
	device_remove_file(&client->dev, &dev_attr_ledpwmmaxlimit);
	device_remove_file(&client->dev, &dev_attr_ledpwmscaling);
	device_remove_file(&client->dev, &dev_attr_global_current);
	device_remove_file(&client->dev, &dev_attr_whitebalance);

	aw21036_led_rgb_deregister(led);

	ret = aw21036_disable(led);

	mutex_destroy(&led->lock);

	devm_kfree(&client->dev, led);

	return ret;
}

static void aw21036_shutdown(struct i2c_client *client)
{
	struct aw21036 *priv = i2c_get_clientdata(client);

	aw21036_disable(priv);
}

static const struct i2c_device_id aw21036_id[] = {
	{ AW21036_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw21036_id);

static const struct of_device_id of_aw21036_leds_match[] = {
	{ .compatible = "aw,aw21036", },
	{},
};
MODULE_DEVICE_TABLE(of, of_aw21036_leds_match);

static struct i2c_driver aw21036_driver = {
	.driver = {
		.name	= AW21036_I2C_NAME,
		.of_match_table = of_aw21036_leds_match,
	},
	.probe		= aw21036_probe,
	.remove		= aw21036_remove,
	.shutdown	= aw21036_shutdown,
	.id_table	= aw21036_id,
};
module_i2c_driver(aw21036_driver);

MODULE_AUTHOR("Amazon.com");
MODULE_DESCRIPTION("AWINIC AW21036 LED Driver");
MODULE_LICENSE("GPL v2");
