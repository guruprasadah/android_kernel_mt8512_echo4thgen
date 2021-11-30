# Copyright (C) 2020  Amazon.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

ifeq ($(CONFIG_MTK_HIFIXDSP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifi_dsp/mt8512/

obj-$(CONFIG_AMAZON_DSP_FW_LOAD)	+= mt8512_dsp_fw_load.o
obj-$(CONFIG_I2C_ADSP_IPI)		+= i2c-adsp-ipi.o
endif
