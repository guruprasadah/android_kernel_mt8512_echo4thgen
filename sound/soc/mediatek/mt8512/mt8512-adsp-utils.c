/*
 * mt8512-adsp-utils.c  --  Mediatek 8512 adsp utility
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

#include "mt8512-adsp-utils.h"


int mt8512_adsp_get_scene_by_dai_id(int id)
{
	switch (id) {
	case MT8512_ADSP_FE_VA:
	case MT8512_ADSP_FE_MIC_RECORD:
	case MT8512_ADSP_FE_HOSTLESS_VA:
		/* FALLTHROUGH */
	case MT8512_ADSP_BE_TDM_IN:
	case MT8512_ADSP_BE_UL9:
	case MT8512_ADSP_BE_UL2:
		return TASK_SCENE_VA;
		/* FALLTHROUGH */
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	case MT8512_ADSP_FE_PCM_PLAYBACK:
	case MT8512_ADSP_BE_DLM:
	case MT8512_ADSP_BE_DL2:
		return TASK_SCENE_PCM_PLAYBACK;
#endif
	default:
		break;
	}

	return -1;
}

int mt8512_adsp_send_ipi_cmd(struct ipi_msg_t *p_msg,
			     uint8_t task_scene,
			     uint8_t target,
			     uint8_t data_type,
			     uint8_t ack_type,
			     uint16_t msg_id,
			     uint32_t param1,
			     uint32_t param2,
			     char *payload)
{
	struct ipi_msg_t ipi_msg;
	struct ipi_msg_t *msg;
	int ret = 0;

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));

	if (p_msg)
		msg = p_msg;
	else
		msg = &ipi_msg;

	ret = audio_send_ipi_msg(msg, task_scene, target, data_type,
				 ack_type, msg_id, param1, param2,
				 (char *)payload);
	if (ret != 0)
		pr_info("%s audio_send_ipi_msg (%d-%d-%d-%d) fail %d\n",
			__func__, task_scene, data_type,
			ack_type, msg_id, ret);

	return ret;
}

int mt8512_adsp_dai_id_pack(int dai_id)
{
	int id = -EINVAL;

	switch (dai_id) {
	case MT8512_ADSP_FE_HOSTLESS_VA:
		id = DAI_PACK_ID(MT8512_ADSP_FE_VA,
			DAI_VA_RECORD_TYPE, DAI_HOSTLESS);
		break;
	case MT8512_ADSP_FE_VA:
		id = DAI_PACK_ID(MT8512_ADSP_FE_VA,
			DAI_VA_RECORD_TYPE, DAI_NON_HOSTLESS);
		break;
	case MT8512_ADSP_FE_MIC_RECORD:
		id = DAI_PACK_ID(MT8512_ADSP_FE_MIC_RECORD,
			DAI_MIC_RECORD_TYPE, DAI_NON_HOSTLESS);
		break;
	case MT8512_ADSP_BE_TDM_IN:
	case MT8512_ADSP_BE_UL9:
	case MT8512_ADSP_BE_UL2:
		id = DAI_PACK_ID(dai_id - MT8512_ADSP_BE_START,
			DAI_VA_RECORD_TYPE, DAI_NON_HOSTLESS);
		break;
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	case MT8512_ADSP_FE_PCM_PLAYBACK:
		id = DAI_PACK_ID(dai_id, DAI_PLAYBACK_TYPE, DAI_NON_HOSTLESS);
		break;
	case MT8512_ADSP_BE_DLM:
	case MT8512_ADSP_BE_DL2:
		id = DAI_PACK_ID(dai_id - MT8512_ADSP_BE_START,
			DAI_PLAYBACK_TYPE, DAI_NON_HOSTLESS);
		break;
#endif
	default:
		break;
	}
	return id;
}

bool mt8512_adsp_need_ul_dma_copy(int id)
{
	switch (id) {
	case MT8512_ADSP_FE_MIC_RECORD:
	case MT8512_ADSP_FE_VA:
		return true;
	default:
		break;
	}

	return false;
}

bool mt8512_adsp_need_dl_dma_copy(int id)
{
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	switch (id) {
	case MT8512_ADSP_FE_PCM_PLAYBACK:
		return true;
	default:
		break;
	}
#endif

	return false;
}

