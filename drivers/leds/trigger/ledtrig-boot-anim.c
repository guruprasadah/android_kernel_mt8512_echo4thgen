/*
 * ledtrig-boot-anim.c
 *
 * Copyright (c) 2019 Amazon.com, Inc. or its affiliates. All Rights
 * Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/led-class-rgb.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/pm.h>

#define FRAME_DEFAULT_DELAY_MS		88
#define RGB_COLOR_CYAN			0x00FFFF
#define RGB_COLOR_BLUE			0x0000FF
#define ACTIVATE_DELAY_MS		10
#define NUM_FRAMES			12

enum {
	INDEX_TRIG_DELAY_MS = 0,
	INDEX_TRIG_FORGROUND_COLOR,
	INDEX_TRIG_BACKGROUND_COLOR,
	INDEX_TRIG_ANIMATION_TYPE,
	NUM_TRIG_PATTERN_PARAMS
};

enum {
	BOOT_ANIMATION_RADIAL_LED = 0,
	BOOT_ANIMATION_SINGLE_LED
};

static int trigpattern[NUM_TRIG_PATTERN_PARAMS] = {FRAME_DEFAULT_DELAY_MS,
		RGB_COLOR_CYAN, RGB_COLOR_BLUE, BOOT_ANIMATION_RADIAL_LED};
static int trig_pattern_count = NUM_TRIG_PATTERN_PARAMS;
MODULE_PARM_DESC(trigpattern, "trigpattern=<frame_delay_ms>, 0xRRGGBB(foreground color), 0xRRGGBB(background color), <boot_animation_type>");
module_param_array(trigpattern, int, &trig_pattern_count, S_IRUGO);

struct bootanim_trig_data {
	u8 enable_all_foreground_led;
	u16 curr_foreground_led;
	atomic_t led_count;
	u32 frame_delay_jiffies;
	struct led_rgb_colors color_foreground;
	struct led_rgb_colors color_background;
	struct timer_list timer;
	struct wakeup_source ws;
};

static struct bootanim_trig_data bootanim_trig_dev;

static void ledtrig_radial_led_bootanim_function(unsigned long data);

static void ledtrig_single_led_bootanim_function(unsigned long data);

static void bootanim_trig_radial_led_update_patterns(
		struct bootanim_trig_data *data)
{
	if (data->curr_foreground_led >= atomic_read(&data->led_count) - 1)
		data->curr_foreground_led = 0;
	else
		data->curr_foreground_led++;
}

static void bootanim_trig_activate(struct led_classdev *led_cdev)
{
	led_cdev->trigger_data = &bootanim_trig_dev;
	led_cdev->activated = true;
	/* Init the timer when activated first time */
	if (atomic_inc_return(&bootanim_trig_dev.led_count) == 1) {
		__pm_stay_awake(&bootanim_trig_dev.ws);

		if (trigpattern[INDEX_TRIG_ANIMATION_TYPE] ==
				BOOT_ANIMATION_RADIAL_LED)
			setup_timer(&bootanim_trig_dev.timer,
				ledtrig_radial_led_bootanim_function, 0);
		else
			setup_timer(&bootanim_trig_dev.timer,
				ledtrig_single_led_bootanim_function, 0);
	}

	/*
	 * Keep delaying the timer for ACTIVATE_DELAY_MS until no new trigger is activated.
	 * So we start boot animation when last trigger is activated + ACTIVATE_DELAY_MS
	 */
	del_timer(&bootanim_trig_dev.timer);
	mod_timer(&bootanim_trig_dev.timer, jiffies + msecs_to_jiffies(ACTIVATE_DELAY_MS));
}

static void bootanim_trig_deactivate(struct led_classdev *led_cdev)
{
	if (led_cdev->activated) {
		led_cdev->activated = false;
		if (atomic_dec_and_test(&bootanim_trig_dev.led_count)) {
			del_timer_sync(&bootanim_trig_dev.timer);
			__pm_relax(&bootanim_trig_dev.ws);
		}
	}
}

