/*
 * mt8512-adsp-pcm.c  --  MT8512 Adsp Pcm driver
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/pm_runtime.h>
#include "mach/mtk_hifixdsp_common.h"

#include "mt8512-adsp-utils.h"
#include "mt8512-afe-common.h"
#include "mt8512-afe-utils.h"
#include "mt8512-reg.h"
#include "../common/mtk-base-afe.h"
#if defined(CONFIG_MTK_QOS_SUPPORT)
#include <linux/pm_qos.h>
#endif

struct adsp_dma_ring_buf {
	unsigned char *start_addr;
	uint32_t size_bytes;
	uint32_t hw_offset_bytes;
	uint32_t appl_offset_bytes;
};

struct cpu_dma_ring_buf {
	unsigned char *dma_buf_vaddr;
	size_t dma_buf_size;
	size_t dma_offset;
	size_t dma_period_size_bytes;
};

struct mt8512_adsp_dai_memory {
	struct snd_pcm_substream *substream;
	struct adsp_dma_ring_buf adsp_dma;
	struct cpu_dma_ring_buf  cpu_dma;
	struct io_ipc_ring_buf_shared *adsp_dma_control;
	uint32_t adsp_dma_control_paddr;
	uint32_t is_first_write;
};

struct mt8512_adsp_be_dai_data {
	struct snd_pcm_substream *substream;
	int mem_type;
};

struct mt8512_adsp_pcm_priv {
	struct device *dev;
	bool dsp_boot_run;
	bool dsp_ready;
	bool dsp_loading;
	bool dsp_suspend;
	struct mt8512_adsp_dai_memory dai_mem[MT8512_ADSP_FE_CNT];
	struct mt8512_adsp_be_dai_data be_data[MT8512_ADSP_BE_CNT];
	struct mtk_base_afe *afe;
#if defined(CONFIG_MTK_QOS_SUPPORT)
	int max_pll;
	struct pm_qos_request pm_adsp;
#endif
};

static struct mt8512_adsp_pcm_priv *g_priv;

#define IS_ADSP_READY() (g_priv->dsp_ready)

static void load_hifi4dsp_callback(void *arg);

static void mt8512_reset_dai_memory(struct mt8512_adsp_dai_memory *dai_mem)
{
	memset(dai_mem, 0, sizeof(struct mt8512_adsp_dai_memory));
}

static void mt8512_reset_dai_dma_offset(struct mt8512_adsp_dai_memory *dai_mem)
{
	dai_mem->adsp_dma.hw_offset_bytes = 0;
	dai_mem->adsp_dma.appl_offset_bytes = 0;
	dai_mem->cpu_dma.dma_offset = 0;
}

static const struct snd_pcm_hardware mt8512_adsp_pcm_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED,
	.buffer_bytes_max = 512 * 1024,
	.period_bytes_min = 512,
	.period_bytes_max = 256 * 1024,
	.periods_min = 2,
	.periods_max = 256,
};

static int mt8512_adsp_get_afe_memif_id(int dai_id)
{
	int memif_id = 0;

	switch (dai_id) {
	case MT8512_ADSP_BE_UL9:
		memif_id = MT8512_AFE_MEMIF_UL9;
		break;
	case MT8512_ADSP_BE_TDM_IN:
		memif_id = MT8512_AFE_MEMIF_UL8;
		break;
	case MT8512_ADSP_BE_UL2:
		memif_id = MT8512_AFE_MEMIF_UL2;
		break;
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	case MT8512_ADSP_BE_DLM:
		memif_id = MT8512_AFE_MEMIF_DLM;
		break;
	case MT8512_ADSP_BE_DL2:
		memif_id = MT8512_AFE_MEMIF_DL2;
		break;
#endif
	default:
		memif_id = -EINVAL;
		break;
	}
	return memif_id;
}

static int mt8512_adsp_pcm_fe_startup(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id); /* SCENE_VA_TASK */
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	snd_soc_set_runtime_hwparams(substream,
				     &mt8512_adsp_pcm_pcm_hardware);

	if (scene < 0)
		return -EINVAL;

	ret = mt8512_adsp_send_ipi_cmd(NULL,
				       scene,
				       AUDIO_IPI_LAYER_TO_DSP,
				       AUDIO_IPI_MSG_ONLY,
				       AUDIO_IPI_MSG_NEED_ACK,
				       MSG_TO_DSP_HOST_PORT_STARTUP,
				       0, 0, NULL);

	priv->dai_mem[id].substream = substream;

	return ret;
}

static void mt8512_adsp_pcm_fe_shutdown(struct snd_pcm_substream *substream,
					 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);

	if (!IS_ADSP_READY())
		return;

	if (scene < 0)
		return;

	mt8512_adsp_send_ipi_cmd(NULL,
				 scene,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_HOST_CLOSE,
				 0, 0, NULL);

	mt8512_reset_dai_memory(&priv->dai_mem[id]);
}

static int mt8512_adsp_pcm_fe_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params,
					 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	struct host_ipc_msg_hw_param ipc_hw_param;
	struct dsp_ipc_msg_hw_param ack_hw_param;
	struct ipi_msg_t ipi_msg;
	struct mt8512_adsp_dai_memory *dai_mem = &priv->dai_mem[id];
	struct adsp_dma_ring_buf *adsp_dma = &dai_mem->adsp_dma;
	struct cpu_dma_ring_buf *cpu_dma = &dai_mem->cpu_dma;
	phys_addr_t paddr;
	void __iomem *vaddr;
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	if (scene < 0)
		return -EINVAL;

	memset(&ipi_msg, 0, sizeof(struct ipi_msg_t));
	/* TODO config share buffer size by DTS */
	ipc_hw_param.dai_id = mt8512_adsp_dai_id_pack(id);
	ipc_hw_param.sample_rate = params_rate(params);
	ipc_hw_param.channel_num = params_channels(params);
	ipc_hw_param.bitwidth = params_width(params);
	ipc_hw_param.period_size = params_period_size(params);
	ipc_hw_param.period_count = params_periods(params);

	ret = mt8512_adsp_send_ipi_cmd(&ipi_msg,
				       scene,
				       AUDIO_IPI_LAYER_TO_DSP,
				       AUDIO_IPI_PAYLOAD,
				       AUDIO_IPI_MSG_NEED_ACK,
				       MSG_TO_DSP_HOST_HW_PARAMS,
				       sizeof(ipc_hw_param),
				       0,
				       (char *)&ipc_hw_param);
	if (ret != 0)
		return ret;

	if (ipi_msg.ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		dev_dbg(priv->dev,
			"unexpected ack type %u\n",
			ipi_msg.ack_type);
		return -EINVAL;
	}

	AUDIO_IPC_COPY_DSP_HW_PARAM(ipi_msg.payload, &ack_hw_param);

	/* NOTICE: now only support DRAM share buffer */
	if (ack_hw_param.adsp_dma.mem_type != AFE_MEM_TYPE_DRAM)
		return -ENOMEM;

	/* get adsp dma control block between adsp pcm driver and adsp */
	paddr = ack_hw_param.adsp_dma.dma_paddr;
	paddr =
		adsp_hal_phys_addr_dsp2cpu(paddr);
	vaddr =
		adsp_get_shared_sysram_phys2virt(paddr);
	dai_mem->adsp_dma_control_paddr = (uint32_t)paddr;
	dai_mem->adsp_dma_control =
	    (struct io_ipc_ring_buf_shared *)vaddr;

	/* config dma between adsp pcm driver and adsp */
	paddr = dai_mem->adsp_dma_control->start_addr;
	paddr =
		adsp_hal_phys_addr_dsp2cpu(paddr);
	vaddr =
		adsp_get_shared_sysram_phys2virt(paddr);
	adsp_dma->start_addr = (unsigned char *)vaddr;
	adsp_dma->size_bytes =
		dai_mem->adsp_dma_control->size_bytes;
	adsp_dma->hw_offset_bytes =
		dai_mem->adsp_dma_control->ptr_to_hw_offset_bytes;
	adsp_dma->appl_offset_bytes =
		dai_mem->adsp_dma_control->ptr_to_appl_offset_bytes;

	/* config dma between adsp pcm driver  and user space */
	cpu_dma->dma_buf_vaddr = substream->runtime->dma_area;
	cpu_dma->dma_buf_size = substream->runtime->dma_bytes;
	cpu_dma->dma_period_size_bytes =
		params_period_bytes(params);

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	if (id == MT8512_ADSP_FE_PCM_PLAYBACK)
		dai_mem->is_first_write = 1;
