/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yong Liang, MediaTek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_MT8512
#define _DT_BINDINGS_RESET_CONTROLLER_MT8512

/* INFRACFG resets */
#define MT8512_INFRA_THERMAL_CTRL_RST				0
#define MT8512_INFRA_USB_TOP_RST				1
#define MT8512_INFRA_MM_IOMMU_RST				3
#define MT8512_INFRA_MSDC1_RST					6
#define MT8512_INFRA_MSDC0_RST					7
#define MT8512_INFRA_APDMA_RST					9
#define MT8512_INFRA_BTIF_RST					12
#define MT8512_INFRA_AUXADC_RST					15

#define MT8512_INFRA_PTP_CTRL_RST				32
#define MT8512_INFRA_SPI_RST					33
#define MT8512_INFRA_I2C0_RST					34
#define MT8512_INFRA_UART0_RST					38
#define MT8512_INFRA_UART1_RST					39
#define MT8512_INFRA_UART2_RST					40
#define MT8512_INFRA_FLASHIF_RST				42
#define MT8512_INFRA_SPI2_RST					45
#define MT8512_INFRA_NFI_RST					47

#define MT8512_INFRA_SPM_RST					65
#define MT8512_INFRA_UBISIF_RST					66
#define MT8512_INFRA_KP_RST					68
#define MT8512_INFRA_APXGPT_RST					69
#define MT8512_INFRA_SSUSB_U2PHY_RST				73
#define MT8512_INFRA_IRRX_RST					75
#define MT8512_INFRA_DSP_UART_RST				76
#define MT8512_INFRA_PWM_AO_RST					77
#define MT8512_INFRA_I2C1_RST					78
#define MT8512_INFRA_I2C2_RST					79

#define MT8512_INFRA_GCPU_RST					96
#define MT8512_INFRA_GCE_RST					97
#define MT8512_INFRA_AP2CONN_RST				115
#define MT8512_INFRA_CONN2INFRA_RST				116
#define MT8512_INFRA_ECC_TOP_RST				124

#define MT8512_INFRA_SW_RST_NUM					125

#endif  /* _DT_BINDINGS_RESET_CONTROLLER_MT8512 */
