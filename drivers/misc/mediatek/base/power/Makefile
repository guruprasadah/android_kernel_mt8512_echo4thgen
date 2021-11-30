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

obj-$(CONFIG_MACH_MT8518) += hps_v1/
obj-$(CONFIG_MACH_MT8518) += mt8518/
obj-$(CONFIG_MACH_MT8512) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
ifeq ($(CONFIG_MTK_UNIFY_POWER), y)
obj-$(CONFIG_MACH_MT8518) += upower_v2/
obj-$(CONFIG_MACH_MT8512) += upower_v2/
endif

