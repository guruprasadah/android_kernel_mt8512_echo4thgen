/*
 * leds-is31fl3246.c
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

#define IS31FL3246_I2C_NAME "is31fl3246_led"

#define REG_CONTROL     0x00
#define REG_HF_PWM_BASE 0x01
#define REG_LF_PWM_BASE 0x49
#define REG_UPDATE      0x6D
#define REG_GCCR_G      0x6E
#define REG_GCCR_R      0x6F
#define REG_GCCR_B      0x70
#define REG_PDCPR       0x71
#define REG_RESET       0x7F

#define IS31FL3246_SW_RESET         0x00
#define IS31FL3246_UPDATE_TRIGGER   0x00
#define IS21FL3246_CTRL_SSD_SHIFT   0
#define IS21FL3246_CTRL_SSD_MASK    0x01
#define IS21FL3246_CTRL_PMS_SHIFT   1
#define IS21FL3246_CTRL_PMS_MASK    0x02
#define IS21FL3246_CTRL_HFPS_SHIFT  4
#define IS21FL3246_CTRL_HFPS_MASK   0x30
#define IS21FL3246_CTRL_RGBM_SHIFT  6
#define IS21FL3246_CTRL_RGBM_MASK   0x40
#define IS31FL3246_CHIP_ENABLE      BIT(0)
#define IS31FL3246_BR_BANK          BIT(6)
#define IS31FL3246_GCCR_DEFAULT     0xFF
#define IS31FL3246_STARTUP_BR       0xFF
#define IS31FL3246_HF_PWM_H_DEFAULT 0x00
#define IS21FL3246_PDCPR_HLS_SHIFT  6
#define IS21FL3246_PDCPR_HLS_MASK   0x40
#define IS21FL3246_PDCPR_PDE_SHIFT  7
#define IS21FL3246_PDCPR_PDE_MASK   0x80
#define IS21FL3246_PDCPR_PS_SHIFT   0
#define IS21FL3246_PDCPR_PS_MASK    0x3F

#define MAX_LED_STRINGS            12

#define NUM_LED_CALIB_PARAMS       3
#define BYTE_MASK                  0xFF
#define INDEX_LEDCALIBENABLE       0
#define INDEX_PWMSCALING           1
#define INDEX_PWMMAXLIMIT          2
#define LED_PWM_MAX_SCALING        0x7F
#define LED_PWM_SCALING_DEFAULT    0x7F7F7F
#define LED_PWM_MAX_LIMIT_DEFAULT  0xFFFFFF

#define LED_CALIB_COLOR_COUNT      3
#define LED_CALIB_RED_INDEX        0
#define LED_CALIB_GREEN_INDEX      1
#define LED_CALIB_BLUE_INDEX       2

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

struct is31fl3246_led {
	char label[LED_MAX_NAME_SIZE];
	struct led_classdev_rgb rgb_cdev;
	struct led_classdev_frame frame_cdev;
	struct is31fl3246 *priv;
	int led_number;
	u8 ctrl_bank_enabled;
	u8 rgb_map[e_rgb_map_idx_count];
	u8 led_type;
};

/**
 * struct is31fl3246 -
 * @enable_gpio: Hardware enable gpio
 * @regulator: LED supply regulator pointer
 * @client: Pointer to the I2C client
 * @regmap: Devices register map
 * @dev: Pointer to the devices device struct
 * @lock: Lock for reading/writing the device
 * @model_id: ID of the device
 * @leds: Array of LED strings
 */
struct is31fl3246 {
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
	struct is31fl3246_led leds[];
};

static const struct reg_default is31fl3246_reg_defs[] = {
	{REG_CONTROL, 0x00},
	{REG_HF_PWM_BASE, 0x00},
	{REG_LF_PWM_BASE, 0x00},
	{REG_UPDATE, IS31FL3246_UPDATE_TRIGGER},
	{REG_GCCR_G, 0x00},
	{REG_GCCR_R, 0x00},
	{REG_GCCR_B, 0x00},
	{REG_PDCPR, 0x00},
	{REG_RESET, 0x00},
};

static const struct regmap_config is31fl3246_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = REG_RESET,
	.reg_defaults = is31fl3246_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(is31fl3246_reg_defs),
	.cache_type = REGCACHE_RBTREE,
};

