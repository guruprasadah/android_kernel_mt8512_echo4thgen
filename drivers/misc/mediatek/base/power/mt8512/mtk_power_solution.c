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

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "mtk_sloa_fs.h"
#include <linux/string.h>

#define BUCK_NUM 2
/*
 * type: vcore/vproc buck ctrl type(pwm/i2c/gpio)
 * channel: buck pwm/i2c channel
 * latency: wakeup latency
 * voltage_fix: fix voltage or not
 */
struct buckProperty {
	unsigned int type;
	unsigned int channel;
	unsigned int latency;
	unsigned int voltage_fix;
};

struct pwrslt {
	unsigned int merge_ctrl;
	unsigned int fix;
	struct buckProperty buck[BUCK_NUM];
};

enum {
	VCORE_CORE_SRAM = 0,
	VPROC_PROC_SRAM,
	POWER_STATE_MAX,
};

enum BUCK_TYPE {
	I2C = 0,
	PWM,
	GPIO,
};

struct pwrslt pwr_slt[POWER_STATE_MAX];

const u8 *buckcompatible[POWER_STATE_MAX][BUCK_NUM] = { {
	"vcore-solution", "vcore-sram-solution",},
	{
	"vproc-solution", "vproc-sram-solution",}
};

static int init_state_node(char *matches, int state_idx)
{
	int err, i;
	const struct device_node *state_node, *buck_node;
	const char *desc;

	state_node = of_find_compatible_node(NULL, NULL, matches);

	err =  of_property_read_u32(state_node, "fix",
				   &pwr_slt[state_idx].fix);
	if (err)
		pr_debug("missing fix property\n");

	else if (pwr_slt[state_idx].fix) {
		if (state_idx == VCORE_CORE_SRAM) {
			sloa_vcore_req(true);
			return 0;
		}
	}

	err = of_property_read_u32(state_node, "merge",
				   &pwr_slt[state_idx].merge_ctrl);

	if (err)
		pr_debug("missing merge property\n");

	for (i = 0; i < (pwr_slt[state_idx].merge_ctrl ^ 0x1) + 1; i++) {

		buck_node = of_find_compatible_node(NULL, NULL,
					buckcompatible[state_idx][i]);

		err = of_property_read_string(buck_node, "type", &desc);

		if (!err) {
			if (!strcmp("i2c", desc))
				pwr_slt[state_idx].buck[i].type = I2C;
			else if (!strcmp("pwm", desc))
				pwr_slt[state_idx].buck[i].type = PWM;
			else if (!strcmp("gpio", desc))
				pwr_slt[state_idx].buck[i].type = GPIO;
		} else
			pr_debug("missing type property\n");

		if (pwr_slt[state_idx].buck[i].type == PWM)
			sloa_vcore_req(true);

		err = of_property_read_u32(buck_node, "channel",
					   &pwr_slt[state_idx].buck[i].channel);

		if (err)
			pr_debug("missing channel property\n");


		err = of_property_read_u32(buck_node, "latency",
					   &pwr_slt[state_idx].buck[i].latency);

		if (err)
			pr_debug("missing latency property\n");

		err = of_property_read_u32(buck_node, "voltage-fix",
				&pwr_slt[state_idx].buck[i].voltage_fix);

		if (err)
			pr_debug("missing voltage-fix property\n");
	}

	return 0;
}

#define POWER_SOLUTION  "mt8512,power-solution"

int dt_probe_power_solution(void)
{
	struct device_node *state_node, *power_node;
	int i, err = 0;
	unsigned int device_type = 0, clk_on = 0;
	char *power_state_compatible_node[2] = { "mt8512,vcore-core-sram",
						"mt8512,vproc-proc-sram" };

	power_node = of_find_compatible_node(NULL, NULL, POWER_SOLUTION);

	if (!power_node) {
		pr_debug("info:cannot find topckgen node mt8512,power-solution");
		goto fail;
	}

	err = of_property_read_u32(power_node, "device-names",
				   &device_type);
	if (!err) {
		if (device_type) {
			sloa_suspend_src_req(APSRC_REQ, 1);
			sloa_dpidle_src_req(APSRC_REQ, 1);
		}
	}

	err = of_property_read_u32(power_node, "clk-on",
				   &clk_on);
	if (!err) {
		if (clk_on)
			sloa_26m_on(true);
	}

	for (i = 0; ; i++) {
		state_node = of_parse_phandle(power_node, "power-states", i);
		if (!state_node)
			break;

		if (i == POWER_STATE_MAX)
			break;

		err = init_state_node(power_state_compatible_node[i], i);
		if (err) {
			pr_debug("Parsing idle state node failed with err\n");
			break;
		}
		of_node_put(state_node);
	}

	of_node_put(state_node);
	of_node_put(power_node);
	if (err)
		return err;

	return i;
fail:
	return -ENODEV;
}
late_initcall(dt_probe_power_solution);

