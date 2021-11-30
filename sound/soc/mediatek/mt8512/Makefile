ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ifeq ($(CONFIG_MTK_HIFIXDSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
endif

snd-soc-mt8512-pcm-objs := \
    mt8512-afe-pcm.o mt8512-afe-utils.o mt8512-afe-controls.o mt8512-afe-debug.o

ifeq ($(CONFIG_MTK_HIFIXDSP_SUPPORT),y)
snd-soc-mt8512-adsp-pcm-objs := \
    mt8512-adsp-pcm.o mt8512-adsp-utils.o
endif

# MTK Platform Support
obj-$(CONFIG_SND_SOC_MT8512) += snd-soc-mt8512-pcm.o
obj-$(CONFIG_MTK_HIFIXDSP_SUPPORT) += snd-soc-mt8512-adsp-pcm.o
# Machine support
obj-$(CONFIG_SND_SOC_MT8512_EVB) += mt8512-evb.o
obj-$(CONFIG_SND_SOC_MT8512_FPGA) += mt8512-evb.o
obj-$(CONFIG_SND_SOC_MT8512B_LASER) += laser.o
obj-$(CONFIG_SND_SOC_MT8512B_BROWNIE) += brownie.o
obj-$(CONFIG_SND_SOC_MT8512B_P1) += mt8512-evb.o
