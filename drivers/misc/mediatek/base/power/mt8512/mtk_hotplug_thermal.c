/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/init.h>	/* module_init, module_exit */
#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/cpu.h>			/* cpu_up */
#include <asm-generic/bug.h>	/* BUG_ON */
#include <linux/proc_fs.h>  /* proc_mkdir, proc_create */
#include <linux/seq_file.h> /* seq_printf, single_open */
#include <linux/uaccess.h>  /* copy_from_user */

#define num_possible_little_cpus() cpumask_weight(&hpt_ctxt.little_cpumask)
#define num_possible_big_cpus() cpumask_weight(&hpt_ctxt.big_cpumask)
#define HP_HAVE_SCHED_TPLG		0
#define LOG_CPUMASK			0

#include "mtk_hotplug_thermal.h"

struct hpt_ctxt_struct {
	struct mutex lock; /* Synchronizes accesses */

	/* cpu arch */
	unsigned int is_hmp;
	struct cpumask little_cpumask;
	struct cpumask big_cpumask;
	unsigned int little_cpu_id_min;
	unsigned int little_cpu_id_max;
	unsigned int big_cpu_id_min;
	unsigned int big_cpu_id_max;

	unsigned int always_online_little[4];
	unsigned int always_online_big[4];
};

struct hpt_ctxt_struct hpt_ctxt = {
	/* core */
	.lock = __MUTEX_INITIALIZER(hpt_ctxt.lock),

	.always_online_little = {1, 0, 0, 0},
	.always_online_big = {0, 0, 0, 0},
};

static int hpt_always_online_little_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%u %u %u %u\n",
		hpt_ctxt.always_online_little[0],
		hpt_ctxt.always_online_little[1],
		hpt_ctxt.always_online_little[2],
		hpt_ctxt.always_online_little[3]);
	return 0;
}

static ssize_t hpt_always_online_little_proc_write(struct file *file,
					       const char __user *buffer,
					       size_t count, loff_t *pos)
{
	int len = 0, err = 0, cpu, temp[4] = {0};
	char desc[30];
	struct cpumask little_online_cpumask;

	len = min(count, sizeof(desc) - 1);

	cpumask_and(&little_online_cpumask,
		&hpt_ctxt.little_cpumask, cpu_online_mask);

	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%u %u %u %u ",
			&temp[0], &temp[1],
			&temp[2], &temp[3]) == 4) {

		lock_power_throttle();

		hpt_ctxt.always_online_little[1] = temp[1];
		hpt_ctxt.always_online_little[2] = temp[2];
		hpt_ctxt.always_online_little[3] = temp[3];

		for (cpu = hpt_ctxt.little_cpu_id_min;
			cpu <= hpt_ctxt.little_cpu_id_max; ++cpu) {

			if (hpt_ctxt.always_online_little[cpu] == 1) {
				if (!cpumask_test_cpu(cpu,
					&little_online_cpumask)) {
					err = hpt_cpu_up(cpu);
					if (err)
						return -EINVAL;
					cpumask_set_cpu(cpu,
						&little_online_cpumask);
				}
				continue;
			}
		}
		err = hpt_ctxt.always_online_little[0]
				+ hpt_ctxt.always_online_little[1]
				+ hpt_ctxt.always_online_little[2]
				+ hpt_ctxt.always_online_little[3];
		update_min_thermal_limit_cpu((unsigned int)err);
		unlock_power_throttle();
		update_thermal_limit_protect();
		return count;
	}
	return -EINVAL;
}

static int hpt_always_online_big_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "%u %u %u %u\n",
		hpt_ctxt.always_online_big[0],
		hpt_ctxt.always_online_big[1],
		hpt_ctxt.always_online_big[2],
		hpt_ctxt.always_online_big[3]);
	return 0;
}