static struct is31fl3246_led *rgbled_cdev_to_led(struct led_classdev_rgb *rgbled_cdev)
{
	return container_of(rgbled_cdev, struct is31fl3246_led, rgb_cdev);
}

static struct is31fl3246_led *ledframe_cdev_to_led(struct led_classdev_frame *ledframe_cdev)
{
	return container_of(ledframe_cdev, struct is31fl3246_led, frame_cdev);
}

static ssize_t is31fl3246_reg_update(struct is31fl3246 *priv, int reg_addr,
				int input_val, int mask, int shift)
{
	ssize_t ret;
	int register_val;

	mutex_lock(&priv->lock);

	ret = regmap_read(priv->regmap, reg_addr, &register_val);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot read reg (0x%x).\n", reg_addr);
		mutex_unlock(&priv->lock);
		return ret;
	}
	register_val = (register_val & ~mask) | (input_val << shift);
	ret = regmap_write(priv->regmap, reg_addr, register_val);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write reg (0x%x)\n", reg_addr);

	mutex_unlock(&priv->lock);

	return ret;
}

static int is31fl3246_set_color(struct led_classdev_rgb *rgbled_cdev)
{
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct is31fl3246 *priv = led->priv;
	u8 bulk_write_offset = 0;
	u8 led_offset = 0;
	int ret, scaled_value;
	u8 colorArray[e_rgb_map_idx_count] = {0};

	led_offset = (led->led_number * 3);
	bulk_write_offset = REG_LF_PWM_BASE + led_offset;

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

	ret = regmap_bulk_write(priv->regmap, bulk_write_offset, colorArray, e_rgb_map_idx_count);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot write LED value\n");
		goto out;
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

out:
	mutex_unlock(&led->priv->lock);
	return ret;
}

static int is31fl3246_set_red(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct is31fl3246 *priv = led->priv;
	u8 led_offset;
	u8 red_reg;
	int ret, scaled_value;

	led_offset = (led->led_number * 3);
	red_reg = REG_LF_PWM_BASE + led_offset + led->rgb_map[e_rgb_map_idx_red];

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_RED_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_RED_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, red_reg, brightness);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot write LED value\n");
		goto out;
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

out:
	mutex_unlock(&led->priv->lock);
	return ret;
}

static int is31fl3246_set_green(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct is31fl3246 *priv = led->priv;
	u8 led_offset;
	u8 green_reg;
	int ret, scaled_value;

	led_offset = (led->led_number * 3);
	green_reg = REG_LF_PWM_BASE + led_offset + led->rgb_map[e_rgb_map_idx_green];

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_GREEN_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_GREEN_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, green_reg, brightness);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot write LED value\n");
		goto out;
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

out:
	mutex_unlock(&led->priv->lock);
	return ret;
}

static int is31fl3246_set_blue(struct led_classdev_rgb *rgbled_cdev,
				enum led_brightness brightness)
{
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);
	struct is31fl3246 *priv = led->priv;
	u8 led_offset;
	u8 blue_reg;
	int ret, scaled_value;

	led_offset = (led->led_number * 3);
	blue_reg = REG_LF_PWM_BASE + led_offset + led->rgb_map[e_rgb_map_idx_blue];

	if (priv->ledcalibenable) {
		scaled_value = (int) priv->ledpwmscalingrgb[LED_CALIB_BLUE_INDEX]
				* brightness / LED_PWM_MAX_SCALING;
		brightness = min_t(int, scaled_value,
				(int) priv->ledpwmmaxlimiterrgb[LED_CALIB_BLUE_INDEX]);
	}

	mutex_lock(&led->priv->lock);
	ret = regmap_write(priv->regmap, blue_reg, brightness);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot write LED value\n");
		goto out;
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

out:
	mutex_unlock(&led->priv->lock);
	return ret;
}

static int is31fl3246_brightness_set(struct is31fl3246_led *led,
				enum led_brightness brt_val)
{
	int ret = 0;
	u8 reg_val;

	mutex_lock(&led->priv->lock);

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		/* There is not a true full bank control for this driver, so
		   write all of the LEDs the same brightness value. Every other
		   register is the HFP_H + FMS settings, keep default value.*/
		u8 frame[NUM_LED_CHANNELS*2] = {0};
		int i;