static struct led_trigger bootanim_trigger = {
	.name     = "boot-anim",
	.activate = bootanim_trig_activate,
	.deactivate = bootanim_trig_deactivate,
};

static void ledtrig_single_led_bootanim_function(unsigned long data)
{
	struct led_classdev_rgb *rgbled_cdev;
	struct led_classdev *led_cdev;
	struct led_rgb_colors color;
	u32 delay;

	if (bootanim_trig_dev.enable_all_foreground_led)
		color = bootanim_trig_dev.color_foreground;
	else
		color = bootanim_trig_dev.color_background;

	read_lock(&bootanim_trigger.leddev_list_lock);
	list_for_each_entry(led_cdev, &bootanim_trigger.led_cdevs, trig_list) {
		rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
		if (led_cdev->activated)
			led_rgb_set_color(rgbled_cdev, color);
	}
	read_unlock(&bootanim_trigger.leddev_list_lock);

	bootanim_trig_dev.enable_all_foreground_led ^= 1;

	if (bootanim_trig_dev.enable_all_foreground_led)
		delay = msecs_to_jiffies(trigpattern[INDEX_TRIG_DELAY_MS] *
					 (NUM_FRAMES - 1));
	else
		delay = bootanim_trig_dev.frame_delay_jiffies;
	mod_timer(&bootanim_trig_dev.timer, jiffies + delay);
}

static void ledtrig_radial_led_bootanim_function(unsigned long data)
{
	struct led_classdev_rgb *rgbled_cdev;
	struct led_classdev *led_cdev;
	struct led_rgb_colors color;
	int i = 0;

	read_lock(&bootanim_trigger.leddev_list_lock);
	list_for_each_entry(led_cdev, &bootanim_trigger.led_cdevs, trig_list) {
		rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
		if (i == bootanim_trig_dev.curr_foreground_led)
			color = bootanim_trig_dev.color_foreground;
		else
			color = bootanim_trig_dev.color_background;
		i++;
		if (led_cdev->activated)
			led_rgb_set_color(rgbled_cdev, color);
	}
	read_unlock(&bootanim_trigger.leddev_list_lock);

	bootanim_trig_radial_led_update_patterns(&bootanim_trig_dev);

	/* Only restart timer when led_count > 0 */
	if (atomic_read(&bootanim_trig_dev.led_count) > 0)
		mod_timer(&bootanim_trig_dev.timer,
			jiffies + bootanim_trig_dev.frame_delay_jiffies);
}

/* Convert bitmap_rgb(0xRRGGBB) to led_rgb_colors */
static void bootanim_trig_convert_to_rgb(struct led_rgb_colors *rgbcolor,
					int bitmap_rgb)
{
	rgbcolor->red = (bitmap_rgb & (0xFF << 16)) >> 16;
	rgbcolor->green = (bitmap_rgb & (0xFF << 8)) >> 8;
	rgbcolor->blue = bitmap_rgb & 0xFF;
}

static int __init bootanim_trigger_init(void)
{
	bootanim_trig_dev.frame_delay_jiffies = msecs_to_jiffies(trigpattern[INDEX_TRIG_DELAY_MS]);

	bootanim_trig_convert_to_rgb(&bootanim_trig_dev.color_foreground,
					trigpattern[INDEX_TRIG_FORGROUND_COLOR]);

	bootanim_trig_convert_to_rgb(&bootanim_trig_dev.color_background,
					trigpattern[INDEX_TRIG_BACKGROUND_COLOR]);

	atomic_set(&bootanim_trig_dev.led_count, 0);

	wakeup_source_init(&bootanim_trig_dev.ws, "boot-anim");

	return led_trigger_register(&bootanim_trigger);
}
module_init(bootanim_trigger_init);

static void __exit bootanim_trigger_exit(void)
{
	wakeup_source_trash(&bootanim_trig_dev.ws);
	led_trigger_unregister(&bootanim_trigger);
}
module_exit(bootanim_trigger_exit);

MODULE_DESCRIPTION("RGB LED boot animation triggers");
MODULE_AUTHOR("Amazon");
MODULE_LICENSE("GPL");
