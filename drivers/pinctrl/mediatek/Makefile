subdir-ccflags-y += -Werror
# Core
obj-y				+= pinctrl-mtk-common.o

# SoC Drivers
#obj-$(CONFIG_PINCTRL_MT2701)	+= pinctrl-mt2701.o
#obj-$(CONFIG_PINCTRL_MT2712)	+= pinctrl-mt2712.o
obj-$(CONFIG_PINCTRL_MT7623)	+= pinctrl-mt7623.o
obj-$(CONFIG_PINCTRL_MT8135)	+= pinctrl-mt8135.o
obj-$(CONFIG_PINCTRL_MT8127)	+= pinctrl-mt8127.o
obj-$(CONFIG_PINCTRL_MT8173)	+= pinctrl-mt8173.o
obj-$(CONFIG_PINCTRL_MT6397)	+= pinctrl-mt6397.o
obj-$(CONFIG_PINCTRL_MT6758)	+= pinctrl-mt6758.o
obj-$(CONFIG_PINCTRL_MT6765)	+= pinctrl-mt6765.o
obj-$(CONFIG_PINCTRL_MT8512)	+= pinctrl-mt8512.o
obj-$(CONFIG_PINCTRL_MT8518)	+= pinctrl-mt8518.o
ifneq ($(filter y,$(CONFIG_MTK_HIFI4DSP_SUPPORT)),)
### include
ccflags-y += \
-I$(srctree)/drivers/misc/mediatek/hifi4dsp/hifi4dsp_ipi/mt8570 \
-I$(srctree)/drivers/misc/mediatek/hifi4dsp/hifi4dsp_ipi/common
obj-y				+= pinctrl-mt8518-ipi.o
endif