		for (i = 0; i < NUM_LED_CHANNELS * 2; i = i+2) {
			frame[i] = brt_val;
			frame[i+1] = IS31FL3246_HF_PWM_H_DEFAULT;
		}
		ret = regmap_bulk_write(led->priv->regmap, REG_HF_PWM_BASE,
					frame, NUM_LED_CHANNELS * 2);
		if (ret)
			dev_err(&led->priv->client->dev, "Cannot write brightness register\n");
	}
	else {
		/* led_number * 2 because HF PWM is every other register is HFP_L */
		reg_val = REG_HF_PWM_BASE + led->led_number * 2;

		ret = regmap_write(led->priv->regmap, reg_val, brt_val);
		if (ret)
			dev_err(&led->priv->client->dev, "Cannot write brightness register\n");
	}

	ret = regmap_write(led->priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&led->priv->client->dev, "Cannot write update register\n");

	mutex_unlock(&led->priv->lock);

	return ret;
}

static int is31fl3246_rgb_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brt_val)
{
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);

	return is31fl3246_brightness_set(led, brt_val);
}

static int is31fl3246_frame_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brt_val)
{
	struct led_classdev_frame *ledframe_cdev = lcdev_to_framecdev(led_cdev);
	struct is31fl3246_led *led = ledframe_cdev_to_led(ledframe_cdev);

	return is31fl3246_brightness_set(led, brt_val);
}

static enum led_brightness is31fl3246_brightness_get(struct is31fl3246_led *led)
{
	unsigned int brt_val = 0;
	u8 reg_val;
	int ret;

	mutex_lock(&led->priv->lock);

	if (led->ctrl_bank_enabled == e_ctrl_bank_full) {
		/* There is not a true full bank control for this driver,
		   so read all of the LED brightness values. Values should all
		   be the same, but print warn if they are not. Every other
		   reg is the HFP_H + FMS settings, so skip those values.*/
		u8 frame[NUM_LED_CHANNELS*2] = {0};
		int i;
		int avg_brt = 0;

		ret = regmap_bulk_read(led->priv->regmap, REG_HF_PWM_BASE, frame, NUM_LED_CHANNELS * 2);

		for (i = 0; i < NUM_LED_CHANNELS * 2; i = i+2) {
			avg_brt += frame[i];
			if ( i > 2 && frame[i] != frame[i-2]) {
				dev_warn(&led->priv->client->dev, "Brightness doesn't match for %i brt_val=%d\n", i, frame[i]);
			}
		}
		brt_val = avg_brt / NUM_LED_CHANNELS;
	} else {
		/* led_number * 2 because HF PWM is every other register is HFP_L */
		reg_val = REG_HF_PWM_BASE + led->led_number * 2;
		ret = regmap_read(led->priv->regmap, reg_val, &brt_val);
	}

	mutex_unlock(&led->priv->lock);

	return brt_val;
}

static enum led_brightness is31fl3246_rgb_brightness_get(struct led_classdev *led_cdev)
{
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct is31fl3246_led *led = rgbled_cdev_to_led(rgbled_cdev);

	return is31fl3246_brightness_get(led);
}

static enum led_brightness is31fl3246_frame_brightness_get(struct led_classdev *led_cdev)
{
	struct led_classdev_frame *ledframe_cdev = lcdev_to_framecdev(led_cdev);
	struct is31fl3246_led *led = ledframe_cdev_to_led(ledframe_cdev);

	return is31fl3246_brightness_get(led);
}

static int is31fl3246_set_frame(struct led_classdev_frame *ledframe_cdev)
{
	struct is31fl3246_led *led = ledframe_cdev_to_led(ledframe_cdev);
	struct is31fl3246 *priv = led->priv;
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
	ret = regmap_bulk_write(priv->regmap, REG_LF_PWM_BASE, frame, NUM_LED_CHANNELS);
	if (ret) {
		dev_err(&priv->client->dev, "Cannot update LED Frame\n");
	}

	ret = regmap_write(led->priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&led->priv->client->dev, "Cannot write update register\n");

	mutex_unlock(&led->priv->lock);

	return ret;
}