#endif

	return 0;
}

static int mt8512_adsp_pcm_fe_hw_free(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	struct host_ipc_msg_hw_free ipc_hw_free;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;

	ipc_hw_free.dai_id = mt8512_adsp_dai_id_pack(id);
	mt8512_adsp_send_ipi_cmd(NULL,
			       scene,
			       AUDIO_IPI_LAYER_TO_DSP,
			       AUDIO_IPI_PAYLOAD,
			       AUDIO_IPI_MSG_NEED_ACK,
			       MSG_TO_DSP_HOST_HW_FREE,
			       sizeof(ipc_hw_free),
			       0,
			       (char *)&ipc_hw_free);

	return snd_pcm_lib_free_pages(substream);
}

static int mt8512_adsp_pcm_fe_prepare(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;


	ret = mt8512_adsp_send_ipi_cmd(NULL,
				       scene,
				       AUDIO_IPI_LAYER_TO_DSP,
				       AUDIO_IPI_MSG_ONLY,
				       AUDIO_IPI_MSG_NEED_ACK,
				       MSG_TO_DSP_HOST_PREPARE,
				       0, 0, NULL);

	return ret;
}

static int mt8512_adsp_pcm_fe_trigger(struct snd_pcm_substream *substream,
				       int cmd,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	struct mt8512_adsp_dai_memory *dai_mem = &priv->dai_mem[id];
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	struct host_ipc_msg_trigger ipc_trigger;
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;

	ipc_trigger.dai_id = mt8512_adsp_dai_id_pack(id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8512_adsp_send_ipi_cmd(NULL,
					 scene,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_DIRECT_SEND,
					 MSG_TO_DSP_HOST_TRIGGER_START,
					 sizeof(ipc_trigger),
					 0,
					 (char *)&ipc_trigger);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8512_adsp_send_ipi_cmd(NULL,
					 scene,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_DIRECT_SEND,
					 MSG_TO_DSP_HOST_TRIGGER_STOP,
					 sizeof(ipc_trigger),
					 0,
					 (char *)&ipc_trigger);
		mt8512_reset_dai_dma_offset(dai_mem);
		break;
	default:
		break;
	}

	return ret;
}

static int mt8512_adsp_pcm_be_startup(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = priv->afe;
	int ret = 0;
	uint32_t dai_id;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;
	dai_id = mt8512_adsp_dai_id_pack(id);
	ret = mt8512_adsp_send_ipi_cmd(NULL,
				       scene,
				       AUDIO_IPI_LAYER_TO_DSP,
				       AUDIO_IPI_PAYLOAD,
				       AUDIO_IPI_MSG_NEED_ACK,
				       MSG_TO_DSP_DSP_PORT_STARTUP,
				       sizeof(uint32_t),
				       0,
				       (char *)&dai_id);
	if (ret == 0)
		pm_runtime_get_sync(afe->dev);

	return ret;
}

static void mt8512_adsp_pcm_be_shutdown(struct snd_pcm_substream *substream,
					 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = priv->afe;
	uint32_t dai_id;

	if (!IS_ADSP_READY())
		return;

	if (scene < 0)
		return;
	dai_id = mt8512_adsp_dai_id_pack(id);
	mt8512_adsp_send_ipi_cmd(NULL,
				 scene,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_DSP_CLOSE,
				 sizeof(uint32_t),
				 0,
				 (char *)&dai_id);

	pm_runtime_put_sync(afe->dev);
}

static int mt8512_adsp_pcm_be_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params,
					 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	int memif_id = mt8512_adsp_get_afe_memif_id(id);
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mt8512_adsp_be_dai_data *adsp_be =
		&priv->be_data[id - MT8512_ADSP_BE_START];
	struct mtk_base_afe *afe = priv->afe;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_adsp_data *afe_adsp = &(afe_priv->adsp_data);
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	struct host_ipc_msg_hw_param ipc_hw_param;
	struct dsp_ipc_msg_hw_param ack_hw_param;
	struct ipi_msg_t ipi_msg;
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;

	memset(&ipi_msg, 0, sizeof(ipi_msg));

	ipc_hw_param.dai_id = mt8512_adsp_dai_id_pack(id);
	ipc_hw_param.sample_rate = params_rate(params);
	ipc_hw_param.channel_num = params_channels(params);
	ipc_hw_param.bitwidth = params_width(params);
	ipc_hw_param.period_size = params_period_size(params);
	ipc_hw_param.period_count = params_periods(params);

	/* Config AFE DMA buffer */
	memif->buffer_size = params_buffer_bytes(params);
	ipc_hw_param.adsp_dma.dma_paddr = 0;
	if (adsp_be->mem_type == AFE_MEM_TYPE_AFE_SRAM) {
		unsigned int paddr, size;

		afe_adsp->get_afe_memif_sram(afe, memif_id, &paddr, &size);
		if (paddr != 0 && memif->buffer_size < size) {
			ipc_hw_param.adsp_dma.dma_paddr = paddr;
			ipc_hw_param.adsp_dma.mem_type = AFE_MEM_TYPE_AFE_SRAM;
		} else {
			ipc_hw_param.adsp_dma.mem_type = AFE_MEM_TYPE_DRAM;
		}
	} else {
		ipc_hw_param.adsp_dma.mem_type = (uint32_t)adsp_be->mem_type;
	}

	ret = mt8512_adsp_send_ipi_cmd(&ipi_msg,
				       scene,
				       AUDIO_IPI_LAYER_TO_DSP,
				       AUDIO_IPI_PAYLOAD,
				       AUDIO_IPI_MSG_NEED_ACK,
				       MSG_TO_DSP_DSP_HW_PARAMS,
				       sizeof(ipc_hw_param),
				       0,
				       (char *)&ipc_hw_param);

	if (ret != 0)
		return ret;

	if (ipi_msg.ack_type != AUDIO_IPI_MSG_ACK_BACK) {
		dev_dbg(priv->dev,
			"unexpected ack type %u\n",
			ipi_msg.ack_type);
		return -EINVAL;
	}
	AUDIO_IPC_COPY_DSP_HW_PARAM(ipi_msg.payload, &ack_hw_param);
	if (ack_hw_param.adsp_dma.dma_paddr == 0)
		return -ENOMEM;

	/* Notice: for AFE & DTCM do not need address convert */
	if (ack_hw_param.adsp_dma.mem_type != AFE_MEM_TYPE_AFE_SRAM)
		memif->phys_buf_addr =
		    adsp_hal_phys_addr_dsp2cpu(ack_hw_param.adsp_dma.dma_paddr);
	else
		memif->phys_buf_addr = ack_hw_param.adsp_dma.dma_paddr;

	ret = afe_adsp->set_afe_memif(afe,
				       memif_id,
				       params_rate(params),
				       params_channels(params),
				       params_format(params));
	if (ret < 0)
		return ret;

	return ret;
}

