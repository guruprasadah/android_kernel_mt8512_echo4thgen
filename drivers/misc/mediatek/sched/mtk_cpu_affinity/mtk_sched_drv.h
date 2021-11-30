/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT_SCHED_DRV_H
#define _MT_SCHED_DRV_H

#include <linux/ioctl.h>
#include <linux/cpumask.h>
#include <linux/pid.h>
#include "mtk_sched_ioctl.h"

long mt_sched_setaffinity(pid_t pid, const struct cpumask *mask);
long mt_sched_getaffinity(pid_t pid, struct cpumask *mask,
		struct cpumask *mt_mask);
long mt_sched_addaffinity(struct task_struct *mask,
		const struct cpumask *mt_mask);
long mt_sched_exitaffinity(pid_t pid);

#ifdef CONFIG_COMPAT
long sched_ioctl_compat(struct file *filp, unsigned int cmd, unsigned long arg);
#endif

#define MT_SCHED_AFFININTY_DEBUG 0

#endif