static struct led_frame_ops is31fl3246_frame_ops = {
	.set_frame =  is31fl3246_set_frame,
};

static int is31fl3246_enable(struct is31fl3246 *priv)
{
	int ret;
	int hf_reg;
	if (priv->regulator) {
		ret = regulator_enable(priv->regulator);
		if (ret) {
			dev_err(&priv->client->dev, "Failed to enable regulator\n");
			return ret;
		}
	}

	if (priv->enable_gpio) {
		gpiod_direction_output(priv->enable_gpio, 1);
		/* tEN_H = 500 us*/
		usleep_range(500, 700);
	} else {
		ret = regmap_write(priv->regmap, REG_RESET, IS31FL3246_SW_RESET);
		if (ret) {
			dev_err(&priv->client->dev,
				"Cannot reset the device\n");
			return ret;
		}
	}

	/* Enable chip as 8-bit with RGB group controlled brightness */
	ret = regmap_write(priv->regmap, REG_CONTROL, IS31FL3246_CHIP_ENABLE | IS31FL3246_BR_BANK);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write ctrl enable\n");

	ret = regmap_write(priv->regmap, REG_GCCR_G, IS31FL3246_GCCR_DEFAULT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write global current control green\n");
	ret = regmap_write(priv->regmap, REG_GCCR_R, IS31FL3246_GCCR_DEFAULT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write global current control red\n");
	ret = regmap_write(priv->regmap, REG_GCCR_B, IS31FL3246_GCCR_DEFAULT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write global current control blue\n");

	/* Set frequency mode select to be HFP + LFP */
	for (hf_reg = REG_HF_PWM_BASE + 1; hf_reg < REG_LF_PWM_BASE; hf_reg += 2) {
		ret = regmap_write(priv->regmap, hf_reg, IS31FL3246_HF_PWM_H_DEFAULT);
		if (ret)
			dev_err(&priv->client->dev, "Cannot write HF mode reg %d\n", hf_reg);
	}

	ret = regmap_write(priv->regmap, REG_UPDATE, IS31FL3246_UPDATE_TRIGGER);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write update register\n");

	return ret;
}

static struct led_rgb_ops is31fl3246_rgb_ops = {
	.set_color = is31fl3246_set_color,
	.set_red_brightness = is31fl3246_set_red,
	.set_green_brightness = is31fl3246_set_green,
	.set_blue_brightness = is31fl3246_set_blue,
};

static int is31fl3246_init(struct is31fl3246 *priv)
{
	int i;
	int ret;
	struct is31fl3246_led *led;
	struct led_classdev *led_cdev;

	ret = is31fl3246_enable(priv);
	if (ret)
		return ret;

	for (i = 0; i < priv->num_of_leds; i++) {
		led = &priv->leds[i];

		led->priv = priv;

		if (led->led_type == e_led_frame) {
			led->frame_cdev.ops = &is31fl3246_frame_ops;
			led_cdev = &led->frame_cdev.led_cdev;
			led_cdev->name = led->label;
			led_cdev->brightness_set_blocking = is31fl3246_frame_brightness_set;
			led_cdev->brightness_get = is31fl3246_frame_brightness_get;
			ret = led_classdev_frame_register(&priv->client->dev, &led->frame_cdev);
			if (ret) {
				dev_err(&priv->client->dev, "led frame register err: %d\n", ret);
				while (--i >= 0) {
					led = &priv->leds[i];
					led_classdev_frame_unregister(&led->frame_cdev);
				}
				return ret;
			}

			led->ctrl_bank_enabled = e_ctrl_bank_full;
			is31fl3246_brightness_set(led, IS31FL3246_STARTUP_BR);

			continue;
		}

		led->rgb_cdev.ops = &is31fl3246_rgb_ops;
		led_cdev = &led->rgb_cdev.led_cdev;
		led_cdev->name = led->label;
		led_cdev->brightness_set_blocking = is31fl3246_rgb_brightness_set;
		led_cdev->brightness_get = is31fl3246_rgb_brightness_get;

		ret = led_classdev_rgb_register(&priv->client->dev,
						 &led->rgb_cdev);
		if (ret) {
			dev_err(&priv->client->dev, "led register err: %d\n",
				ret);
			while (--i >= 0) {
				led = &priv->leds[i];
				led_classdev_rgb_unregister(&led->rgb_cdev);
			}
			break;
		}
		led->ctrl_bank_enabled = e_ctrl_bank_brightness;
		is31fl3246_brightness_set(led, IS31FL3246_STARTUP_BR);
	}

	return ret;
}

static void is31fl3246_convert_to_rgb(u8 *rgb_array, int bitmap)
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

static void is31fl3246_initialize_led_calibrations(struct is31fl3246 *priv)
{
	priv->ledcalibenable = ledcalibparams[INDEX_LEDCALIBENABLE];

	if (priv->ledcalibenable) {
		is31fl3246_convert_to_rgb(priv->ledpwmscalingrgb,
					ledcalibparams[INDEX_PWMSCALING]);
		is31fl3246_convert_to_rgb(priv->ledpwmmaxlimiterrgb,
					ledcalibparams[INDEX_PWMMAXLIMIT]);
	} else {
		is31fl3246_convert_to_rgb(priv->ledpwmscalingrgb,
					LED_PWM_SCALING_DEFAULT);
		is31fl3246_convert_to_rgb(priv->ledpwmmaxlimiterrgb,
					LED_PWM_MAX_LIMIT_DEFAULT);
	}
}

static ssize_t phase_register_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int pdcpr_value;

	mutex_lock(&priv->lock);

	ret = regmap_read(priv->regmap, REG_PDCPR, &pdcpr_value);
	if (ret) {
		dev_err(dev, "Cannot read REG_PDCPR\n");
		mutex_unlock(&priv->lock);
		return ret;
	}

	mutex_unlock(&priv->lock);

	ret = sprintf(buf, "0x%x HLS(%i) PDE(%i)  PS(%i)\n", pdcpr_value,
			(pdcpr_value & IS21FL3246_PDCPR_HLS_MASK) >> IS21FL3246_PDCPR_HLS_SHIFT,
			(pdcpr_value & IS21FL3246_PDCPR_PDE_MASK) >> IS21FL3246_PDCPR_PDE_SHIFT,
			(pdcpr_value & IS21FL3246_PDCPR_PS_MASK) >> IS21FL3246_PDCPR_PS_SHIFT);

	return ret;
}

static DEVICE_ATTR_RO(phase_register);

static ssize_t phase_hls_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to parse group phase delay input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 1) {
		dev_err(dev, "is31fl3246: Invalid input (%d). HLS can only be 0 or 1\n", input_val);
		ret = -EINVAL;
		return ret;
	}
	ret = is31fl3246_reg_update(priv, REG_PDCPR, input_val,
			IS21FL3246_PDCPR_HLS_MASK, IS21FL3246_PDCPR_HLS_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_PDCPR reg.\n");

	return len;
}

static DEVICE_ATTR_WO(phase_hls);

static ssize_t phase_pde_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to parse phase delay enable input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 1) {
		dev_err(dev, "is31fl3246: Invalid input (%d). Enable can only be 0 or 1\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_PDCPR, input_val,
			IS21FL3246_PDCPR_PDE_MASK, IS21FL3246_PDCPR_PDE_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_PDCPR reg.\n");

	return len;
}

static DEVICE_ATTR_WO(phase_pde);

static ssize_t phase_clock_ps_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to parse clock phase select input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 63) {
		dev_err(dev, "is31fl3246: Invalid input (%d). Enable can only be 0 - 63\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_PDCPR, input_val,
			IS21FL3246_PDCPR_PS_MASK, IS21FL3246_PDCPR_PS_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_PDCPR reg.\n");

	return len;
}

static DEVICE_ATTR_WO(phase_clock_ps);

static ssize_t control_register_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int ctrl_value;

	mutex_lock(&priv->lock);

	ret = regmap_read(priv->regmap, REG_CONTROL, &ctrl_value);
	if (ret) {
		dev_err(dev, "Cannot read REG_CONTROL\n");
		mutex_unlock(&priv->lock);
		return ret;
	}

	mutex_unlock(&priv->lock);

	ret = sprintf(buf, "0x%x SSD(%i) PMS(%i)  HFPS(%i) RGBM(%i)\n", ctrl_value,
			(ctrl_value & IS21FL3246_CTRL_SSD_MASK) >> IS21FL3246_CTRL_SSD_SHIFT,
			(ctrl_value & IS21FL3246_CTRL_PMS_MASK) >> IS21FL3246_CTRL_PMS_SHIFT,
			(ctrl_value & IS21FL3246_CTRL_HFPS_MASK) >> IS21FL3246_CTRL_HFPS_SHIFT,
			(ctrl_value & IS21FL3246_CTRL_RGBM_MASK) >> IS21FL3246_CTRL_RGBM_SHIFT);

	return ret;
}