static int mt8512_adsp_pcm_be_hw_free(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	uint32_t dai_id;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;
	dai_id = mt8512_adsp_dai_id_pack(id);
	mt8512_adsp_send_ipi_cmd(NULL,
				 scene,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_DSP_HW_FREE,
				 sizeof(uint32_t),
				 0,
				 (char *)&dai_id);

	return 0;
}

static int mt8512_adsp_pcm_be_prepare(struct snd_pcm_substream *substream,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	int ret = 0;
	uint32_t dai_id;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return -EINVAL;
	dai_id = mt8512_adsp_dai_id_pack(id);
	ret = mt8512_adsp_send_ipi_cmd(NULL,
				 scene,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_DSP_PREPARE,
				 sizeof(uint32_t),
				 0,
				 (char *)&dai_id);

	return ret;
}

static int mt8512_adsp_pcm_be_trigger(struct snd_pcm_substream *substream,
				       int cmd,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	int id = rtd->cpu_dai->id;
	int scene = mt8512_adsp_get_scene_by_dai_id(id);
	int memif_id = mt8512_adsp_get_afe_memif_id(id);
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = priv->afe;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_adsp_data *afe_adsp = &(afe_priv->adsp_data);
	struct host_ipc_msg_trigger ipc_trigger;
	int ret = 0;

	if (!IS_ADSP_READY())
		return -ENODEV;

	if (scene < 0)
		return 0;

	ipc_trigger.dai_id = mt8512_adsp_dai_id_pack(id);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		afe_adsp->set_afe_memif_enable(afe,
					       memif_id,
					       runtime->rate,
					       runtime->period_size,
					       1);
		mt8512_adsp_send_ipi_cmd(NULL,
					 scene,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_DIRECT_SEND,
					 MSG_TO_DSP_DSP_TRIGGER_START,
					 sizeof(ipc_trigger),
					 0,
					 (char *)&ipc_trigger);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		afe_adsp->set_afe_memif_enable(afe,
					       memif_id,
					       runtime->rate,
					       runtime->period_size,
					       0);
		mt8512_adsp_send_ipi_cmd(NULL,
					 scene,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_DIRECT_SEND,
					 MSG_TO_DSP_DSP_TRIGGER_STOP,
					 sizeof(ipc_trigger),
					 0,
					 (char *)&ipc_trigger);
		break;
	default:
		break;
	}

	return ret;
}

static int mt8512_adsp_hostless_active(void)
{
	struct snd_pcm_substream *substream;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *dai;

	substream = g_priv->dai_mem[MT8512_ADSP_FE_HOSTLESS_VA].substream;
	if (substream == NULL)
		return 0;
	rtd = substream->private_data;
	if (rtd == NULL)
		return 0;
	dai = rtd->cpu_dai;

	if (dai != NULL)
		return dai->active;
	else
		return 0;
}

static int mt8512_adsp_dai_suspend(struct snd_soc_dai *dai)
{
	struct mt8512_adsp_pcm_priv *priv = dev_get_drvdata(dai->dev);
	bool hostless;

	/* if dsp is not ready or on, ignore suspend */
	if (!IS_ADSP_READY())
		return 0;

	if (priv->dsp_suspend)
		return 0;

	hostless = (mt8512_adsp_hostless_active() > 0);
	if (hostless) {
		/* Do VAD Low power action */
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_AUDIO_CONTROLLER,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_AP_SUSPEND_T,
					 0, 0, NULL);
	}
	/* If there is no hostless active */
	/* Whatever other DAIs is active or not, do nothing now */
	/* DSP may in WFI mode, or clock off? */

	priv->dsp_suspend = true;

	return 0;
}

static int mt8512_adsp_dai_resume(struct snd_soc_dai *dai)
{
	struct mt8512_adsp_pcm_priv *priv = dev_get_drvdata(dai->dev);
	bool hostless;

	/* if dsp is not ready or on, ignore resume */
	if (!IS_ADSP_READY())
		return 0;

	if (!priv->dsp_suspend)
		return 0;

	hostless = (mt8512_adsp_hostless_active() > 0);
	if (hostless) {
		/* Do hostless low power control */
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_AUDIO_CONTROLLER,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_AP_RESUME_T,
					 0, 0, NULL);
	}
	/* If there is no hostless active */
	/* Whatever other DAIs is active or not, do nothing now */
	/* DSP may in WFI mode */

	priv->dsp_suspend = false;
	return 0;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt8512_adsp_pcm_fe_dai_ops = {
	.startup	= mt8512_adsp_pcm_fe_startup,
	.shutdown	= mt8512_adsp_pcm_fe_shutdown,
	.hw_params	= mt8512_adsp_pcm_fe_hw_params,
	.hw_free	= mt8512_adsp_pcm_fe_hw_free,
	.prepare	= mt8512_adsp_pcm_fe_prepare,
	.trigger	= mt8512_adsp_pcm_fe_trigger,
};

/* BE DAIs */
static const struct snd_soc_dai_ops mt8512_adsp_pcm_be_dai_ops = {
	.startup	= mt8512_adsp_pcm_be_startup,
	.shutdown	= mt8512_adsp_pcm_be_shutdown,
	.hw_params	= mt8512_adsp_pcm_be_hw_params,
	.hw_free	= mt8512_adsp_pcm_be_hw_free,
	.prepare	= mt8512_adsp_pcm_be_prepare,
	.trigger	= mt8512_adsp_pcm_be_trigger,
};

