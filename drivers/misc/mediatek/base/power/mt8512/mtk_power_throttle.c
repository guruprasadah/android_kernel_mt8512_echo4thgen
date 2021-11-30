/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h> /* ARRAY_SIZE */
#include <linux/slab.h> /* kzalloc */
#include "mtk_static_power.h"	/* static power */
#include <linux/cpufreq.h>
#include "mtk_power_throttle.h"
#include "mach/mtk_thermal.h"
#include <linux/proc_fs.h>  /* proc_mkdir, proc_create */
#include <linux/seq_file.h> /* seq_printf, single_open */
#include <linux/uaccess.h>  /* copy_from_user */

static unsigned long clipped_freq;
static unsigned int limited_max_ncpu;
static unsigned int limited_max_freq;

#define NR_MAX_OPP_TBL  16
#define NR_MAX_CPU      2

static unsigned long previous_limit_power;
/*
 * min_thermal_limit_cpu only be modify by hpt
 */
static unsigned int min_thermal_limit_cpu = 1;
//static unsigned int max_thermal_limit_cpu = NR_MAX_CPU;
static unsigned int min_thermal_limit_freq;
static unsigned int max_thermal_limit_freq = UINT_MAX;

struct proc_dir_entry __attribute__((weak))
*mtk_thermal_get_proc_drv_therm_dir_entry(void)
{
	pr_info("Fail: %s /proc/driver/thermal doesn't exist\n"
			"maybe thermal config is not enable\n",	__func__);
	return NULL;
}

int __attribute__((weak)) mtktscpu_debug_log = 0;

struct mt_cpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_ncpu;
	unsigned int cpufreq_power;
};

struct mt_cpu_dvfs {
	const char *name;
	unsigned int cpu_id;	                /* for cpufreq */
	unsigned int cpu_level;
	struct mt_cpu_dvfs_ops *ops;

	/* opp (freq) table */
	struct mt_cpu_freq_info *opp_tbl;       /* OPP table */
	int nr_opp_tbl;                         /* size for OPP table */
	int idx_opp_tbl;                        /* current OPP idx */
	int idx_normal_max_opp;                 /* idx for normal max OPP */
	/* keep the setting for late resume */
	int idx_opp_tbl_for_late_resume;

	/* freq table for cpufreq */
	struct cpufreq_frequency_table *freq_tbl_for_cpufreq;

	/* power table */
	struct mt_cpu_power_info *power_tbl;
	unsigned int nr_power_tbl;

	/* enable/disable DVFS function */
	int dvfs_disable_count;
	bool dvfs_disable_by_ptpod;
	bool dvfs_disable_by_suspend;
	bool dvfs_disable_by_early_suspend;
	bool dvfs_disable_by_procfs;

	/* limit for thermal */
	unsigned int limited_max_ncpu;
	unsigned int limited_max_freq;
	unsigned int idx_opp_tbl_for_thermal_thro;
	unsigned int thermal_protect_limited_power;

	/* limit for HEVC (via. sysfs) */
	unsigned int limited_freq_by_hevc;

	/* limit max freq from user */
	unsigned int limited_max_freq_by_user;

	/* for ramp down */
	int ramp_down_count;
	int ramp_down_count_const;

#ifdef CONFIG_CPU_DVFS_DOWNGRADE_FREQ
	/* param for micro throttling */
	bool downgrade_freq_for_ptpod;
#endif

	int over_max_cpu;
	int ptpod_temperature_limit_1;
	int ptpod_temperature_limit_2;
	int ptpod_temperature_time_1;
	int ptpod_temperature_time_2;

	int pre_online_cpu;
	unsigned int pre_freq;
	unsigned int downgrade_freq;

	unsigned int downgrade_freq_counter;
	unsigned int downgrade_freq_counter_return;

	unsigned int downgrade_freq_counter_limit;
	unsigned int downgrade_freq_counter_return_limit;

	/* turbo mode */
	unsigned int turbo_mode;

	/* power throttling */
#ifdef CONFIG_CPU_DVFS_POWER_THROTTLING
	/* keep the setting for power throttling */
	int idx_opp_tbl_for_pwr_thro;
	/* idx for power throttle max OPP */
	int idx_pwr_thro_max_opp;
	unsigned int pwr_thro_mode;
#endif
};

struct mt_cpu_dvfs cpu_dvfs;

struct mt_cpu_freq_info opp_tbl_default[NR_MAX_OPP_TBL] = {
	OP(0, 0),
};

static DEFINE_MUTEX(power_throttle_lock);

void lock_power_throttle(void)
{
	mutex_lock(&power_throttle_lock);
}

void unlock_power_throttle(void)
{
	mutex_unlock(&power_throttle_lock);
}

void update_thermal_limit_protect(void)
{
	if (previous_limit_power != 0)
		mt_cpufreq_thermal_protect(previous_limit_power);
}