static DEVICE_ATTR_RO(control_register);

static ssize_t control_ssd_en_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to parse software shutdown enable input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 1) {
		dev_err(dev, "is31fl3246: Invalid input (%d). Enable can only be 0 or 1\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_CONTROL, input_val,
			IS21FL3246_CTRL_SSD_MASK, IS21FL3246_CTRL_SSD_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_CONTROL reg.\n");

	return len;
}

static DEVICE_ATTR_WO(control_ssd_en);

static ssize_t control_pms_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to parse high frequency pwm resolution input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 1) {
		dev_err(dev, "is31fl3246: Invalid input (%d). PMS can only be 0 or 1\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_CONTROL, input_val,
			IS21FL3246_CTRL_PMS_MASK, IS21FL3246_CTRL_PMS_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_CONTROL reg.\n");

	return len;
}

static DEVICE_ATTR_WO(control_pms);

static ssize_t control_hfps_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to high frequency pwm select input\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 3) {
		dev_err(dev, "is31fl3246: Invalid input (%d). HFPS can only be 0 - 3\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_CONTROL, input_val,
			IS21FL3246_CTRL_HFPS_MASK, IS21FL3246_CTRL_HFPS_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_CONTROL reg.\n");

	return len;
}

static DEVICE_ATTR_WO(control_hfps);

static ssize_t control_rgbm_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int input_val;

	if (sscanf(buf, "%d", &input_val) != 1) {
		dev_err(dev, "is31fl3246: unable to RGB mode select\n");
		ret = -EINVAL;
		return ret;
	}

	if (input_val < 0 || input_val > 1) {
		dev_err(dev, "is31fl3246: Invalid input (%d). RGBM can only be 0 or 1\n", input_val);
		ret = -EINVAL;
		return ret;
	}

	ret = is31fl3246_reg_update(priv, REG_CONTROL, input_val,
			IS21FL3246_CTRL_RGBM_MASK, IS21FL3246_CTRL_RGBM_SHIFT);
	if (ret)
		dev_err(&priv->client->dev, "Cannot update REG_CONTROL reg.\n");

	return len;
}

