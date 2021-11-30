/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_SLOA_FS_H__
#define __MTK_SLOA_FS_H__

enum clk_26m {
	CLK_26M_OFF = 0,
	FAKE_DCXO_26M,
	ULPLL_26M,
};

enum SRC_REQ {
	APSRC_REQ = 0,
	F26M_REQ,
	INFRA_REQ,
	VRF18_REQ,
	DDREN_REQ,
};

void sloa_suspend_infra_power(bool on);
int sloa_vcore_req(bool on);
int sloa_suspend_src_req(enum SRC_REQ req, unsigned int val);
int sloa_dpidle_src_req(enum SRC_REQ req, unsigned int val);
int sloa_26m_on(bool on);

#endif