void update_min_thermal_limit_cpu(unsigned int num)
{
	if (num < 1 || num > NR_MAX_CPU) {
		pr_info("min limit cpu num is invalid!!!!\n");
		return;
	}
	min_thermal_limit_cpu = num;
	pr_info("%s: min_thermal_limit_cpu changes to %d\n",
				__func__, min_thermal_limit_cpu);
	pr_info("previous_limit_power:%lu\n", previous_limit_power);
}

static int thermal_limit_freq_proc_show(
	struct seq_file *m, void *v)
{
	int i = 0;

	seq_puts(m, "min_thermal_limit_freq  max_thermal_limit_freq\n");
	seq_printf(m, "%u %u\n",
		min_thermal_limit_freq, max_thermal_limit_freq);
	for (; i < NR_MAX_OPP_TBL; i++) {
		i % 2 ? seq_printf(m, "%u\t", opp_tbl_default[i].cpufreq_khz) :
		seq_printf(m, "%u\n", opp_tbl_default[i].cpufreq_khz);
	}
#if 0
	seq_printf(m, "%u %u %u %u %u %u %u %u\n",
		opp_tbl_default[0].cpufreq_khz, opp_tbl_default[1].cpufreq_khz,
		opp_tbl_default[2].cpufreq_khz, opp_tbl_default[3].cpufreq_khz,
		opp_tbl_default[4].cpufreq_khz, opp_tbl_default[5].cpufreq_khz,
		opp_tbl_default[6].cpufreq_khz, opp_tbl_default[7].cpufreq_khz);
#endif
	return 0;
}

static ssize_t thermal_limit_freq_proc_write(struct file *file,
					       const char __user *buffer,
					       size_t count, loff_t *pos)
{
	int len = 0, temp1, temp2;
	char desc[30];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc, "%u %u", &temp1, &temp2) == 2) {
		if (temp1 <= temp2) {
			lock_power_throttle();
			min_thermal_limit_freq = temp1;
			max_thermal_limit_freq = temp2;
			unlock_power_throttle();
			update_thermal_limit_protect();
		}
		return count;
	}
	return -EINVAL;
}

static int thermal_limit_freq_proc_proc_open(struct inode *inode,
				  struct file *file)
{
	return single_open(file, thermal_limit_freq_proc_show,
			   PDE_DATA(inode));
}

static const struct
file_operations thermal_limit_freq_proc_proc_fops = {
	.owner = THIS_MODULE,
	.open = thermal_limit_freq_proc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = thermal_limit_freq_proc_write,
};

static void thermal_limit_freq_procfs_create(void)
{
	struct proc_dir_entry *mtktscpu_dir = NULL;

	mtktscpu_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscpu_dir)
		pr_info("proc/driver/thermal dir not exist\n");

	if (!proc_create("thermal_limit_freq", 0664, mtktscpu_dir,
				 &thermal_limit_freq_proc_proc_fops))
		pr_info("create /proc/driver/thermal/thermal_limit_freq failed\n");

}

int __attribute__ ((weak)) is_svs_initialized_done(void)
{
	pr_warn("%s() is not implemented\n", __func__);
	return 0;
}

static void _power_calculation(struct mt_cpu_dvfs *p, int oppidx, int ncpu)
{
/* TBD, need MN confirm */
#define CA35_2CORE_REF_POWER	480 /* mW  */
#define CA35_REF_FREQ	2000000	/* KHz */
#define CA35_REF_VOLT	1025000	/* mV * 1000 */

	int ref_freq, ref_volt;
	int p_dynamic = 0, p_leakage = 0;
	int possible_cpu = NR_MAX_CPU;
	int index;
	/* int	temp; */

	ref_freq = CA35_REF_FREQ;
	ref_volt = CA35_REF_VOLT;

	p_dynamic = CA35_2CORE_REF_POWER;

	/* temp = get_immediate_ts1_wrap() / 1000; */
	/* use 85 degree to gen power data */
	p_leakage = mt_spower_get_leakage(MT_SPOWER_CA7,
		p->opp_tbl[oppidx].cpufreq_volt / 1000, 85);/*  unit in mW */

	p_dynamic = p_dynamic *
	(p->opp_tbl[oppidx].cpufreq_khz / 1000) / (ref_freq / 1000) *
	p->opp_tbl[oppidx].cpufreq_volt / ref_volt *
	p->opp_tbl[oppidx].cpufreq_volt / ref_volt +
	p_leakage;

	index = (NR_MAX_OPP_TBL * (possible_cpu - 1 - ncpu) + oppidx);
	p->power_tbl[index].cpufreq_ncpu = ncpu + 1;
	p->power_tbl[index].cpufreq_khz
		= p->opp_tbl[oppidx].cpufreq_khz;
	p->power_tbl[index].cpufreq_power
		= ((p_dynamic * (ncpu + 1) / possible_cpu < 0) ? 0
			: p_dynamic * (ncpu + 1) / possible_cpu);
}