static DEVICE_ATTR_WO(control_rgbm);

static ssize_t global_current_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	int red, green, blue;
	u8 gccr_array[e_rgb_map_idx_count] = {0};

	if (sscanf(buf, "%d %d %d", &red, &green, &blue) != 3) {
		dev_err(dev, "is31fl3246: unable to parse whitebalance input\n");
		ret = -EINVAL;
		return ret;
	}

	if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
		dev_err(dev, "is31fl3246: Invalid input to store white balance.\n");
		ret = -EINVAL;
		return ret;
	}

	gccr_array[0] = green;
	gccr_array[1] = red;
	gccr_array[2] = blue;

	mutex_lock(&priv->lock);

	ret = regmap_bulk_write(priv->regmap, REG_GCCR_G, gccr_array,
				e_rgb_map_idx_count);
	if (ret)
		dev_err(&priv->client->dev, "Cannot write GCCR_G/R/B regs\n");

	mutex_unlock(&priv->lock);

	return len;
}

static ssize_t global_current_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));
	ssize_t ret;
	u8 gccr_array[e_rgb_map_idx_count] = {0};

	mutex_lock(&priv->lock);

	ret = regmap_bulk_read(priv->regmap, REG_GCCR_G, gccr_array,
						   e_rgb_map_idx_count);
	if (ret) {
		dev_err(dev, "Cannot bulk read REG_WBR\n");
	}

	mutex_unlock(&priv->lock);

	/* The driver stores in green, red, blue order but print in RGB */
	ret = sprintf(buf, "%d %d %d\n", gccr_array[1], gccr_array[0],
			gccr_array[2]);

	return ret;
}

