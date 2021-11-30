/* LED FRAME class interface
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

#ifndef __LINUX_FRAME_LEDS_H_INCLUDED
#define __LINUX_FRAME_LEDS_H_INCLUDED

#include <linux/leds.h>

#define LED_FRAME_SYSFS_GROUPS 2

struct led_classdev_frame;

struct led_frame_ops {
	/* set frame */
	int (*set_frame)(struct led_classdev_frame *frameled_cdev);
	/* get frame */
	int (*get_frame)(struct led_classdev_frame *frameled_cdev, u32 *color);

	/*Optional: set brightness for whole frame */
	int (*set_frame_brightness)(struct led_classdev_frame *frameled_cdev,
				  enum led_brightness brightness);
	/*Optional: get brightness for whole frame */
	enum led_brightness (*get_frame_brightness)(struct led_classdev_frame *frameled_cdev);
};

struct led_classdev_frame {
	/* led class device */
	struct led_classdev led_cdev;

	/* frame led specific ops */
	struct led_frame_ops *ops;

	/* frame */
	uint8_t *frame;

	/* New requested frame */
	uint8_t *new_frame;

	/* Number of channels controlled by frame */
	int nr_channels;

	/* List/map of channels */
	uint8_t *channel_map;

	uint8_t frame_brightness;
	const struct attribute_group *sysfs_groups[LED_FRAME_SYSFS_GROUPS];

	/* Ensures consistent access */
	struct mutex lock;
};

static inline struct led_classdev_frame *lcdev_to_framecdev(
						struct led_classdev *lcdev)
{
	return container_of(lcdev, struct led_classdev_frame, led_cdev);
}

/**
 * led_classdev_frame_register - register a new object of led_classdev class
 *				 with support for frame LEDs
 * @parent: the LED device to register
 * @fled_cdev: the led_classdev_frame structure for this device
 *
 * Returns: 0 on success or negative error value on failure
 */
extern int led_classdev_frame_register(struct device *parent,
				struct led_classdev_frame *frameled_cdev);

/**
 * led_classdev_frame_unregister - unregisters an object of led_classdev class
 *				   with support for frame LEDs
 * @frameled_cdev: the LED device to unregister
 *
 * Unregister a previously registered via led_classdev_frame_register object
 */
extern void led_classdev_frame_unregister(struct led_classdev_frame *frameled_cdev);

#endif	/* __LINUX_FRAME_LEDS_H_INCLUDED */