static struct snd_soc_dai_driver mt8512_adsp_pcm_dais[] = {
	/* FE DAIs */
	{
		.name = "FE_HOSTLESS_VA",
		.id = MT8512_ADSP_FE_HOSTLESS_VA,
		.suspend = mt8512_adsp_dai_suspend,
		.resume = mt8512_adsp_dai_resume,
		.capture = {
			.stream_name = "FE_HOSTLESS_VA",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_fe_dai_ops,
	}, {
		.name = "FE_VA",
		.id = MT8512_ADSP_FE_VA,
		.suspend = mt8512_adsp_dai_suspend,
		.resume = mt8512_adsp_dai_resume,
		.capture = {
			.stream_name = "FE_VA",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_fe_dai_ops,
	}, {
		.name = "FE_MICR",
		.id = MT8512_ADSP_FE_MIC_RECORD,
		.suspend = mt8512_adsp_dai_suspend,
		.resume = mt8512_adsp_dai_resume,
		.capture = {
			.stream_name = "FE_MICR",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_fe_dai_ops,
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	}, {
		.name = "FE_PCM_PLAYBACK",
		.id = MT8512_ADSP_FE_PCM_PLAYBACK,
		.suspend = mt8512_adsp_dai_suspend,
		.resume = mt8512_adsp_dai_resume,
		.playback = {
			.stream_name = "FE_PCM_PLAYBACK",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_fe_dai_ops,
#endif
	}, {
	/* BE DAIs */
		.name = "BE_TDM_IN",
		.id = MT8512_ADSP_BE_TDM_IN,
		.capture = {
			.stream_name = "BE_TDM_IN Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_be_dai_ops,
	}, {
		.name = "BE_UL9_IN",
		.id = MT8512_ADSP_BE_UL9,
		.capture = {
			.stream_name = "BE_UL9_IN Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_be_dai_ops,
	}, {
		.name = "BE_UL2_IN",
		.id = MT8512_ADSP_BE_UL2,
		.capture = {
			.stream_name = "BE_UL2_IN Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_be_dai_ops,
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	}, {
		.name = "BE_DLM_OUT",
		.id = MT8512_ADSP_BE_DLM,
		.playback = {
			.stream_name = "BE_DLM_OUT Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_be_dai_ops,
	}, {
		.name = "BE_DL2_OUT",
		.id = MT8512_ADSP_BE_DL2,
		.playback = {
			.stream_name = "BE_DL2_OUT Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S32_LE,
		},
		.ops = &mt8512_adsp_pcm_be_dai_ops,
#endif
	},
};

static const char * const vain_text[] = {
	"OPEN", "TDMIN", "UL9", "UL2"
};

static const char * const vain_io_text[] = {
	"OPEN", "TDMIN_IO", "UL9_IO", "UL2_IO"
};

static SOC_ENUM_SINGLE_VIRT_DECL(vain_enum, vain_text);
static SOC_ENUM_SINGLE_VIRT_DECL(vain_io_enum, vain_io_text);

static const struct snd_kcontrol_new vain_mux =
	    SOC_DAPM_ENUM("VA_IN Source", vain_enum);

static const struct snd_kcontrol_new vain_io_mux =
	    SOC_DAPM_ENUM("VA_IN_IO Source", vain_io_enum);

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
static const char * const dl_out_text[] = {
	"OPEN", "DLM", "DL2"
};

static const char * const dl_io_text[] = {
	"OPEN", "DLM_IO", "DL2_IO"
};

static SOC_ENUM_SINGLE_DECL(dl_out_enum,
	SND_SOC_NOPM, 0, dl_out_text);
static SOC_ENUM_SINGLE_DECL(dl_io_enum,
	SND_SOC_NOPM, 0, dl_io_text);

static const struct snd_kcontrol_new dl_out_demux =
	    SOC_DAPM_ENUM("DL_OUT Sink", dl_out_enum);

static const struct snd_kcontrol_new dl_io_demux =
	    SOC_DAPM_ENUM("DL_IO Sink", dl_io_enum);

#endif

static const struct snd_kcontrol_new va_tdmin_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_soc_dapm_widget mt8512_adsp_pcm_widgets[] = {
	SND_SOC_DAPM_MIXER("VA_UL2_IO", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("VA_TDMIN_IO", SND_SOC_NOPM, 0, 0,
			    &va_tdmin_enable_ctl),
	SND_SOC_DAPM_MIXER("VA_UL9_IO", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("VA_IN Mux", SND_SOC_NOPM, 0, 0, &vain_mux),
	SND_SOC_DAPM_MUX("VA_IN_IO Mux", SND_SOC_NOPM, 0, 0, &vain_io_mux),

	SND_SOC_DAPM_INPUT("VA UL2 In"),
	SND_SOC_DAPM_INPUT("VA TDMIN In"),
	SND_SOC_DAPM_INPUT("VA UL9 In"),

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	SND_SOC_DAPM_AIF_OUT("PCM_OUT_DLM", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("PCM_OUT_DL2", NULL, 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DEMUX("PCM_OUT SEL", SND_SOC_NOPM, 0, 0, &dl_out_demux),
	SND_SOC_DAPM_DEMUX("PCM_OUT_IO SEL", SND_SOC_NOPM, 0, 0, &dl_io_demux),

	SND_SOC_DAPM_OUTPUT("PCM DLM OUT"),
	SND_SOC_DAPM_OUTPUT("PCM DL2 OUT"),
#endif
};

static const struct snd_soc_dapm_route mt8512_adsp_pcm_routes[] = {
	{"VA_UL2_IO", NULL, "O34"},
	{"VA_UL2_IO", NULL, "O35"},
	{"VA_UL2_IO", NULL, "O36"},
	{"VA_UL2_IO", NULL, "O37"},
	{"VA_UL2_IO", NULL, "O38"},
	{"VA_UL2_IO", NULL, "O39"},
	{"VA_UL2_IO", NULL, "O40"},
	{"VA_UL2_IO", NULL, "O41"},
	{"VA_IN_IO Mux", "UL2_IO", "VA_UL2_IO"},

	{"BE_UL2_IN Capture", NULL, "VA UL2 In"},
	{"VA_IN Mux", "UL2", "BE_UL2_IN Capture"},

	{"VA_TDMIN_IO", "Switch", "ETDM1 Capture"},
	{"VA_IN_IO Mux", "TDMIN_IO", "VA_TDMIN_IO"},

	{"BE_TDM_IN Capture", NULL, "VA TDMIN In"},
	{"VA_IN Mux", "TDMIN", "BE_TDM_IN Capture"},

	{"VA_UL9_IO", NULL, "O26"},
	{"VA_UL9_IO", NULL, "O27"},
	{"VA_UL9_IO", NULL, "O28"},
	{"VA_UL9_IO", NULL, "O29"},
	{"VA_UL9_IO", NULL, "O30"},
	{"VA_UL9_IO", NULL, "O31"},
	{"VA_UL9_IO", NULL, "O32"},
	{"VA_UL9_IO", NULL, "O33"},
	{"VA_UL9_IO", NULL, "O34"},
	{"VA_UL9_IO", NULL, "O35"},
	{"VA_UL9_IO", NULL, "O36"},
	{"VA_UL9_IO", NULL, "O37"},
	{"VA_UL9_IO", NULL, "O38"},
	{"VA_UL9_IO", NULL, "O39"},
	{"VA_UL9_IO", NULL, "O40"},
	{"VA_UL9_IO", NULL, "O41"},
	{"VA_IN_IO Mux", "UL9_IO", "VA_UL9_IO"},

	{"BE_UL9_IN Capture", NULL, "VA UL9 In"},
	{"VA_IN Mux", "UL9", "BE_UL9_IN Capture"},

	{"FE_HOSTLESS_VA", NULL, "VA_IN_IO Mux"},
	{"FE_VA", NULL, "VA_IN_IO Mux"},
	{"FE_MICR", NULL, "VA_IN_IO Mux"},

	{"FE_HOSTLESS_VA", NULL, "VA_IN Mux"},
	{"FE_VA", NULL, "VA_IN Mux"},
	{"FE_MICR", NULL, "VA_IN Mux"},

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	{"I22", NULL, "PCM_OUT_DLM"},
	{"I23", NULL, "PCM_OUT_DLM"},
	{"I24", NULL, "PCM_OUT_DLM"},
	{"I25", NULL, "PCM_OUT_DLM"},
	{"I26", NULL, "PCM_OUT_DLM"},
	{"I27", NULL, "PCM_OUT_DLM"},
	{"I28", NULL, "PCM_OUT_DLM"},
	{"I29", NULL, "PCM_OUT_DLM"},

	{"I40", NULL, "PCM_OUT_DL2"},
	{"I41", NULL, "PCM_OUT_DL2"},

	{"PCM_OUT_DLM", "DLM_IO", "PCM_OUT_IO SEL"},
	{"PCM_OUT_DL2", "DL2_IO", "PCM_OUT_IO SEL"},

	{"PCM_OUT_IO SEL", NULL, "FE_PCM_PLAYBACK"},


	{"PCM DLM OUT", NULL, "BE_DLM_OUT Playback"},
	{"PCM DL2 OUT", NULL, "BE_DL2_OUT Playback"},

	{"BE_DLM_OUT Playback", "DLM", "PCM_OUT SEL"},
	{"BE_DL2_OUT Playback", "DL2", "PCM_OUT SEL"},

	{"PCM_OUT SEL", NULL, "FE_PCM_PLAYBACK"},

#endif
};

static int mt8512_adsp_low_power_init(struct mt8512_adsp_pcm_priv *priv)
{
	struct mtk_base_afe *afe = priv->afe;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_adsp_data *afe_adsp = &(afe_priv->adsp_data);

	dev_info(afe->dev, "%s\n", __func__);

	afe_adsp->set_afe_init(afe);

#if defined(CONFIG_MTK_QOS_SUPPORT)
	if (priv->max_pll == 400000000)
		pm_qos_update_request(&priv->pm_adsp, 1); /* VCORE_OPP_1 */
	else if (priv->max_pll == 600000000)
		pm_qos_update_request(&priv->pm_adsp, 0); /* VCORE_OPP_0 */
#endif

	return 0;
}

static int mt8512_adsp_low_power_uninit(struct mt8512_adsp_pcm_priv *priv)
{
	struct mtk_base_afe *afe = priv->afe;
	struct mt8512_afe_private *afe_priv = afe->platform_priv;
	struct mt8512_adsp_data *afe_adsp = &(afe_priv->adsp_data);

	dev_info(afe->dev, "%s\n", __func__);

#if defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_update_request(&priv->pm_adsp, PM_QOS_VCORE_OPP_DEFAULT_VALUE);
#endif

	afe_adsp->set_afe_uninit(afe);
	return 0;
}

static int mt8512_adsp_enable_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = IS_ADSP_READY();
	return 0;
}

static int mt8512_adsp_enable_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *plat = snd_soc_kcontrol_platform(kcontrol);
	struct mt8512_adsp_pcm_priv *priv = snd_soc_platform_get_drvdata(plat);
	int ret = 0;
	int enable = ucontrol->value.integer.value[0];

	if ((enable && IS_ADSP_READY()) || (!enable && !IS_ADSP_READY()))
		return 0;

	/* already loading */
	if (priv->dsp_loading)
		return -EBUSY;

	if (enable) {
		priv->dsp_loading = true;
#if defined(CONFIG_MTK_QOS_SUPPORT)
		pm_qos_update_request(&priv->pm_adsp, 0); /* VCORE_OPP_0 */
#endif
		ret =
		async_load_hifixdsp_bin_and_run(load_hifi4dsp_callback, priv);
		if (ret) {
			dev_info(plat->dev,
				"%s async_load_hifi4dsp_bin_and_run fail %d\n",
				__func__, ret);
			priv->dsp_loading = false;
		}
	}  else {
		/* TODO add stop function */
		ret = hifixdsp_stop_run();
		if (ret) {
			dev_info(plat->dev,
				"%s hifixdsp_stop_run fail %d\n",
				__func__, ret);
		} else {
			priv->dsp_ready = false;
			mt8512_adsp_low_power_uninit(priv);
		}
	}

	return ret;
}

static int mt8512_adsp_ap_suspend_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	/* TODO add suspend/resume value */
	return 0;
}

static int mt8512_adsp_ap_suspend_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_AUDIO_CONTROLLER,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_AP_SUSPEND_T,
					 0, 0, NULL);
	} else {
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_AUDIO_CONTROLLER,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_AP_RESUME_T,
					 0, 0, NULL);
	}

	return 0;
}

static int mt8512_adsp_va_voiceupload_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt8512_adsp_va_voiceupload_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_VOICE_UPLOAD_DONE,
					 0, 0, NULL);
	}
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int mt8512_adsp_va_vad_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt8512_adsp_va_vad_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct ipc_va_params params;

	params.va_type = VA_VAD;
	if (ucontrol->value.integer.value[0]) {
		params.enable_flag = 1;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_VAD,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	} else {
		params.enable_flag = 0;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_VAD,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	}

	return 0;
}

static int mt8512_adsp_va_aec_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt8512_adsp_va_aec_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct ipc_va_params params;

	params.va_type = VA_AEC;
	if (ucontrol->value.integer.value[0]) {
		params.enable_flag = 1;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_AEC,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	} else {
		params.enable_flag = 0;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_AEC,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	}

	return 0;
}

static int mt8512_adsp_va_ww_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt8512_adsp_va_ww_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct ipc_va_params params;