static DEVICE_ATTR_RW(global_current);

static ssize_t ledcalibenable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	int ret, temp;
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

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
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

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
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		dev_err(dev,
			"Invalid input for ledpwmscaling_store, ret=%d\n",
			ret);
		return ret;
	}

	is31fl3246_convert_to_rgb(priv->ledpwmscalingrgb, temp);

	dev_dbg(&priv->client->dev, "ledpwmscaling = %x\n", temp);

	return len;
}

static ssize_t ledpwmscaling_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int ret = 0, i;
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

	for (i = 0; i < e_rgb_map_idx_count; i++)
		ret += sprintf(buf + ret, "%x",
				priv->ledpwmscalingrgb[i]);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmscaling);

static ssize_t ledpwmmaxlimit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t len)
{
	int ret, temp;
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		dev_err(dev,
			"Invalid input for ledpwmmaxlimit_store, ret=%d\n",
			ret);
		return ret;
	}

	is31fl3246_convert_to_rgb(priv->ledpwmmaxlimiterrgb, temp);

	return len;
}

static ssize_t ledpwmmaxlimit_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int ret = 0, i;
	struct is31fl3246 *priv = i2c_get_clientdata(to_i2c_client(dev));

	for (i = 0; i < e_rgb_map_idx_count; i++)
		ret += sprintf(buf + ret, "%x",
				priv->ledpwmmaxlimiterrgb[i]);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmmaxlimit);

static void is31fl3246_parse_ledcalibparams_from_dt(struct device *dev)
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

static int parse_frame_from_dt(struct device *dev, struct device_node *node, struct is31fl3246_led *led)
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