static ssize_t hpt_always_online_big_proc_write(struct file *file,
					       const char __user *buffer,
					       size_t count, loff_t *pos)
{
	int len = 0, err = 0, cpu, temp[4] = {0};
	char desc[30];
	struct cpumask big_online_cpumask;

	len = min(count, sizeof(desc) - 1);
	cpumask_and(&big_online_cpumask, &hpt_ctxt.big_cpumask,
			cpu_online_mask);

	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%u %u %u %u ", &temp[0], &temp[1],
			&temp[2], &temp[3]) == 4){

		lock_power_throttle();

		hpt_ctxt.always_online_big[0] = temp[0];
		hpt_ctxt.always_online_big[1] = temp[1];
		hpt_ctxt.always_online_big[2] = temp[2];
		hpt_ctxt.always_online_big[3] = temp[3];

		for (cpu = hpt_ctxt.big_cpu_id_min;
			cpu <= hpt_ctxt.big_cpu_id_max; ++cpu) {
			if (hpt_ctxt.always_online_big[cpu] == 1) {
				if (!cpumask_test_cpu(cpu,
					&big_online_cpumask)) {
					err = hpt_cpu_up(cpu);
					if (err)
						return -EINVAL;
					cpumask_set_cpu(cpu,
						&big_online_cpumask);
				}
				continue;
			}
		}

		unlock_power_throttle();

		return count;
	}

	return -EINVAL;
}

static int hpt_always_online_little_proc_open(struct inode *inode,
				  struct file *file)
{
	return single_open(file, hpt_always_online_little_proc_show,
			   PDE_DATA(inode));
}

static const struct file_operations hpt_always_online_little_proc_fops = {
	.owner = THIS_MODULE,
	.open = hpt_always_online_little_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = hpt_always_online_little_proc_write,
};

static int hpt_always_online_big_proc_open(struct inode *inode,
				  struct file *file)
{
	return single_open(file, hpt_always_online_big_proc_show,
			   PDE_DATA(inode));
}

static const struct file_operations hpt_always_online_big_proc_fops = {
	.owner = THIS_MODULE,
	.open = hpt_always_online_big_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = hpt_always_online_big_proc_write,
};

static void hpt_procfs_create(void)
{
	struct proc_dir_entry *mtktscpu_dir = NULL;

	mtktscpu_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscpu_dir)
		pr_info("proc/driver/thermal dir not exist\n");

	if (!proc_create("always_online_little", 0664, mtktscpu_dir,
				 &hpt_always_online_little_proc_fops))
		pr_info("create /proc/driver/thermal/always_online_little failed\n");

	if (!proc_create("always_online_big", 0664, mtktscpu_dir,
				 &hpt_always_online_big_proc_fops))
		pr_info("create /proc/driver/thermal/always_online_big failed\n");

}

static int hpt_init(void)
{
	int r = 0;
#if LOG_CPUMASK
	char str1[32];
#endif

	//log_info("%s\n", __func__);

	/* init cpu arch in hpt_ctxt */
	/* init cpumask */
	cpumask_clear(&hpt_ctxt.little_cpumask);
	cpumask_clear(&hpt_ctxt.big_cpumask);

	hpt_cpu_get_big_little_cpumasks(&hpt_ctxt.big_cpumask,
					&hpt_ctxt.little_cpumask);

#if LOG_CPUMASK
	cpulist_scnprintf(str1, sizeof(str1), &hpt_ctxt.little_cpumask);
	//log_info("hpt_ctxt.little_cpumask: %s\n", str1);
	cpulist_scnprintf(str1, sizeof(str1), &hpt_ctxt.big_cpumask);
	//log_info("hpt_ctxt.big_cpumask: %s\n", str1);
#endif

	if (cpumask_weight(&hpt_ctxt.little_cpumask) == 0) {
		cpumask_copy(&hpt_ctxt.little_cpumask, &hpt_ctxt.big_cpumask);
		cpumask_clear(&hpt_ctxt.big_cpumask);
	}

	/* verify arch is hmp or smp */
	if (!cpumask_empty(&hpt_ctxt.little_cpumask) &&
	    !cpumask_empty(&hpt_ctxt.big_cpumask)) {
		unsigned int cpu;

		hpt_ctxt.is_hmp = 1;
		hpt_ctxt.little_cpu_id_min = num_possible_cpus();
		hpt_ctxt.big_cpu_id_min = num_possible_cpus();

		for_each_cpu((cpu), &hpt_ctxt.little_cpumask) {
			if (cpu < hpt_ctxt.little_cpu_id_min)
				hpt_ctxt.little_cpu_id_min = cpu;
			if (cpu > hpt_ctxt.little_cpu_id_max)
				hpt_ctxt.little_cpu_id_max = cpu;
		}

		for_each_cpu((cpu), &hpt_ctxt.big_cpumask) {
			if (cpu < hpt_ctxt.big_cpu_id_min)
				hpt_ctxt.big_cpu_id_min = cpu;
			if (cpu > hpt_ctxt.big_cpu_id_max)
				hpt_ctxt.big_cpu_id_max = cpu;
		}
	} else {
		hpt_ctxt.is_hmp = 0;
		hpt_ctxt.little_cpu_id_min = 0;
		hpt_ctxt.little_cpu_id_max = num_possible_little_cpus() - 1;
	}

	r = hpt_ctxt.always_online_little[0] +
		hpt_ctxt.always_online_little[1] +
		hpt_ctxt.always_online_little[2] +
		hpt_ctxt.always_online_little[3];

	hpt_procfs_create();
	update_min_thermal_limit_cpu((unsigned int)r);

	pr_info(
		"%s: little_cpu_id_min: %u, little_cpu_id_max: %u, big_cpu_id_min: %u, big_cpu_id_max: %u\n",
		__func__, hpt_ctxt.little_cpu_id_min,
		hpt_ctxt.little_cpu_id_max, hpt_ctxt.big_cpu_id_min,
		hpt_ctxt.big_cpu_id_max);

	return r;
}