	params.va_type = VA_KEYWORD;
	if (ucontrol->value.integer.value[0]) {
		params.enable_flag = 1;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_KEYWORD,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	} else {
		params.enable_flag = 0;
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_PAYLOAD,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_AEC,
					 sizeof(struct ipc_va_params),
					 sizeof(struct ipc_va_params),
					 (char *)&params);
	}

	return 0;
}

static int mt8512_adsp_va_beamforming_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	uint32_t beamforming;
	struct ipi_msg_t ipi_msg;

	if (!IS_ADSP_READY())
		return -ENODEV;

	mt8512_adsp_send_ipi_cmd(&ipi_msg,
				 TASK_SCENE_VA,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_SCENE_VA_BEAMFORMING,
				 sizeof(uint32_t),
				 sizeof(uint32_t),
				 (char *)&beamforming);

	if (ipi_msg.ack_type != AUDIO_IPI_MSG_ACK_BACK)
		beamforming = 0;

	ucontrol->value.integer.value[0] = beamforming;

	return 0;
}

static int mt8512_adsp_va_force_ok_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8512_adsp_va_force_ok_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_VAD_FORCE_OK,
					 0, 0, NULL);
	} else {
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_VAD_FORCE_OK,
					 0, 0, NULL);
		msleep(150);
		mt8512_adsp_send_ipi_cmd(NULL,
					 TASK_SCENE_VA,
					 AUDIO_IPI_LAYER_TO_DSP,
					 AUDIO_IPI_MSG_ONLY,
					 AUDIO_IPI_MSG_NEED_ACK,
					 MSG_TO_DSP_SCENE_VA_KEYWORD_FORCE_OK,
					 0, 0, NULL);
	}

	return 0;
}

static int mt8512_adsp_create_stress_task_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8512_adsp_create_stress_task_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		mt8512_adsp_send_ipi_cmd(NULL,
				 TASK_SCENE_AUDIO_CONTROLLER,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_CREATE_STRESS_TEST_T,
				 0, 0, NULL);
	} else {
		mt8512_adsp_send_ipi_cmd(NULL,
				 TASK_SCENE_AUDIO_CONTROLLER,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_DESTROY_STRESS_TEST_T,
				 0, 0, NULL);
	}

	return 0;
}