static int is31fl3246_probe_dt(struct is31fl3246 *priv)
{
	struct fwnode_handle *child = NULL;
	struct led_classdev *led_cdev;
	struct is31fl3246_led *led;
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
		ret = fwnode_property_read_u32(child, "issi,led-module", &led_number);
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

static int is31fl3246_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct is31fl3246 *led;
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

	led->regmap = devm_regmap_init_i2c(client, &is31fl3246_regmap_config);

	if (IS_ERR(led->regmap)) {
		ret = PTR_ERR(led->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n", ret);
		devm_kfree(&client->dev, led);
		return ret;
	}

	ret = is31fl3246_probe_dt(led);
	if (ret) {
		devm_kfree(&client->dev, led);
		return ret;
	}

	is31fl3246_parse_ledcalibparams_from_dt(&client->dev);

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
	ret = device_create_file(&client->dev, &dev_attr_control_register);
	if (ret) {
		dev_err(&client->dev,
				"Could not create control_register sysfs entry\n");
		goto err_control_register;
	}
	ret = device_create_file(&client->dev, &dev_attr_control_ssd_en);
	if (ret) {
		dev_err(&client->dev,
				"Could not create control_ssd_en sysfs entry\n");
		goto err_control_ssd_en;
	}
	ret = device_create_file(&client->dev, &dev_attr_control_pms);
	if (ret) {
		dev_err(&client->dev,
				"Could not create control_pms sysfs entry\n");
		goto err_control_pms;
	}
	ret = device_create_file(&client->dev, &dev_attr_control_hfps);
	if (ret) {
		dev_err(&client->dev,
				"Could not create control_hfps sysfs entry\n");
		goto err_control_hfps;
	}
	ret = device_create_file(&client->dev, &dev_attr_control_rgbm);
	if (ret) {
		dev_err(&client->dev,
				"Could not create control_rgbm sysfs entry\n");
		goto err_control_rgbm;
	}
	ret = device_create_file(&client->dev, &dev_attr_phase_register);
	if (ret) {
		dev_err(&client->dev,
				"Could not create phase_register sysfs entry\n");
		goto err_phase_register;
	}
	ret = device_create_file(&client->dev, &dev_attr_phase_hls);
	if (ret) {
		dev_err(&client->dev,
				"Could not create phase_hls sysfs entry\n");
		goto err_phase_hls;
	}
	ret = device_create_file(&client->dev, &dev_attr_phase_pde);
	if (ret) {
		dev_err(&client->dev,
				"Could not create phase_pde sysfs entry\n");
		goto err_phase_pde;
	}
	ret = device_create_file(&client->dev, &dev_attr_phase_clock_ps);
	if (ret) {
		dev_err(&client->dev,
				"Could not create phase_clock_ps sysfs entry\n");
		goto err_phase_clock_ps;
	}

	is31fl3246_initialize_led_calibrations(led);

	ret = is31fl3246_init(led);
	if (ret) {
		devm_kfree(&client->dev, led);
		return ret;
	}

	return 0;

err_phase_clock_ps:
	device_remove_file(&client->dev, &dev_attr_phase_pde);

err_phase_pde:
	device_remove_file(&client->dev, &dev_attr_phase_hls);

err_phase_hls:
	device_remove_file(&client->dev, &dev_attr_phase_register);

err_phase_register:
	device_remove_file(&client->dev, &dev_attr_control_rgbm);

err_control_rgbm:
	device_remove_file(&client->dev, &dev_attr_control_hfps);

err_control_hfps:
	device_remove_file(&client->dev, &dev_attr_control_pms);

err_control_pms:
	device_remove_file(&client->dev, &dev_attr_control_ssd_en);

err_control_ssd_en:
	device_remove_file(&client->dev, &dev_attr_control_register);

err_control_register:
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

static void is31fl3246_led_rgb_deregister(struct is31fl3246 *priv)
{
	int i;
	struct is31fl3246_led *led;

	for (i = 0; i < priv->num_of_leds; i++) {
		led = &priv->leds[i];

		dev_dbg(&priv->client->dev, "deregister\n");
		led_classdev_rgb_unregister(&led->rgb_cdev);
	}
}

static int is31fl3246_disable(struct is31fl3246 *priv)
{
	int ret;

	ret = regmap_update_bits(priv->regmap, REG_CONTROL, IS31FL3246_CHIP_ENABLE, 0);
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

static int is31fl3246_remove(struct i2c_client *client)
{
	struct is31fl3246 *led = i2c_get_clientdata(client);
	int ret;

	device_remove_file(&client->dev, &dev_attr_ledcalibenable);
	device_remove_file(&client->dev, &dev_attr_ledpwmmaxlimit);
	device_remove_file(&client->dev, &dev_attr_ledpwmscaling);
	device_remove_file(&client->dev, &dev_attr_global_current);
	device_remove_file(&client->dev, &dev_attr_control_register);
	device_remove_file(&client->dev, &dev_attr_control_ssd_en);
	device_remove_file(&client->dev, &dev_attr_control_pms);
	device_remove_file(&client->dev, &dev_attr_control_hfps);
	device_remove_file(&client->dev, &dev_attr_control_rgbm);
	device_remove_file(&client->dev, &dev_attr_phase_register);
	device_remove_file(&client->dev, &dev_attr_phase_hls);
	device_remove_file(&client->dev, &dev_attr_phase_pde);
	device_remove_file(&client->dev, &dev_attr_phase_clock_ps);


	is31fl3246_led_rgb_deregister(led);

	ret = is31fl3246_disable(led);

	mutex_destroy(&led->lock);

	devm_kfree(&client->dev, led);

	return ret;
}

static void is31fl3246_shutdown(struct i2c_client *client)
{
	struct is31fl3246 *priv = i2c_get_clientdata(client);

	is31fl3246_disable(priv);
}

static const struct i2c_device_id is31fl3246_id[] = {
	{ IS31FL3246_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, is31fl3246_id);

static const struct of_device_id of_is31fl3246_leds_match[] = {
	{ .compatible = "issi,is31fl3246", },
	{},
};
MODULE_DEVICE_TABLE(of, of_is31fl3246_leds_match);

static struct i2c_driver is31fl3246_driver = {
	.driver = {
		.name	= IS31FL3246_I2C_NAME,
		.of_match_table = of_is31fl3246_leds_match,
	},
	.probe		= is31fl3246_probe,
	.remove		= is31fl3246_remove,
	.shutdown	= is31fl3246_shutdown,
	.id_table	= is31fl3246_id,
};
module_i2c_driver(is31fl3246_driver);

MODULE_AUTHOR("Amazon.com");
MODULE_DESCRIPTION("Lumisil IS31FL3246 LED Driver");
MODULE_LICENSE("GPL");