void init_mt_cpu_dvfs(struct mt_cpu_dvfs *p)
{
	p->nr_opp_tbl = ARRAY_SIZE(opp_tbl_default);
	p->opp_tbl = opp_tbl_default;
	thermal_limit_freq_procfs_create();
}

void dump_power_table(void)
{
	int	i;
	/* dump power table */
	for (i = 0; i < cpu_dvfs.nr_opp_tbl * NR_MAX_CPU; i++) {
		pr_info("%s[%d] = { .cpufreq_khz = %d,\t.cpufreq_ncpu = %d,\t.cpufreq_power = %d }\n",
		__func__,
		i,
		cpu_dvfs.power_tbl[i].cpufreq_khz,
		cpu_dvfs.power_tbl[i].cpufreq_ncpu,
		cpu_dvfs.power_tbl[i].cpufreq_power
		);
	}
}

int setup_power_table(struct mt_cpu_dvfs *p)
{
	unsigned int pwr_eff_tbl[NR_MAX_OPP_TBL][NR_MAX_CPU];
	int possible_cpu = NR_MAX_CPU;
	int i, j;
	int ret = 0;

	WARN_ON(p == NULL);

	if (p->power_tbl)
		goto out;

	/* allocate power table */
	memset((void *)pwr_eff_tbl, 0, sizeof(pwr_eff_tbl));
	p->power_tbl = kzalloc
		(p->nr_opp_tbl * possible_cpu
		* sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	if (p->power_tbl == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	p->nr_power_tbl = p->nr_opp_tbl * possible_cpu;

	/* calc power and fill in power table */
	for (i = 0; i < p->nr_opp_tbl; i++) {
		for (j = 0; j < possible_cpu; j++) {
			if (pwr_eff_tbl[i][j] == 0)
				_power_calculation(p, i, j);
		}
	}

	/* sort power table */
	for (i = p->nr_opp_tbl * possible_cpu; i > 0; i--) {
		for (j = 1; j < i; j++) {
			if (p->power_tbl[j - 1].cpufreq_power
				< p->power_tbl[j].cpufreq_power) {
				struct mt_cpu_power_info tmp;

				tmp.cpufreq_khz	=
					p->power_tbl[j - 1].cpufreq_khz;
				tmp.cpufreq_ncpu =
					p->power_tbl[j - 1].cpufreq_ncpu;
				tmp.cpufreq_power =
					p->power_tbl[j - 1].cpufreq_power;

				p->power_tbl[j - 1].cpufreq_khz   =
					p->power_tbl[j].cpufreq_khz;
				p->power_tbl[j - 1].cpufreq_ncpu  =
					p->power_tbl[j].cpufreq_ncpu;
				p->power_tbl[j - 1].cpufreq_power =
					p->power_tbl[j].cpufreq_power;

				p->power_tbl[j].cpufreq_khz
					= tmp.cpufreq_khz;
				p->power_tbl[j].cpufreq_ncpu
					= tmp.cpufreq_ncpu;
				p->power_tbl[j].cpufreq_power
					= tmp.cpufreq_power;
			}
		}
	}

	/* dump power table */
	dump_power_table();

out:
	return ret;
}

/**
 * cpufreq_thermal_notifier - notifier callback for cpufreq policy change.
 * @nb:	struct notifier_block * with callback info.
 * @event: value showing cpufreq event for which this function invoked.
 * @data: callback-specific data
 *
 * Callback to hijack the notification on cpufreq policy transition.
 * Every time there is a change in policy, we will intercept and
 * update the cpufreq policy with thermal constraints.
 *
 * Return: 0 (success)
 */

static int mtk_cpufreq_thermal_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	int ret;

	pr_debug("%s %ld\n", __func__, event);

	if (event != CPUFREQ_ADJUST)
		return NOTIFY_DONE;

	/*
	 * policy->max is the maximum allowed frequency defined by user
	 * and clipped_freq is the maximum that thermal constraints
	 * allow.
	 *
	 * If clipped_freq is lower than policy->max, then we need to
	 * readjust policy->max.
	 *
	 * But, if clipped_freq is greater than policy->max, we don't
	 * need to do anything.
	 */

	if (mtktscpu_debug_log  & 0x1)
		pr_info("%s clipped_freq = %ld, policy->max=%d, policy->min=%d\n",
				__func__, clipped_freq,
				policy->max, policy->min);

	/* Since thermal throttling could hogplug CPU, less cpufreq might cause
	 * a performance issue.
	 * Only DVFS TLP feature enable, we can keep the max freq by CPUFREQ
	 * GOVERNOR or Pref service.
	 */
	if ((policy->max != clipped_freq) && (clipped_freq >= policy->min)) {
		ret = is_svs_initialized_done();
		if (ret) {
			pr_info("SVS is initializing. Cannot do thermal throttling, ret = %d\n",
				ret);
			return NOTIFY_DONE;
		}

		cpufreq_verify_within_limits(policy, 0, clipped_freq);
	}

	return NOTIFY_OK;
}

/* Notifier for cpufreq policy change */
static struct notifier_block thermal_cpufreq_notifier_block = {
	.notifier_call = mtk_cpufreq_thermal_notifier,
};

static int power_table_ready;
int setup_power_table_tk(void)
{
	int	ret;

	init_mt_cpu_dvfs(&cpu_dvfs);

	if (cpu_dvfs.opp_tbl[0].cpufreq_khz == 0)
		return 0;

	cpufreq_register_notifier(&thermal_cpufreq_notifier_block,
					  CPUFREQ_POLICY_NOTIFIER);

	ret = setup_power_table(&cpu_dvfs);
	power_table_ready = 1;
	return ret;
}

void mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	struct mt_cpu_dvfs *p;
	int possible_cpu;
	int found = 0;
	int i = 0;

	if (power_table_ready == 0) {
		pr_info("%s power_table_ready is not ready\n", __func__);
		return;
	}

	lock_power_throttle();

	previous_limit_power = limited_power;
	p = &cpu_dvfs;
	WARN_ON(p == NULL);
	possible_cpu = NR_MAX_CPU;

	/* no limited */
	if (limited_power == 0) {
		limited_max_ncpu = possible_cpu;
		limited_max_freq = cpu_dvfs.power_tbl[0].cpufreq_khz;
	} else {

		for (i = 0; i < p->nr_opp_tbl * possible_cpu; i++) {

			if (p->power_tbl[i].cpufreq_power == 0 ||
				p->power_tbl[i].cpufreq_ncpu == 0 ||
				p->power_tbl[i].cpufreq_khz == 0)
				break;

			if (p->power_tbl[i].cpufreq_power <= limited_power) {
				if (p->power_tbl[i].cpufreq_ncpu >=
						min_thermal_limit_cpu &&
						p->power_tbl[i].cpufreq_ncpu <=
						NR_MAX_CPU &&
						p->power_tbl[i].cpufreq_khz >=
						min_thermal_limit_freq &&
						p->power_tbl[i].cpufreq_khz <=
						max_thermal_limit_freq) {
					limited_max_ncpu =
						p->power_tbl[i].cpufreq_ncpu;
					limited_max_freq =
						p->power_tbl[i].cpufreq_khz;
					found = 1;
					break;
				}
#if 0
				else {
					pr_err("%s cpu:%d\n", __func__,
					p->power_tbl[i].cpufreq_ncpu);
					pr_err("%s min_thermal_limit_cpu:%d\n",
					__func__, min_thermal_limit_cpu);
					continue;
				}
#endif
			}
		}
		if (!found) {
			for (i--; i <= p->nr_opp_tbl * possible_cpu
					&& i >= 0; i--) {
				if (p->power_tbl[i].cpufreq_ncpu <
					min_thermal_limit_cpu)
					continue;
				if (p->power_tbl[i].cpufreq_khz >=
						min_thermal_limit_freq &&
					p->power_tbl[i].cpufreq_khz <=
						max_thermal_limit_freq) {
					limited_max_ncpu =
						p->power_tbl[i].cpufreq_ncpu;
					limited_max_freq =
						p->power_tbl[i].cpufreq_khz;
					found = 1;
					break;
				}
			}
		}

		/* not found and use lowest power limit */
		if (!found) {
			pr_info("Warning: not found valid freq and ncpu!!!!!!!\n");
			return;
		}
	}

	clipped_freq = limited_max_freq;

	hpt_set_cpu_num_limit(limited_max_ncpu, 0);
	/* update cpufreq policy */
	cpufreq_update_policy(0);

	unlock_power_throttle();
	if (mtktscpu_debug_log & 0x1) {
		pr_info("%s found = %d, limited_power = %u, freq = %u, cpu_num =%u, table: %d\n",
			__func__, found, p->power_tbl[i].cpufreq_power,
			p->power_tbl[i].cpufreq_khz,
			p->power_tbl[i].cpufreq_ncpu, i);
		pr_info("%s possible_cpu = %d, cpu_dvfs.power_tbl[0].cpufreq_khz =%u\n",
			__func__, possible_cpu,
			cpu_dvfs.power_tbl[0].cpufreq_khz);
		pr_info("%s limited_max_ncpu = %u, limited_max_freq = %u\n",
			__func__, limited_max_ncpu, limited_max_freq);
	}
}