#if 0
static int mt8168_adsp_dcxo_sw_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8168_adsp_dcxo_sw_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0])
		clk_buf_mode_set(CLK_BUF_BB_MD, 0);
	else
		clk_buf_mode_set(CLK_BUF_BB_MD, 1);

	return 0;
}
#endif

static const struct snd_kcontrol_new mt8512_adsp_controls[] = {
	SOC_SINGLE_BOOL_EXT("ADSP Enable",
			    0,
			    mt8512_adsp_enable_get,
			    mt8512_adsp_enable_put),
	SOC_SINGLE_BOOL_EXT("VA VAD Enable",
			    0,
			    mt8512_adsp_va_vad_get,
			    mt8512_adsp_va_vad_put),
	SOC_SINGLE_BOOL_EXT("VA AEC Enable",
			    0,
			    mt8512_adsp_va_aec_get,
			    mt8512_adsp_va_aec_put),
	SOC_SINGLE_BOOL_EXT("VA WakeWord Enable",
			    0,
			    mt8512_adsp_va_ww_get,
			    mt8512_adsp_va_ww_put),
	SOC_SINGLE_EXT("VA Beamforming",
			    0,
			    0,
			    0x800000,
			    0,
			    mt8512_adsp_va_beamforming_get,
			    0),
	SOC_SINGLE_BOOL_EXT("VA VOICEUPLOAD Done",
			    0,
			    mt8512_adsp_va_voiceupload_get,
			    mt8512_adsp_va_voiceupload_put),
	SOC_SINGLE_BOOL_EXT("AP Suspend Test",
			    0,
			    mt8512_adsp_ap_suspend_get,
			    mt8512_adsp_ap_suspend_put),

	SOC_SINGLE_BOOL_EXT("VA Force Test",
			    0,
			    mt8512_adsp_va_force_ok_get,
			    mt8512_adsp_va_force_ok_put),
	SOC_SINGLE_BOOL_EXT("ADSP Create Test Task",
			    0,
			    mt8512_adsp_create_stress_task_get,
			    mt8512_adsp_create_stress_task_put),
#if 0
	SOC_SINGLE_BOOL_EXT("DCXO SW",
			    0,
			    mt8168_adsp_dcxo_sw_get,
			    mt8168_adsp_dcxo_sw_put),
#endif
};

static const struct snd_soc_component_driver mt8512_adsp_pcm_dai_comp_drv = {
	.name = "mt8512-adsp-pcm-dai",
	.dapm_widgets = mt8512_adsp_pcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8512_adsp_pcm_widgets),
	.dapm_routes = mt8512_adsp_pcm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8512_adsp_pcm_routes),
	.controls = mt8512_adsp_controls,
	.num_controls = ARRAY_SIZE(mt8512_adsp_controls),
};

static void mt8512_adsp_pcm_data_copy(struct snd_pcm_substream *substream,
	struct mt8512_adsp_dai_memory *dai_mem)
{
	unsigned char *adsp_dma_buf_vaddr =
		   dai_mem->adsp_dma.start_addr;
	uint32_t adsp_dma_buf_size = dai_mem->adsp_dma.size_bytes;
	uint32_t adsp_dma_hw_off = 0;
	uint32_t adsp_dma_appl_off = dai_mem->adsp_dma.appl_offset_bytes;
	unsigned char *cpu_dma_buf_vaddr =
		    dai_mem->cpu_dma.dma_buf_vaddr;
	uint32_t cpu_dma_buf_size = dai_mem->cpu_dma.dma_buf_size;
	uint32_t cpu_dma_offset = dai_mem->cpu_dma.dma_offset;
	uint32_t period_size_bytes = dai_mem->cpu_dma.dma_period_size_bytes;
	uint32_t avail_bytes;
	uint32_t cpu_dma_free_bytes;
	uint32_t copy_bytes;

	adsp_dma_hw_off =
		dai_mem->adsp_dma_control->ptr_to_hw_offset_bytes;

	if (adsp_dma_hw_off >= adsp_dma_appl_off) {
		avail_bytes = adsp_dma_hw_off - adsp_dma_appl_off;
	} else {
		avail_bytes = adsp_dma_buf_size - adsp_dma_appl_off +
			adsp_dma_hw_off;
	}

	cpu_dma_free_bytes = snd_pcm_capture_hw_avail(substream->runtime);
	cpu_dma_free_bytes =
		frames_to_bytes(substream->runtime, cpu_dma_free_bytes);

	if (avail_bytes >= cpu_dma_free_bytes)
		avail_bytes = cpu_dma_free_bytes - period_size_bytes;

	if (avail_bytes < period_size_bytes)
		return;

	copy_bytes = (avail_bytes / period_size_bytes) * period_size_bytes;

	while (copy_bytes > 0) {
		uint32_t from_bytes = 0;

		if (adsp_dma_hw_off >= adsp_dma_appl_off)
			from_bytes = adsp_dma_hw_off - adsp_dma_appl_off;
		else
			from_bytes = adsp_dma_buf_size - adsp_dma_appl_off;

		if (from_bytes > copy_bytes)
			from_bytes = copy_bytes;

		while (from_bytes > 0) {
			uint32_t to_bytes = 0;

			if (cpu_dma_offset + from_bytes < cpu_dma_buf_size)
				to_bytes = from_bytes;
			else
				to_bytes = cpu_dma_buf_size - cpu_dma_offset;

			memcpy(cpu_dma_buf_vaddr + cpu_dma_offset,
				adsp_dma_buf_vaddr + adsp_dma_appl_off,
				to_bytes);

			from_bytes -= to_bytes;
			copy_bytes -= to_bytes;

			cpu_dma_offset = (cpu_dma_offset + to_bytes) %
				cpu_dma_buf_size;
			adsp_dma_appl_off = (adsp_dma_appl_off + to_bytes) %
				adsp_dma_buf_size;
		}
	}

	dai_mem->adsp_dma_control->ptr_to_appl_offset_bytes =
		adsp_dma_appl_off;
	dai_mem->adsp_dma.appl_offset_bytes = adsp_dma_appl_off;
	dai_mem->adsp_dma.hw_offset_bytes = adsp_dma_hw_off;
	dai_mem->cpu_dma.dma_offset = cpu_dma_offset;
}

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
static void mt8512_adsp_pcm_data_write(struct snd_pcm_substream *substream,
	struct mt8512_adsp_dai_memory *dai_mem)
{
	unsigned char *adsp_dma_buf_vaddr =
		   dai_mem->adsp_dma.start_addr;
	uint32_t adsp_dma_buf_size = dai_mem->adsp_dma.size_bytes;
	uint32_t adsp_dma_hw_off = 0;
	uint32_t adsp_dma_appl_off = dai_mem->adsp_dma.appl_offset_bytes;
	unsigned char *cpu_dma_buf_vaddr =
		    dai_mem->cpu_dma.dma_buf_vaddr;
	uint32_t cpu_dma_buf_size = dai_mem->cpu_dma.dma_buf_size;
	uint32_t cpu_dma_offset = dai_mem->cpu_dma.dma_offset;
	uint32_t period_size_bytes = dai_mem->cpu_dma.dma_period_size_bytes;
	uint32_t avail_bytes;
	uint32_t cpu_dma_queued_bytes;
	uint32_t copy_bytes;