#if 0
static void hotplug_thermal_limit_big(unsigned int big_num_limit)
{
	unsigned int cpu;
	unsigned int val;
	int err;
	unsigned int big_num_online = num_online_big_cpus();
	struct cpumask big_online_cpumask;

	cpumask_and(&big_online_cpumask,
		&hpt_ctxt.big_cpumask, cpu_online_mask);

	if (big_num_online == big_num_limit) {
		return;
	} else if (big_num_online > big_num_limit) {
		val = big_num_online - big_num_limit;
		for (cpu = hpt_ctxt.big_cpu_id_max;
			cpu >= hpt_ctxt.big_cpu_id_min &&
			cpu <= hpt_ctxt.big_cpu_id_max; --cpu) {

			if (hpt_ctxt.always_online_big[cpu] == 1) {
				if (!cpumask_test_cpu(cpu,
					&big_online_cpumask)) {
					err = hpt_cpu_up(cpu);
					if (err)
						return;
					cpumask_set_cpu(cpu,
						&big_online_cpumask);
					++big_num_online;
					++val;
				}
				continue;
			}
			if (!cpumask_test_cpu(cpu,
				&big_online_cpumask))
				continue;

			err = hpt_cpu_down(cpu);
			if (err)
				return;

			cpumask_clear_cpu(cpu,
				&big_online_cpumask);
			--big_num_online;
			--val;

			if (val == 0 ||
				big_num_online == big_num_limit)
				break;
		}
	} else {
		val = big_num_limit - big_num_online;
		for (cpu = hpt_ctxt.big_cpu_id_min;
			cpu >= hpt_ctxt.big_cpu_id_min &&
			cpu <= hpt_ctxt.big_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu,
				&big_online_cpumask))
				continue;

			err = hpt_cpu_up(cpu);
			if (err)
				return;
			cpumask_set_cpu(cpu, &big_online_cpumask);
			++big_num_online;
			--val;
			if (val == 0 ||
				big_num_online == big_num_limit)
				break;
		}
	}

}

