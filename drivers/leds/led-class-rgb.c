// SPDX-License-Identifier: GPL-2.0
/* LED RGB class interface
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/led-class-rgb.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "leds.h"

static ssize_t color_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	const struct led_rgb_ops *ops = rgbled_cdev->ops;
	int red, green, blue;
	ssize_t ret = -EINVAL;

	mutex_lock(&led_cdev->led_access);

	if (led_sysfs_is_disabled(led_cdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	if (sscanf(buf, "%d %d %d", &red, &green, &blue) != 3) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	/* Should these values be retainable if the ops fails should the old
	 * values be restored?
	 */
	colors->red = red;
	colors->green = green;
	colors->blue = blue;

	ret = ops->set_color(rgbled_cdev);
	if (ret < 0)
		goto unlock;

	ret = size;
unlock:
	mutex_unlock(&led_cdev->led_access);
	return ret;
}

static ssize_t color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	u8 red, green, blue;

	red = rgbled_cdev->rgb_colors.red;
	green = rgbled_cdev->rgb_colors.green;
	blue = rgbled_cdev->rgb_colors.blue;

	return sprintf(buf, "%d %d %d\n", red, green, blue);
}
static DEVICE_ATTR_RW(color);

static struct attribute *led_set_color_attrs[] = {
	&dev_attr_color.attr,
	NULL,
};

static const struct attribute_group led_color_group = {
	.attrs = led_set_color_attrs,
};

static ssize_t red_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	const struct led_rgb_ops *ops = rgbled_cdev->ops;
	unsigned long state;
	ssize_t ret = -EINVAL;

	mutex_lock(&led_cdev->led_access);

	if (led_sysfs_is_disabled(led_cdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		goto unlock;

	if (state > LED_FULL) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = ops->set_red_brightness(rgbled_cdev, state);
	if (ret < 0)
		goto unlock;

	colors->red = state;

	ret = size;
unlock:
	mutex_unlock(&led_cdev->led_access);
	return ret;
}

static ssize_t red_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;

	if (rgbled_cdev->ops->get_red_brightness)
		colors->red = rgbled_cdev->ops->get_red_brightness(rgbled_cdev);

	return sprintf(buf, "%d\n", colors->red);
}
static DEVICE_ATTR_RW(red);

static struct attribute *led_color_red_attrs[] = {
	&dev_attr_red.attr,
	NULL,
};

static const struct attribute_group led_set_red_group = {
	.attrs = led_color_red_attrs,
};
static ssize_t green_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	const struct led_rgb_ops *ops = rgbled_cdev->ops;
	unsigned long state;
	ssize_t ret = -EINVAL;

	mutex_lock(&led_cdev->led_access);

	if (led_sysfs_is_disabled(led_cdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		goto unlock;

	if (state > LED_FULL) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = ops->set_green_brightness(rgbled_cdev, state);
	if (ret < 0)
		goto unlock;

	colors->green = state;
	ret = size;
unlock:
	mutex_unlock(&led_cdev->led_access);
	return ret;
}

static ssize_t green_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;

	if (rgbled_cdev->ops->get_green_brightness)
		colors->green = rgbled_cdev->ops->get_green_brightness(rgbled_cdev);

	return sprintf(buf, "%d\n", colors->green);
}
static DEVICE_ATTR_RW(green);

static struct attribute *led_color_green_attrs[] = {
	&dev_attr_green.attr,
	NULL,
};

static const struct attribute_group led_set_green_group = {
	.attrs = led_color_green_attrs,
};

static ssize_t blue_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	const struct led_rgb_ops *ops = rgbled_cdev->ops;
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;
	unsigned long state;
	ssize_t ret = -EINVAL;

	mutex_lock(&led_cdev->led_access);

	if (led_sysfs_is_disabled(led_cdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		goto unlock;

	if (state > LED_FULL) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = ops->set_blue_brightness(rgbled_cdev, state);
	if (ret < 0)
		goto unlock;

	colors->blue = state;
	ret = size;
unlock:
	mutex_unlock(&led_cdev->led_access);
	return ret;
}

static ssize_t blue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_rgb *rgbled_cdev = lcdev_to_rgbcdev(led_cdev);
	struct led_rgb_colors *colors = &rgbled_cdev->rgb_colors;

	if (rgbled_cdev->ops->get_blue_brightness)
		colors->blue = rgbled_cdev->ops->get_blue_brightness(rgbled_cdev);

	return sprintf(buf, "%d\n", colors->blue);
}
static DEVICE_ATTR_RW(blue);

static struct attribute *led_color_blue_attrs[] = {
	&dev_attr_blue.attr,
	NULL,
};

static const struct attribute_group led_set_blue_group = {
	.attrs = led_color_blue_attrs,
};

static void led_rgb_init_sysfs_groups(struct led_classdev_rgb *rgbled_cdev)
{
	struct led_classdev *led_cdev = &rgbled_cdev->led_cdev;
	const struct led_rgb_ops *ops = rgbled_cdev->ops;
	const struct attribute_group **rgb_groups = rgbled_cdev->sysfs_groups;

	int num_sysfs_groups = 0;

	rgb_groups[num_sysfs_groups++] = &led_color_group;

	if (ops->set_red_brightness)
		rgb_groups[num_sysfs_groups++] = &led_set_red_group;

	if (ops->set_green_brightness)
		rgb_groups[num_sysfs_groups++] = &led_set_green_group;

	if (ops->set_blue_brightness)
		rgb_groups[num_sysfs_groups++] = &led_set_blue_group;

	led_cdev->groups = rgb_groups;
}

static void set_color_delayed(struct work_struct *ws)
{
	struct led_classdev_rgb *rgbled_cdev =
		container_of(ws, struct led_classdev_rgb, set_color_work);

	mutex_lock(&rgbled_cdev->led_cdev.led_access);
	rgbled_cdev->ops->set_color(rgbled_cdev);
	mutex_unlock(&rgbled_cdev->led_cdev.led_access);
}

void led_rgb_set_color(struct led_classdev_rgb *rgbled_cdev,
			struct led_rgb_colors color)
{
	rgbled_cdev->rgb_colors = color;
	schedule_work(&rgbled_cdev->set_color_work);
}
EXPORT_SYMBOL_GPL(led_rgb_set_color);

int led_classdev_rgb_register(struct device *parent,
				struct led_classdev_rgb *rgbled_cdev)
{
	struct led_classdev *led_cdev;
	struct led_rgb_ops *ops;

	if (!rgbled_cdev)
		return -EINVAL;

	ops = rgbled_cdev->ops;
	if (!ops || !ops->set_color)
		return -EINVAL;

	led_cdev = &rgbled_cdev->led_cdev;

	/* Select the sysfs attributes to be created for the device */
	led_rgb_init_sysfs_groups(rgbled_cdev);

	INIT_WORK(&rgbled_cdev->set_color_work, set_color_delayed);
	/* Register led class device */
	return led_classdev_register(parent, led_cdev);
}
EXPORT_SYMBOL_GPL(led_classdev_rgb_register);

void led_classdev_rgb_unregister(struct led_classdev_rgb *rgbled_cdev)
{
	if (!rgbled_cdev)
		return;

	cancel_work_sync(&rgbled_cdev->set_color_work);
	led_classdev_unregister(&rgbled_cdev->led_cdev);
}
EXPORT_SYMBOL_GPL(led_classdev_rgb_unregister);

MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
MODULE_DESCRIPTION("RGB LED class interface");
MODULE_LICENSE("GPL v2");