	/* hw read_ptr */
	adsp_dma_hw_off =
		dai_mem->adsp_dma_control->ptr_to_hw_offset_bytes;

	if ((adsp_dma_hw_off + adsp_dma_appl_off + 1) == adsp_dma_buf_size)
		avail_bytes = adsp_dma_buf_size;
	else if (adsp_dma_hw_off >= adsp_dma_appl_off)
		avail_bytes = adsp_dma_hw_off - adsp_dma_appl_off;
	else
		avail_bytes = adsp_dma_buf_size - adsp_dma_appl_off +
			adsp_dma_hw_off;

	cpu_dma_queued_bytes = snd_pcm_playback_hw_avail(substream->runtime);
	cpu_dma_queued_bytes =
		frames_to_bytes(substream->runtime, cpu_dma_queued_bytes);

	if (cpu_dma_queued_bytes >= avail_bytes)
		cpu_dma_queued_bytes = avail_bytes;

	if (cpu_dma_queued_bytes < period_size_bytes)
		return;

	if (dai_mem->is_first_write) {
		cpu_dma_queued_bytes -= period_size_bytes;
		copy_bytes = (cpu_dma_queued_bytes / period_size_bytes)
			* period_size_bytes;
		dai_mem->is_first_write = 0;
	} else
		copy_bytes = period_size_bytes;

	while (copy_bytes > 0) {
		uint32_t from_bytes = 0;

		if (cpu_dma_offset + copy_bytes < cpu_dma_buf_size)
			from_bytes = copy_bytes;
		else
			from_bytes = cpu_dma_buf_size - cpu_dma_offset;

		while (from_bytes > 0) {
			uint32_t to_bytes = 0;

			if (adsp_dma_hw_off >= adsp_dma_appl_off) {
				if (adsp_dma_appl_off + from_bytes
					<= adsp_dma_hw_off)
					to_bytes = from_bytes;
				else
					to_bytes =
						adsp_dma_hw_off
						- adsp_dma_appl_off;
			} else {
				if (adsp_dma_appl_off + from_bytes
					< adsp_dma_buf_size)
					to_bytes = from_bytes;
				else
					to_bytes =
						adsp_dma_buf_size
						- adsp_dma_appl_off;
			}

			memcpy(adsp_dma_buf_vaddr + adsp_dma_appl_off,
				cpu_dma_buf_vaddr + cpu_dma_offset,
				to_bytes);

			from_bytes -= to_bytes;
			copy_bytes -= to_bytes;

			cpu_dma_offset = (cpu_dma_offset + to_bytes) %
				cpu_dma_buf_size;
			adsp_dma_appl_off = (adsp_dma_appl_off + to_bytes) %
				adsp_dma_buf_size;
		}
	}

	dai_mem->adsp_dma_control->ptr_to_appl_offset_bytes =
		adsp_dma_appl_off;
	dai_mem->adsp_dma.appl_offset_bytes = adsp_dma_appl_off;
	dai_mem->adsp_dma.hw_offset_bytes = adsp_dma_hw_off;
	dai_mem->cpu_dma.dma_offset = cpu_dma_offset;
}
#endif

static snd_pcm_uframes_t mt8512_adsp_pcm_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	struct mt8512_adsp_dai_memory *dai_mem = &priv->dai_mem[id];

	if (mt8512_adsp_need_ul_dma_copy(id))
		mt8512_adsp_pcm_data_copy(substream, dai_mem);
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	else if (mt8512_adsp_need_dl_dma_copy(id))
		mt8512_adsp_pcm_data_write(substream, dai_mem);
#endif
	return bytes_to_frames(runtime, priv->dai_mem[id].cpu_dma.dma_offset);
}

const struct snd_pcm_ops mt8512_adsp_pcm_ops = {
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = mt8512_adsp_pcm_pointer,
};

static void audio_ipi_ul_irq_handler(int id)
{
	struct mt8512_adsp_dai_memory *dai_mem = &g_priv->dai_mem[id];
	struct snd_pcm_substream *substream = dai_mem->substream;

	snd_pcm_period_elapsed(substream);
}

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
static void audio_ipi_dl_irq_handler(int id)
{
	struct mt8512_adsp_dai_memory *dai_mem = &g_priv->dai_mem[id];
	struct snd_pcm_substream *substream = dai_mem->substream;

	snd_pcm_period_elapsed(substream);
}
#endif

static void mt8512_adsp_pcm_va_notify(struct dsp_ipc_va_notify *info)
{
	char *hot_event[2];

	hot_event[0] = "ACTION=HOTWORD";
	hot_event[1] = NULL;

	/* TODO improve the printf method & how to store information to priv */
	if (info->type == VA_NOTIFY_VAD_PASS)
		pr_notice("get vad ok notify\n");
	else if (info->type == VA_NOTIFY_WAKEWORD_PASS)
		pr_notice("get wakeword(%s) ok notify\n", info->wakeword);
	kobject_uevent_env(&g_priv->dev->kobj, KOBJ_CHANGE, hot_event);
}

static void mt8512_adsp_pcm_ipi_recv_msg(struct ipi_msg_t *p_ipi_msg)
{
	if (!p_ipi_msg)
		return;

	if (p_ipi_msg->task_scene == TASK_SCENE_AUDIO_CONTROLLER) {
		switch (p_ipi_msg->msg_id) {
		case MSG_TO_HOST_DSP_AUDIO_READY:
			{
#if defined(CONFIG_MTK_QOS_SUPPORT)
				struct dsp_info_notify init_info;

				memcpy((void *)&init_info,
					(void *)(p_ipi_msg->payload),
					sizeof(struct dsp_info_notify));
				g_priv->max_pll = (int)init_info.max_pll;
#endif
				pr_info("%s, DSP Audio ready!\n", __func__);
				g_priv->dsp_loading = false;
			}
			break;
		default:
			break;
		}
	} else if (p_ipi_msg->task_scene == TASK_SCENE_VA) {
		switch (p_ipi_msg->msg_id) {
		case MSG_TO_HOST_DSP_IRQUL:
			{
				struct dsp_ipc_msg_irq ipc_irq;

				memcpy((void *)&ipc_irq,
					(void *)(p_ipi_msg->payload),
					sizeof(struct dsp_ipc_msg_irq));
				/* TODO, need add share buffer info */
				audio_ipi_ul_irq_handler(ipc_irq.dai_id);
			}
			break;
		case MSG_TO_HOST_VA_NOTIFY:
			{
				struct dsp_ipc_va_notify info;

				memcpy((void *)&info,
					(void *)(p_ipi_msg->payload),
					sizeof(struct dsp_ipc_va_notify));
				mt8512_adsp_pcm_va_notify(&info);
			}
			break;
		case MSG_TO_HOST_VA_NOTIFY_TEST:
			pr_info("VA Notify test message received!\n");
			break;
		default:
			break;
		}
#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	} else if (p_ipi_msg->task_scene == TASK_SCENE_PCM_PLAYBACK) {
		switch (p_ipi_msg->msg_id) {
		case MSG_TO_HOST_DSP_IRQDL:
			{
				struct dsp_ipc_msg_irq ipc_irq;

				memcpy((void *)&ipc_irq,
					(void *)(p_ipi_msg->payload),
					sizeof(struct dsp_ipc_msg_irq));

				/* TODO, need add share buffer info */
				audio_ipi_dl_irq_handler(ipc_irq.dai_id);
			}
			break;
		default:
			break;
		}
#endif
	}
}
/* TODO remove unused attribute */
static void load_hifi4dsp_callback(void *arg)
{
	struct mt8512_adsp_pcm_priv *priv = arg;

	if (!hifixdsp_run_status())
		dev_info(priv->dev,
			 "%s hifi4dsp_run_status not done\n",
			 __func__);
	while (priv->dsp_loading)
		usleep_range(10000, 11000);

	mt8512_adsp_low_power_init(priv);

	mt8512_adsp_send_ipi_cmd(NULL,
				 TASK_SCENE_AUDIO_CONTROLLER,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_CREATE_VA_T,
				 0, 0, NULL);
	audio_task_register_callback(TASK_SCENE_VA,
				     mt8512_adsp_pcm_ipi_recv_msg,
				     NULL);

#ifdef CONFIG_SND_SOC_MT8512_ADSP_PCM_PLAYBACK
	mt8512_adsp_send_ipi_cmd(NULL,
				 TASK_SCENE_AUDIO_CONTROLLER,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 MSG_TO_DSP_CREATE_PCM_PLAYBACK_T,
				 0, 0, NULL);
	audio_task_register_callback(TASK_SCENE_PCM_PLAYBACK,
				     mt8512_adsp_pcm_ipi_recv_msg,
				     NULL);
#endif
	priv->dsp_ready = true;
}