static void hotplug_thermal_limit_little(unsigned int little_num_limit)
{
	unsigned int cpu;
	unsigned int val;
	int err;
	unsigned int little_num_online = num_online_little_cpus();
	struct cpumask little_online_cpumask;

	cpumask_and(&little_online_cpumask,
		&hpt_ctxt.little_cpumask, cpu_online_mask);

	if (little_num_online == little_num_limit) {
		return;
	} else if (little_num_online > little_num_limit) {
		val = little_num_online - little_num_limit;
		for (cpu = hpt_ctxt.little_cpu_id_max;
			cpu > hpt_ctxt.little_cpu_id_min &&
			cpu <= hpt_ctxt.little_cpu_id_max; --cpu) {

			if (hpt_ctxt.always_online_little[cpu] == 1) {
				if (!cpumask_test_cpu(cpu,
					&little_online_cpumask)) {
					err = hpt_cpu_up(cpu);
					if (err)
						return;
					cpumask_set_cpu(cpu,
						&little_online_cpumask);
					++little_num_online;
					++val;
				}
				continue;
			}
			if (!cpumask_test_cpu(cpu, &little_online_cpumask))
				continue;

			if (little_num_online <= 1)
				break;

			err = hpt_cpu_down(cpu);
			if (err)
				return;

			cpumask_clear_cpu(cpu, &little_online_cpumask);
			--little_num_online;
			--val;

			if (val == 0 || little_num_online == little_num_limit)
				break;
		}
	} else {
		val = little_num_limit - little_num_online;
		for (cpu = hpt_ctxt.little_cpu_id_min;
			cpu >= hpt_ctxt.little_cpu_id_min &&
			cpu <= hpt_ctxt.little_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu, &little_online_cpumask))
				continue;

			err = hpt_cpu_up(cpu);
			if (err)
				return;
			cpumask_set_cpu(cpu, &little_online_cpumask);
			++little_num_online;
			--val;
			if (val == 0 || little_num_online == little_num_limit)
				break;
		}
	}

}

int hpt_set_cpu_num_limit(unsigned int little_cpu,
	unsigned int big_cpu)
{

	if ((little_cpu > num_possible_little_cpus())
		|| (little_cpu < 1))
		return -1;

	if (hpt_ctxt.is_hmp &&
		(big_cpu > num_possible_big_cpus()))
		return -1;

	mutex_lock(&hpt_ctxt.lock);
	if (num_online_little_cpus() != little_cpu)
		hotplug_thermal_limit_little(little_cpu);

	if (hpt_ctxt.is_hmp) {
		if (num_online_big_cpus() != big_cpu)
			hotplug_thermal_limit_big(big_cpu);
	}

	mutex_unlock(&hpt_ctxt.lock);

	return 0;
}

unsigned int num_online_little_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask,
		&hpt_ctxt.little_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}

unsigned int num_online_big_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask,
		&hpt_ctxt.big_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}
#endif

int hpt_set_cpu_num_limit(unsigned int little_cpu, unsigned int big_cpu)
{

	pr_info("@@@@@@@@@@@@@@@@@@@@@@@\n");
	pr_info("%s thermal limit little cpu:%d, big cpu:%d\n",
		__func__, little_cpu, big_cpu);
	pr_info("@@@@@@@@@@@@@@@@@@@@@@@\n");
	if ((little_cpu > num_possible_little_cpus()) || (little_cpu < 1))
		return -1;

	if (hpt_ctxt.is_hmp && (big_cpu > num_possible_big_cpus()))
		return -1;

	mutex_lock(&hpt_ctxt.lock);

	hotplug_thermal_limit(little_cpu, CLUSTER_L);

	if (hpt_ctxt.is_hmp)
		hotplug_thermal_limit(big_cpu, CLUSTER_B);

	mutex_unlock(&hpt_ctxt.lock);

	return 0;
}

static void hotplug_thermal_limit(unsigned int cpu_num_limit,
	enum cluster_type type)
{
	unsigned int cluster_cpu_id_max, cluster_cpu_id_min;
	unsigned int cpu, val;
	unsigned int *always_online = NULL;
	int err;
	struct cpumask online_cpumask;
	unsigned int cpu_num_online =
		hpt_num_online_cpus(&online_cpumask,
			&always_online, type);

	get_cluster_cpu_id(&cluster_cpu_id_min, &cluster_cpu_id_max, type);

