/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
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
#ifndef _DTS_IOMMU_PORT_MT8512_H_
#define _DTS_IOMMU_PORT_MT8512_H_

#define MTK_M4U_ID(larb, port)	(((larb) << 5) | (port))

#define M4U_LARB0_ID			0
#define M4U_LARB1_ID			1

/* larb0 */
#define M4U_PORT_WF_LUT_RDMA		MTK_M4U_ID(M4U_LARB0_ID, 0)
#define M4U_PORT_WF_LUT_0		MTK_M4U_ID(M4U_LARB0_ID, 1)
#define M4U_PORT_WF_LUT_1		MTK_M4U_ID(M4U_LARB0_ID, 2)
#define M4U_PORT_WF_LUT_2		MTK_M4U_ID(M4U_LARB0_ID, 3)
#define M4U_PORT_WF_LUT_3		MTK_M4U_ID(M4U_LARB0_ID, 4)
#define M4U_PORT_UPD_RDMA		MTK_M4U_ID(M4U_LARB0_ID, 5)
#define M4U_PORT_WB_RDMA		MTK_M4U_ID(M4U_LARB0_ID, 6)
#define M4U_PORT_PIPELINE_WDMA		MTK_M4U_ID(M4U_LARB0_ID, 7)
#define M4U_PORT_DISP_RDMA0		MTK_M4U_ID(M4U_LARB0_ID, 8)
#define M4U_PORT_DISP_FAKE0		MTK_M4U_ID(M4U_LARB0_ID, 9)

/* larb1 */
#define M4U_PORT_MDP_RDMA0		MTK_M4U_ID(M4U_LARB1_ID, 0)
#define M4U_PORT_MDP_WROT0		MTK_M4U_ID(M4U_LARB1_ID, 1)
#define M4U_PORT_DISP_OVL0_2L		MTK_M4U_ID(M4U_LARB1_ID, 2)
#define M4U_PORT_DISP_WDMA0		MTK_M4U_ID(M4U_LARB1_ID, 3)
#define M4U_PORT_JDEC_BITS_RO		MTK_M4U_ID(M4U_LARB1_ID, 4)
#define M4U_PORT_JDEC_WINFT_WO		MTK_M4U_ID(M4U_LARB1_ID, 5)
#define M4U_PORT_JDEC_NZ_WR		MTK_M4U_ID(M4U_LARB1_ID, 6)
#define M4U_PORT_JDEC_COEFF_RO		MTK_M4U_ID(M4U_LARB1_ID, 7)
#define M4U_PORT_IMGRZ_BITS		MTK_M4U_ID(M4U_LARB1_ID, 8)
#define M4U_PORT_IMGRZ_PLD		MTK_M4U_ID(M4U_LARB1_ID, 9)
#define M4U_PORT_IMGRZ_WR		MTK_M4U_ID(M4U_LARB1_ID, 10)
#define M4U_PORT_PNG_LZ77W		MTK_M4U_ID(M4U_LARB1_ID, 11)
#define M4U_PORT_PNG_LNBFW		MTK_M4U_ID(M4U_LARB1_ID, 12)
#define M4U_PORT_PNG_LZ77R		MTK_M4U_ID(M4U_LARB1_ID, 13)
#define M4U_PORT_PNG_LNBFR		MTK_M4U_ID(M4U_LARB1_ID, 14)
#define M4U_PORT_PNG_PAR		MTK_M4U_ID(M4U_LARB1_ID, 15)
#define M4U_PORT_PNG_PELOUT		MTK_M4U_ID(M4U_LARB1_ID, 16)
#define M4U_PORT_DISP_FAKE1		MTK_M4U_ID(M4U_LARB1_ID, 17)

#endif
