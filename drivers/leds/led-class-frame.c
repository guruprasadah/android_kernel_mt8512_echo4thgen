/* LED Frame class interface
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

#include <linux/device.h>
#include <linux/init.h>
#include <linux/led-class-frame.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/slab.h>

static ssize_t frame_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_frame *frameled_cdev = lcdev_to_framecdev(led_cdev);
	const struct led_frame_ops *ops = frameled_cdev->ops;
	int i, count = 0;
	char val[3] = {0,};
	ssize_t ret = -EINVAL;

	mutex_lock(&frameled_cdev->lock);

	if (led_sysfs_is_disabled(led_cdev)) {
		ret = -EBUSY;
		goto unlock;
	}

	if (size < frameled_cdev->nr_channels) {
		pr_err("%s: Invalid input for frame size\n", __func__);
		goto unlock;
	}

	for (i = 0; i < frameled_cdev->nr_channels * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &frameled_cdev->new_frame[count]);
		if (ret) {
			pr_err("%s: Invalid input for frame_store, ret=%d\n",
				__func__, ret);
			goto unlock;
		}
		count++;
	}

	ret = ops->set_frame(frameled_cdev);
	if (ret < 0)
		goto unlock;

	strncpy(frameled_cdev->frame, frameled_cdev->new_frame, frameled_cdev->nr_channels);
	ret = size;
unlock:
	mutex_unlock(&frameled_cdev->lock);
	return ret;
}

static ssize_t frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_frame *frameled_cdev = lcdev_to_framecdev(led_cdev);
	int i, len = 0;

	mutex_lock(&frameled_cdev->lock);

	for (i = 0; i < frameled_cdev->nr_channels; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%02x", frameled_cdev->frame[i]);
	}

	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	mutex_unlock(&frameled_cdev->lock);

	return len;
}

static DEVICE_ATTR_RW(frame);

static struct attribute *led_frame_attrs[] = {
	&dev_attr_frame.attr,
	NULL,
};

static const struct attribute_group led_frame_group = {
	.attrs = led_frame_attrs,
};

static ssize_t frame_brightness_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_frame *frameled_cdev = lcdev_to_framecdev(led_cdev);
	const struct led_frame_ops *ops = frameled_cdev->ops;
	unsigned long state;
	ssize_t ret = -EINVAL;

	mutex_lock(&frameled_cdev->lock);

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

	ret = ops->set_frame_brightness(frameled_cdev, state);
	if (ret < 0)
		goto unlock;

	frameled_cdev->frame_brightness = state;

	ret = size;
unlock:
	mutex_unlock(&frameled_cdev->lock);
	return ret;
}

static ssize_t frame_brightness_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_classdev_frame *frameled_cdev = lcdev_to_framecdev(led_cdev);
	int ret;

	mutex_lock(&frameled_cdev->lock);
	if (frameled_cdev->ops->get_frame_brightness)
		frameled_cdev->frame_brightness = frameled_cdev->ops->get_frame_brightness(frameled_cdev);

	ret = sprintf(buf, "%d\n", frameled_cdev->frame_brightness);

	mutex_unlock(&frameled_cdev->lock);
	return ret;
}
static DEVICE_ATTR_RW(frame_brightness);

static struct attribute *led_frame_brightness_attrs[] = {
	&dev_attr_frame_brightness.attr,
	NULL,
};

static const struct attribute_group led_frame_brightness_group = {
	.attrs = led_frame_brightness_attrs,
};

int led_classdev_frame_register(struct device *parent,
				struct led_classdev_frame *frameled_cdev)
{
	struct led_classdev *led_cdev;
	struct led_frame_ops *ops;

	if (!frameled_cdev)
		return -EINVAL;

	ops = frameled_cdev->ops;
	if (!ops || !ops->set_frame)
		return -EINVAL;

	if (!frameled_cdev->nr_channels)
		return -EINVAL;

	if (!frameled_cdev->frame) {
		frameled_cdev->frame = devm_kzalloc(parent, frameled_cdev->nr_channels, GFP_KERNEL);
		if (!frameled_cdev->frame)
			return -ENOMEM;
	}

	if (!frameled_cdev->new_frame) {
		frameled_cdev->new_frame = devm_kzalloc(parent, frameled_cdev->nr_channels, GFP_KERNEL);
		if (!frameled_cdev->new_frame)
			return -ENOMEM;
	}

	mutex_init(&frameled_cdev->lock);

        frameled_cdev->sysfs_groups[0] = &led_frame_group;


	if(ops->set_frame_brightness)
		frameled_cdev->sysfs_groups[1] = &led_frame_brightness_group;


	led_cdev = &frameled_cdev->led_cdev;
	led_cdev->groups = frameled_cdev->sysfs_groups;

	return led_classdev_register(parent, led_cdev);

}
EXPORT_SYMBOL_GPL(led_classdev_frame_register);

void led_classdev_frame_unregister(struct led_classdev_frame *frameled_cdev)
{
	if (!frameled_cdev)
		return;

	led_classdev_unregister(&frameled_cdev->led_cdev);

	mutex_destroy(&frameled_cdev->lock);
}
EXPORT_SYMBOL_GPL(led_classdev_frame_unregister);

MODULE_LICENSE("GPL v2");
