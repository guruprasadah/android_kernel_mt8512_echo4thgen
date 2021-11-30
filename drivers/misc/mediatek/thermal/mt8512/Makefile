#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/mt8512/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/mt8512
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/video/include/

obj-  := dummy.o

ifeq ($(CONFIG_THERMAL), y)
else
    obj-y += mtk_thermal_dummy.o
endif

obj-y += mtk_thermal_platform.o
obj-y += mtk_thermal_timer.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
#    obj-y += mtk_ts_wmt.o
endif

# Only included in non-EVB projects
ifeq ($(CONFIG_CHARGER_MTK), y)
    obj-y += mtk_cooler_bcct.o
    obj-y += mtk_ts_battery.o
endif

obj-y += mtk_ts_bts.o
obj-y += mtk_ts_cpu.o
ifeq ($(CONFIG_MTK_PMIC), y)
 obj-y += mtk_ts_pmic.o
endif
obj-y += mtk_cooler_hpt.o
