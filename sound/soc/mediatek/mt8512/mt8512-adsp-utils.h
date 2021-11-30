/*
 * mt8512-adsp-utils.h  --  Mediatek 8512 adsp utility
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Mengge Wang <mengge.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MT8512_ADSP_UTILS_H_
#define _MT8512_ADSP_UTILS_H_

#include "audio_task_manager.h"
#include "audio_shared_info.h"
#include "audio_memory.h"
#include <sound/pcm.h>

enum {
	MT8512_ADSP_FE_HOSTLESS_VA = 0,
	MT8512_ADSP_FE_VA,
	MT8512_ADSP_FE_MIC_RECORD,
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	MT8512_ADSP_FE_PCM_PLAYBACK,
#endif
	MT8512_ADSP_FE_CNT,
	MT8512_ADSP_BE_START = MT8512_ADSP_FE_CNT,
	MT8512_ADSP_BE_TDM_IN = MT8512_ADSP_BE_START,
	MT8512_ADSP_BE_UL9,
	MT8512_ADSP_BE_UL2,
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	MT8512_ADSP_BE_DLM,
	MT8512_ADSP_BE_DL2,
#endif
	MT8512_ADSP_BE_END,
	MT8512_ADSP_BE_CNT = MT8512_ADSP_BE_END - MT8512_ADSP_BE_START,
};

int mt8512_adsp_get_scene_by_dai_id(int id);

int mt8512_adsp_send_ipi_cmd(struct ipi_msg_t *p_msg,
			     uint8_t task_scene,
			     uint8_t target,
			     uint8_t data_type,
			     uint8_t ack_type,
			     uint16_t msg_id,
			     uint32_t param1,
			     uint32_t param2,
			     char *payload);

int mt8512_adsp_dai_id_pack(int dai_id);

bool mt8512_adsp_need_ul_dma_copy(int id);

bool mt8512_adsp_need_dl_dma_copy(int id);

#endif