	if (cpu_num_online == cpu_num_limit) {
		return;
	} else if (cpu_num_online > cpu_num_limit) {
		val = cpu_num_online - cpu_num_limit;
		for (cpu = cluster_cpu_id_max;
			cpu >= cluster_cpu_id_min &&
			cpu <= cluster_cpu_id_max; --cpu) {

			if (always_online[cpu] == 1) {
				if (!cpumask_test_cpu(cpu,
					&online_cpumask)) {
					err = hpt_cpu_up(cpu);
					if (err)
						return;
					cpumask_set_cpu(cpu,
						&online_cpumask);
					++cpu_num_online;
					++val;
				}
				continue;
			}
			if (!cpumask_test_cpu(cpu, &online_cpumask))
				continue;

			if (cpu_num_online <= 1)
				break;

			err = hpt_cpu_down(cpu);
			if (err)
				return;

			cpumask_clear_cpu(cpu, &online_cpumask);
			--cpu_num_online;
			--val;

			if (val == 0 || cpu_num_online == cpu_num_limit)
				break;
		}
	} else {
		val = cpu_num_limit - cpu_num_online;
		for (cpu = cluster_cpu_id_min;
			cpu >= cluster_cpu_id_min &&
			cpu <= cluster_cpu_id_max; ++cpu) {
			if (cpumask_test_cpu(cpu, &online_cpumask))
				continue;

			err = hpt_cpu_up(cpu);
			if (err)
				return;
			cpumask_set_cpu(cpu, &online_cpumask);
			++cpu_num_online;
			--val;
			if (val == 0 || cpu_num_online == cpu_num_limit)
				break;
		}
	}

}

unsigned int hpt_num_online_cpus(struct cpumask *cluster_cpu_mask,
		unsigned int **always_online, enum cluster_type type)
{
	switch (type) {
	case CLUSTER_L:
		cpumask_and(cluster_cpu_mask,
			&hpt_ctxt.little_cpumask, cpu_online_mask);
		*always_online = &hpt_ctxt.always_online_little[0];
		return cpumask_weight(cluster_cpu_mask);
	case CLUSTER_B:
		cpumask_and(cluster_cpu_mask,
			&hpt_ctxt.big_cpumask, cpu_online_mask);
		*always_online = &hpt_ctxt.always_online_big[0];
		return cpumask_weight(cluster_cpu_mask);
	default:
		return UINT_MAX;
	}
}

void get_cluster_cpu_id(unsigned int *cluster_cpu_id_min,
		unsigned int *cluster_cpu_id_max, enum cluster_type type)
{
	switch (type) {
	case CLUSTER_L:
		*cluster_cpu_id_min = hpt_ctxt.little_cpu_id_min;
		*cluster_cpu_id_max = hpt_ctxt.little_cpu_id_max;
		break;
	case CLUSTER_B:
		*cluster_cpu_id_min = hpt_ctxt.little_cpu_id_min;
		*cluster_cpu_id_max = hpt_ctxt.little_cpu_id_max;
		break;
	default:
		*cluster_cpu_id_min = UINT_MAX;
		*cluster_cpu_id_max = UINT_MAX;
		break;
	}
}

static void hpt_cpu_get_big_little_cpumasks(struct cpumask *big,
				     struct cpumask *little)
{
#if HP_HAVE_SCHED_TPLG
	sched_get_big_little_cpus(big, little);
#else
	unsigned int cpu;

	cpumask_clear(big);
	cpumask_clear(little);

	for_each_possible_cpu(cpu) {
		if (arch_cpu_is_big(cpu))
			cpumask_set_cpu(cpu, big);
		else
			cpumask_set_cpu(cpu, little);
	}
#endif /* HP_HAVE_SCHED_TPLG */
}

static int hpt_cpu_up(unsigned int cpu)
{
	int err = 0;

	lock_device_hotplug();
	if (!cpu_online(cpu)) {
		err = device_online(get_cpu_device(cpu));
		if (err)
			pr_err("Error: Hotplug: %s %d cpu_online = %d\n",
				__func__, __LINE__, cpu_online(cpu));
	}
	unlock_device_hotplug();

	return err;
}

static int hpt_cpu_down(unsigned int cpu)
{
	int err = 0;

	lock_device_hotplug();
	if (cpu_online(cpu)) {
		err = device_offline(get_cpu_device(cpu));
		if (err)
			pr_err("Error: Hotplug: %s %d cpu%d cpu_online = %d\n",
				__func__, __LINE__,
				cpu, cpu_online(cpu));
	}
	unlock_device_hotplug();

	return err;
}

/*
 * module exit function
 */
static void __exit hpt_exit(void)
{
}

module_init(hpt_init);
module_exit(hpt_exit);


