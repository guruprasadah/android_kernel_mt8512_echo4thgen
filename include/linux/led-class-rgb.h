// SPDX-License-Identifier: GPL-2.0
/* LED RGB class interface
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 */

#ifndef __LINUX_RGB_LEDS_H_INCLUDED
#define __LINUX_RGB_LEDS_H_INCLUDED

#include <linux/leds.h>

struct led_classdev_rgb;

#define LED_RGB_RED	BIT(0)
#define LED_RGB_GREEN	BIT(1)
#define LED_RGB_BLUE	BIT(2)
#define LED_RGB_WHITE	BIT(3)
#define LED_RGB_ALL	(LED_RGB_RED | LED_RGB_GREEN | LED_RGB_BLUE | \
			 LED_RGB_WHITE)

#define LED_RGB_SYSFS_GROUPS_SIZE	5

struct led_rgb_ops {
	/* set RGB color */
	int (*set_color)(struct led_classdev_rgb *rgbled_cdev);
	/* get RGB color */
	int (*get_color)(struct led_classdev_rgb *rgbled_cdev, u32 *color);

	/* set Red color */
	int (*set_red_brightness)(struct led_classdev_rgb *rgbled_cdev,
				  enum led_brightness brightness);
	enum led_brightness (*get_red_brightness)(struct led_classdev_rgb *rgbled_cdev);
	/* set green color */
	int (*set_green_brightness)(struct led_classdev_rgb *rgbled_cdev,
				  enum led_brightness brightness);
	enum led_brightness (*get_green_brightness)(struct led_classdev_rgb *rgbled_cdev);

	/* set blue color */
	int (*set_blue_brightness)(struct led_classdev_rgb *rgbled_cdev,
				  enum led_brightness brightness);
	enum led_brightness (*get_blue_brightness)(struct led_classdev_rgb *rgbled_cdev);

};

struct led_rgb_colors {
	u8 red;
	u8 green;
	u8 blue;
};

struct led_classdev_rgb {
	/* led class device */
	struct led_classdev led_cdev;

	/* rgb led specific ops */
	struct led_rgb_ops *ops;

	/* RGB colors to set intensity per LED */
	struct led_rgb_colors rgb_colors;

	const struct attribute_group *sysfs_groups[LED_RGB_SYSFS_GROUPS_SIZE];

	struct work_struct set_color_work;
};

static inline struct led_classdev_rgb *lcdev_to_rgbcdev(
						struct led_classdev *lcdev)
{
	return container_of(lcdev, struct led_classdev_rgb, led_cdev);
}

/**
 * led_classdev_rgb_register - register a new object of led_classdev class
 *				 with support for rgb LEDs
 * @parent: the rgb LED to register
 * @fled_cdev: the led_classdev_rgb structure for this device
 *
 * Returns: 0 on success or negative error value on failure
 */
extern int led_classdev_rgb_register(struct device *parent,
				struct led_classdev_rgb *rgbled_cdev);

/**
 * led_classdev_rgb_unregister - unregisters an object of led_classdev class
 *				   with support for rgb LEDs
 * @rgbled_cdev: the rgb LED to unregister
 *
 * Unregister a previously registered via led_classdev_rgb_register object
 */
extern void led_classdev_rgb_unregister(struct led_classdev_rgb *rgbled_cdev);

/**
 * led_rgb_set_color - set LED color
 * @rgbled_cdev: the RGB LED to set
 * @color: the color to set it to
 *
 * Set an RGB LED's color
 */
extern void led_rgb_set_color(struct led_classdev_rgb *rgbled_cdev,
			struct led_rgb_colors color);

#endif	/* __LINUX_RGB_LEDS_H_INCLUDED */
