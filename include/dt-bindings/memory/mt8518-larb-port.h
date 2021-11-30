/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
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
#ifndef _DTS_IOMMU_PORT_MT8518_H_
#define _DTS_IOMMU_PORT_MT8518_H_

#define MTK_M4U_ID(larb, port)		(((larb) << 5) | (port))

#define M4U_LARB0_ID			0
#define M4U_LARB1_ID			1
#define M4U_LARB2_ID			2

/* larb0 */
#define M4U_PORT_GFX0			MTK_M4U_ID(M4U_LARB0_ID, 0)

/* larb1 */
#define M4U_PORT_DISP_OVL		MTK_M4U_ID(M4U_LARB1_ID, 0)
#define M4U_PORT_IMGRZ_BITS_GREQ	MTK_M4U_ID(M4U_LARB1_ID, 1)
#define M4U_PORT_IMGRZ_PLD_GREQ		MTK_M4U_ID(M4U_LARB1_ID, 2)
#define M4U_PORT_IMGRZ_WR_GREQ		MTK_M4U_ID(M4U_LARB1_ID, 3)
#define M4U_PORT_DISP_RDMA		MTK_M4U_ID(M4U_LARB1_ID, 4)
#define M4U_PORT_PNG_LZ77W_REQ		MTK_M4U_ID(M4U_LARB1_ID, 5)
#define M4U_PORT_PNG_LNBFW_REQ		MTK_M4U_ID(M4U_LARB1_ID, 6)
#define M4U_PORT_PNG_LZ77R_REQ		MTK_M4U_ID(M4U_LARB1_ID, 7)
#define M4U_PORT_PNG_LNBFR_REQ		MTK_M4U_ID(M4U_LARB1_ID, 8)
#define M4U_PORT_PNG_PAR_REQ		MTK_M4U_ID(M4U_LARB1_ID, 9)
#define M4U_PORT_PNG_PELOUT_GREQ	MTK_M4U_ID(M4U_LARB1_ID, 10)
#define M4U_PORT_DISP_FAKE		MTK_M4U_ID(M4U_LARB1_ID, 11)
#define M4U_PORT_UNUSED2		MTK_M4U_ID(M4U_LARB1_ID, 12)

/* larb2 */
#define M4U_PORT_GFX1			MTK_M4U_ID(M4U_LARB2_ID, 0)

#endif