static int mt8512_adsp_pcm_probe(struct snd_soc_platform *platform)
{
	int ret = 0;
	struct mt8512_adsp_pcm_priv *priv =
		snd_soc_platform_get_drvdata(platform);
	struct mt8512_adsp_data *afe_adsp;
	struct mt8512_afe_private *afe_priv;

	/* TODO, how to get afe private */
	priv->afe = mt8512_afe_pcm_get_info();
	afe_priv = (struct mt8512_afe_private *)(priv->afe->platform_priv);
	afe_adsp = &(afe_priv->adsp_data);
	afe_adsp->adsp_on = true;
	afe_adsp->hostless_active = mt8512_adsp_hostless_active;

	ret = audio_task_register_callback(TASK_SCENE_AUDIO_CONTROLLER,
					   mt8512_adsp_pcm_ipi_recv_msg,
					   NULL);
	if (ret) {
		dev_info(platform->dev,
			"%s register callback for audio controller fail %d\n",
			__func__, ret);
		return ret;
	}

	if (!priv->dsp_boot_run)
		return 0;

	priv->dsp_loading = true;
#if defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_update_request(&priv->pm_adsp, 0); /* VCORE_OPP_0 */
#endif
	ret = async_load_hifixdsp_bin_and_run(load_hifi4dsp_callback, priv);
	if (ret) {
		dev_info(platform->dev,
			"%s async_load_hifi4dsp_bin_and_run fail %d\n",
			__func__, ret);
		priv->dsp_loading = false;
		return ret;
	}

	return 0;
}

static int mt8512_adsp_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size = mt8512_adsp_pcm_pcm_hardware.buffer_bytes_max;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						     card->dev, size, size);
}

static void mt8512_adsp_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

const struct snd_soc_platform_driver mt8512_adsp_pcm_platform = {
	.probe = mt8512_adsp_pcm_probe,
	.ops = &mt8512_adsp_pcm_ops,
	.pcm_new = mt8512_adsp_pcm_new,
	.pcm_free = mt8512_adsp_pcm_free,
};

static int mt8512_adsp_pcm_parse_of(struct mt8512_adsp_pcm_priv *priv,
				     struct device_node *np)
{
	int ret = 0;
	int i;
	char prop[128];
	unsigned int val;
	struct {
		char *name;
		unsigned int val;
	} of_be_table[] = {
		{ "tdmin",	MT8512_ADSP_BE_TDM_IN },
		{ "ul9",	MT8512_ADSP_BE_UL9 },
		{ "ul2",	MT8512_ADSP_BE_UL2 },
	};

	if (!priv || !np)
		return -EINVAL;

	ret = of_property_read_u32_array(np, "mediatek,dsp-boot-run", &val, 1);
	if (ret)
		priv->dsp_boot_run = false;
	else
		priv->dsp_boot_run = (val == 1) ? true : false;

	/* default adsp memif use low power memory for dma */
	for (i = 0; i < ARRAY_SIZE(of_be_table); i++) {
		snprintf(prop, sizeof(prop), "mediatek,%s-mem-type",
			 of_be_table[i].name);
		ret = of_property_read_u32_array(np, prop, &val, 1);
		if (ret)
			priv->be_data[i].mem_type = AFE_MEM_TYPE_LP;
		else
			priv->be_data[i].mem_type = (int)val;
	}

	return ret;
}

static int mt8512_adsp_pcm_dev_probe(struct platform_device *pdev)
{
	int ret;
	struct mt8512_adsp_pcm_priv *priv;
	struct device *dev = &pdev->dev;

	priv = devm_kzalloc(dev,
			    sizeof(struct mt8512_adsp_pcm_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	platform_set_drvdata(pdev, priv);
	g_priv = priv;

	mt8512_adsp_pcm_parse_of(priv, dev->of_node);

	ret = snd_soc_register_platform(dev, &mt8512_adsp_pcm_platform);
	if (ret < 0) {
		dev_info(dev, "Failed to register platform\n");
		return ret;
	}

	ret = snd_soc_register_component(dev,
					 &mt8512_adsp_pcm_dai_comp_drv,
					 mt8512_adsp_pcm_dais,
					 ARRAY_SIZE(mt8512_adsp_pcm_dais));
	if (ret < 0) {
		dev_info(dev, "Failed to register component\n");
		goto err_platform;
	}

#if defined(CONFIG_MTK_QOS_SUPPORT)
	pm_qos_add_request(&priv->pm_adsp,
		PM_QOS_VCORE_OPP, PM_QOS_VCORE_OPP_DEFAULT_VALUE);
#endif

	dev_info(dev, "%s initialized.\n", __func__);
	return 0;

err_platform:
	snd_soc_unregister_platform(dev);
	return ret;
}

static int mt8512_adsp_pcm_dev_remove(struct platform_device *pdev)
{
#if defined(CONFIG_MTK_QOS_SUPPORT)
	struct mt8512_adsp_pcm_priv *priv;

	priv = platform_get_drvdata(pdev);
	pm_qos_remove_request(&g_priv->pm_adsp);
#endif
	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

static const struct of_device_id mt8512_adsp_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8512-adsp-pcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt8512_adsp_pcm_dt_match);

static struct platform_driver mt8512_adsp_pcm_driver = {
	.driver = {
		.name = "mt8512-adsp-pcm",
		.of_match_table = mt8512_adsp_pcm_dt_match,
	},
	.probe = mt8512_adsp_pcm_dev_probe,
	.remove = mt8512_adsp_pcm_dev_remove,
};
module_platform_driver(mt8512_adsp_pcm_driver);

MODULE_DESCRIPTION("MT8512 Audio ADSP driver");
MODULE_AUTHOR("Mengge Wang <mengge.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
