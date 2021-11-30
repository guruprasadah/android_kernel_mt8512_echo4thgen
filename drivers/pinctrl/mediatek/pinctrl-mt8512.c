// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <dt-bindings/pinctrl/mt65xx.h>

#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt8512.h"

#define MAX_GPIO_MODE_PER_REG 10
#define GPIO_MODE_BITS        3

static const struct mtk_pin_info mt8512_pin_info_mode[] = {
	MTK_PIN_INFO(0, 0x1E0, 0, 3, 0),
	MTK_PIN_INFO(1, 0x1E0, 3, 3, 0),
	MTK_PIN_INFO(2, 0x1E0, 6, 3, 0),
	MTK_PIN_INFO(3, 0x1E0, 9, 3, 0),
	MTK_PIN_INFO(4, 0x1E0, 12, 3, 0),
	MTK_PIN_INFO(5, 0x1E0, 15, 3, 0),
	MTK_PIN_INFO(6, 0x1E0, 18, 3, 0),
	MTK_PIN_INFO(7, 0x1E0, 21, 3, 0),
	MTK_PIN_INFO(8, 0x1E0, 24, 3, 0),
	MTK_PIN_INFO(9, 0x1E0, 27, 3, 0),
	MTK_PIN_INFO(10, 0x1F0, 0, 3, 0),
	MTK_PIN_INFO(11, 0x1F0, 3, 3, 0),
	MTK_PIN_INFO(12, 0x1F0, 6, 3, 0),
	MTK_PIN_INFO(13, 0x1F0, 9, 3, 0),
	MTK_PIN_INFO(14, 0x1F0, 12, 3, 0),
	MTK_PIN_INFO(15, 0x1F0, 15, 3, 0),
	MTK_PIN_INFO(16, 0x1F0, 18, 3, 0),
	MTK_PIN_INFO(17, 0x1F0, 21, 3, 0),
	MTK_PIN_INFO(18, 0x1F0, 24, 3, 0),
	MTK_PIN_INFO(19, 0x1F0, 27, 3, 0),
	MTK_PIN_INFO(20, 0x200, 0, 3, 0),
	MTK_PIN_INFO(21, 0x200, 3, 3, 0),
	MTK_PIN_INFO(22, 0x200, 6, 3, 0),
	MTK_PIN_INFO(23, 0x200, 9, 3, 0),
	MTK_PIN_INFO(24, 0x200, 12, 3, 0),
	MTK_PIN_INFO(25, 0x200, 15, 3, 0),
	MTK_PIN_INFO(26, 0x200, 18, 3, 0),
	MTK_PIN_INFO(27, 0x200, 21, 3, 0),
	MTK_PIN_INFO(28, 0x200, 24, 3, 0),
	MTK_PIN_INFO(29, 0x200, 27, 3, 0),
	MTK_PIN_INFO(30, 0x210, 0, 3, 0),
	MTK_PIN_INFO(31, 0x210, 3, 3, 0),
	MTK_PIN_INFO(32, 0x210, 6, 3, 0),
	MTK_PIN_INFO(33, 0x210, 9, 3, 0),
	MTK_PIN_INFO(34, 0x210, 12, 3, 0),
	MTK_PIN_INFO(35, 0x210, 15, 3, 0),
	MTK_PIN_INFO(36, 0x210, 18, 3, 0),
	MTK_PIN_INFO(37, 0x210, 21, 3, 0),
	MTK_PIN_INFO(38, 0x210, 24, 3, 0),
	MTK_PIN_INFO(39, 0x210, 27, 3, 0),
	MTK_PIN_INFO(40, 0x220, 0, 3, 0),
	MTK_PIN_INFO(41, 0x220, 3, 3, 0),
	MTK_PIN_INFO(42, 0x220, 6, 3, 0),
	MTK_PIN_INFO(43, 0x220, 9, 3, 0),
	MTK_PIN_INFO(44, 0x220, 12, 3, 0),
	MTK_PIN_INFO(45, 0x220, 15, 3, 0),
	MTK_PIN_INFO(46, 0x220, 18, 3, 0),
	MTK_PIN_INFO(47, 0x220, 21, 3, 0),
	MTK_PIN_INFO(48, 0x220, 24, 3, 0),
	MTK_PIN_INFO(49, 0x220, 27, 3, 0),
	MTK_PIN_INFO(50, 0x230, 0, 3, 0),
	MTK_PIN_INFO(51, 0x230, 3, 3, 0),
	MTK_PIN_INFO(52, 0x230, 6, 3, 0),
	MTK_PIN_INFO(53, 0x230, 9, 3, 0),
	MTK_PIN_INFO(54, 0x230, 12, 3, 0),
	MTK_PIN_INFO(55, 0x230, 15, 3, 0),
	MTK_PIN_INFO(56, 0x230, 18, 3, 0),
	MTK_PIN_INFO(57, 0x230, 21, 3, 0),
	MTK_PIN_INFO(58, 0x230, 24, 3, 0),
	MTK_PIN_INFO(59, 0x230, 27, 3, 0),
	MTK_PIN_INFO(60, 0x240, 0, 3, 0),
	MTK_PIN_INFO(61, 0x240, 3, 3, 0),
	MTK_PIN_INFO(62, 0x240, 6, 3, 0),
	MTK_PIN_INFO(63, 0x240, 9, 3, 0),
	MTK_PIN_INFO(64, 0x240, 12, 3, 0),
	MTK_PIN_INFO(65, 0x240, 15, 3, 0),
	MTK_PIN_INFO(66, 0x240, 18, 3, 0),
	MTK_PIN_INFO(67, 0x240, 21, 3, 0),
	MTK_PIN_INFO(68, 0x240, 24, 3, 0),
	MTK_PIN_INFO(69, 0x240, 27, 3, 0),
	MTK_PIN_INFO(70, 0x250, 0, 3, 0),
	MTK_PIN_INFO(71, 0x250, 3, 3, 0),
	MTK_PIN_INFO(72, 0x250, 6, 3, 0),
	MTK_PIN_INFO(73, 0x250, 9, 3, 0),
	MTK_PIN_INFO(74, 0x250, 12, 3, 0),
	MTK_PIN_INFO(75, 0x250, 15, 3, 0),
	MTK_PIN_INFO(76, 0x250, 18, 3, 0),
	MTK_PIN_INFO(77, 0x250, 21, 3, 0),
	MTK_PIN_INFO(78, 0x250, 24, 3, 0),
	MTK_PIN_INFO(79, 0x250, 27, 3, 0),
	MTK_PIN_INFO(80, 0x260, 0, 3, 0),
	MTK_PIN_INFO(81, 0x260, 3, 3, 0),
	MTK_PIN_INFO(82, 0x260, 6, 3, 0),
	MTK_PIN_INFO(83, 0x260, 9, 3, 0),
	MTK_PIN_INFO(84, 0x260, 12, 3, 0),
	MTK_PIN_INFO(85, 0x260, 15, 3, 0),
	MTK_PIN_INFO(86, 0x260, 18, 3, 0),
	MTK_PIN_INFO(87, 0x260, 21, 3, 0),
	MTK_PIN_INFO(88, 0x260, 24, 3, 0),
	MTK_PIN_INFO(89, 0x260, 27, 3, 0),
	MTK_PIN_INFO(90, 0x270, 0, 3, 0),
	MTK_PIN_INFO(91, 0x270, 3, 3, 0),
	MTK_PIN_INFO(92, 0x270, 6, 3, 0),
	MTK_PIN_INFO(93, 0x270, 9, 3, 0),
	MTK_PIN_INFO(94, 0x270, 12, 3, 0),
	MTK_PIN_INFO(95, 0x270, 15, 3, 0),
	MTK_PIN_INFO(96, 0x270, 18, 3, 0),
	MTK_PIN_INFO(97, 0x270, 21, 3, 0),
	MTK_PIN_INFO(98, 0x270, 24, 3, 0),
	MTK_PIN_INFO(99, 0x270, 27, 3, 0),
	MTK_PIN_INFO(100, 0x280, 0, 3, 0),
	MTK_PIN_INFO(101, 0x280, 3, 3, 0),
	MTK_PIN_INFO(102, 0x280, 6, 3, 0),
	MTK_PIN_INFO(103, 0x280, 9, 3, 0),
	MTK_PIN_INFO(104, 0x280, 12, 3, 0),
	MTK_PIN_INFO(105, 0x280, 15, 3, 0),
	MTK_PIN_INFO(106, 0x280, 18, 3, 0),
	MTK_PIN_INFO(107, 0x280, 21, 3, 0),
	MTK_PIN_INFO(108, 0x280, 24, 3, 0),
	MTK_PIN_INFO(109, 0x280, 27, 3, 0),
	MTK_PIN_INFO(110, 0x290, 0, 3, 0),
	MTK_PIN_INFO(111, 0x290, 3, 3, 0),
	MTK_PIN_INFO(112, 0x290, 6, 3, 0),
	MTK_PIN_INFO(113, 0x290, 9, 3, 0),
	MTK_PIN_INFO(114, 0x290, 12, 3, 0),
	MTK_PIN_INFO(115, 0x290, 15, 3, 0),
};

static const struct mtk_drv_group_desc mt8512_drv_grp[] =  {
	/* 0E4E8SR 4/8/12/16 */
	MTK_DRV_GRP(4, 16, 1, 2, 4),
	/* 0E2E4SR  2/4/6/8 */
	MTK_DRV_GRP(2, 8, 1, 2, 2),
	/* E8E4E2  2/4/6/8/10/12/14/16 */
	MTK_DRV_GRP(2, 16, 0, 2, 2)
};

static const struct mtk_pin_drv_grp mt8512_pin_drv[] = {
	MTK_PIN_DRV_GRP(0, 0x710, 0, 2),
	MTK_PIN_DRV_GRP(1, 0x710, 0, 2),
	MTK_PIN_DRV_GRP(2, 0x710, 0, 2),
	MTK_PIN_DRV_GRP(3, 0x710, 4, 2),
	MTK_PIN_DRV_GRP(4, 0x710, 4, 2),
	MTK_PIN_DRV_GRP(5, 0x710, 4, 2),
	MTK_PIN_DRV_GRP(6, 0x710, 8, 2),
	MTK_PIN_DRV_GRP(7, 0x710, 8, 2),
	MTK_PIN_DRV_GRP(8, 0x710, 12, 2),
	MTK_PIN_DRV_GRP(9, 0x710, 12, 2),
	MTK_PIN_DRV_GRP(10, 0x710, 12, 2),
	MTK_PIN_DRV_GRP(11, 0x710, 12, 2),
	MTK_PIN_DRV_GRP(12, 0x710, 16, 2),
	MTK_PIN_DRV_GRP(13, 0x710, 16, 2),
	MTK_PIN_DRV_GRP(14, 0x710, 16, 2),
	MTK_PIN_DRV_GRP(15, 0x710, 16, 2),
	MTK_PIN_DRV_GRP(16, 0x710, 20, 2),
	MTK_PIN_DRV_GRP(17, 0x710, 20, 2),
	MTK_PIN_DRV_GRP(18, 0x710, 20, 2),
	MTK_PIN_DRV_GRP(19, 0x710, 20, 2),
	MTK_PIN_DRV_GRP(20, 0x710, 24, 2),
	MTK_PIN_DRV_GRP(21, 0x710, 28, 2),
	MTK_PIN_DRV_GRP(22, 0x710, 28, 2),
	MTK_PIN_DRV_GRP(23, 0x710, 28, 2),
	MTK_PIN_DRV_GRP(24, 0x710, 28, 2),
	MTK_PIN_DRV_GRP(25, 0x710, 28, 2),
	MTK_PIN_DRV_GRP(26, 0x720, 0, 2),
	MTK_PIN_DRV_GRP(27, 0x720, 0, 2),
	MTK_PIN_DRV_GRP(28, 0x720, 4, 2),
	MTK_PIN_DRV_GRP(29, 0x720, 4, 2),
	MTK_PIN_DRV_GRP(30, 0x720, 4, 2),
	MTK_PIN_DRV_GRP(31, 0x720, 4, 2),
	MTK_PIN_DRV_GRP(32, 0x720, 8, 2),
	MTK_PIN_DRV_GRP(33, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(34, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(35, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(36, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(37, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(38, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(39, 0x720, 12, 2),
	MTK_PIN_DRV_GRP(40, 0x720, 16, 2),
	MTK_PIN_DRV_GRP(41, 0x720, 20, 2),
	MTK_PIN_DRV_GRP(42, 0x720, 20, 2),
	MTK_PIN_DRV_GRP(43, 0x720, 20, 2),
	MTK_PIN_DRV_GRP(44, 0x720, 24, 2),
	MTK_PIN_DRV_GRP(45, 0x720, 24, 2),
	MTK_PIN_DRV_GRP(46, 0x720, 24, 2),
	MTK_PIN_DRV_GRP(47, 0x720, 24, 2),
	MTK_PIN_DRV_GRP(48, 0x720, 28, 2),
	MTK_PIN_DRV_GRP(49, 0x720, 28, 2),
	MTK_PIN_DRV_GRP(50, 0x720, 28, 2),
	MTK_PIN_DRV_GRP(51, 0x720, 28, 2),
	MTK_PIN_DRV_GRP(52, 0x730, 0, 2),
	MTK_PIN_DRV_GRP(53, 0x730, 0, 2),
	MTK_PIN_DRV_GRP(54, 0x730, 4, 2),
	MTK_PIN_DRV_GRP(55, 0x730, 4, 2),
	MTK_PIN_DRV_GRP(56, 0x730, 4, 2),
	MTK_PIN_DRV_GRP(57, 0x730, 4, 2),
	MTK_PIN_DRV_GRP(58, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(59, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(60, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(61, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(62, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(63, 0x730, 8, 2),
	MTK_PIN_DRV_GRP(64, 0x730, 12, 2),
	MTK_PIN_DRV_GRP(65, 0x730, 12, 2),
	MTK_PIN_DRV_GRP(66, 0x730, 16, 2),
	MTK_PIN_DRV_GRP(67, 0x730, 16, 2),
	MTK_PIN_DRV_GRP(68, 0x730, 20, 2),
	MTK_PIN_DRV_GRP(69, 0x730, 20, 2),
	MTK_PIN_DRV_GRP(70, 0x730, 24, 2),
	MTK_PIN_DRV_GRP(71, 0x730, 28, 2),
	MTK_PIN_DRV_GRP(72, 0x740, 0, 2),
	MTK_PIN_DRV_GRP(73, 0x740, 0, 2),
	MTK_PIN_DRV_GRP(74, 0x740, 0, 2),
	MTK_PIN_DRV_GRP(75, 0x740, 0, 2),
	MTK_PIN_DRV_GRP(76, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(77, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(78, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(79, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(80, 0x750, 0, 2),
	MTK_PIN_DRV_GRP(81, 0x750, 4, 2),
	MTK_PIN_DRV_GRP(82, 0x750, 8, 2),
	MTK_PIN_DRV_GRP(83, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(84, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(85, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(86, 0x740, 16, 2),
	MTK_PIN_DRV_GRP(87, 0x750, 24, 2),
	MTK_PIN_DRV_GRP(88, 0x750, 28, 2),
	MTK_PIN_DRV_GRP(89, 0x750, 28, 2),
	MTK_PIN_DRV_GRP(90, 0x750, 28, 2),
	MTK_PIN_DRV_GRP(91, 0x750, 28, 2),
	MTK_PIN_DRV_GRP(92, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(93, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(94, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(95, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(96, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(97, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(98, 0x760, 0, 2),
	MTK_PIN_DRV_GRP(99, 0x760, 4, 2),
	MTK_PIN_DRV_GRP(100, 0x760, 4, 2),
	MTK_PIN_DRV_GRP(101, 0x760, 4, 2),
	MTK_PIN_DRV_GRP(102, 0x760, 8, 2),
	MTK_PIN_DRV_GRP(103, 0x760, 8, 2),
	MTK_PIN_DRV_GRP(104, 0x760, 8, 2),
	MTK_PIN_DRV_GRP(105, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(106, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(107, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(108, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(109, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(110, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(111, 0x760, 12, 2),
	MTK_PIN_DRV_GRP(112, 0x760, 16, 2),
	MTK_PIN_DRV_GRP(113, 0x760, 16, 2),
	MTK_PIN_DRV_GRP(114, 0x760, 16, 2),
	MTK_PIN_DRV_GRP(115, 0x760, 16, 2),
};

static const struct mtk_pin_spec_pupd_set_samereg mt8512_spec_pupd[] = {
	MTK_PIN_PUPD_SPEC_SR(0, 0x0F0, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(1, 0x0F0, 17, 16, 15),
	MTK_PIN_PUPD_SPEC_SR(2, 0x0F0, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(3, 0x0F0, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(4, 0x0F0, 26, 25, 24),
	MTK_PIN_PUPD_SPEC_SR(5, 0x0F0, 29, 28, 27),
	MTK_PIN_PUPD_SPEC_SR(6, 0x300, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(7, 0x300, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(8, 0x300, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(9, 0x300, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(10, 0x300, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(11, 0x300, 17, 16, 15),
	MTK_PIN_PUPD_SPEC_SR(32, 0x300, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(40, 0x070, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(41, 0x070, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(42, 0x070, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(43, 0x070, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(44, 0x300, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(45, 0x300, 26, 25, 24),
	MTK_PIN_PUPD_SPEC_SR(46, 0x300, 29, 28, 27),
	MTK_PIN_PUPD_SPEC_SR(47, 0x310, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(70, 0x080, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(71, 0x080, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(72, 0x080, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(73, 0x080, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(74, 0x080, 17, 16, 15),
	MTK_PIN_PUPD_SPEC_SR(75, 0x080, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(76, 0x080, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(77, 0x080, 26, 25, 24),
	MTK_PIN_PUPD_SPEC_SR(78, 0x080, 29, 28, 27),
	MTK_PIN_PUPD_SPEC_SR(79, 0x090, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(80, 0x090, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(81, 0x090, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(82, 0x090, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(83, 0x090, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(84, 0x090, 17, 16, 15),
	MTK_PIN_PUPD_SPEC_SR(85, 0x090, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(86, 0x090, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(87, 0x310, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(92, 0x310, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(93, 0x310, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(94, 0x310, 26, 25, 24),
	MTK_PIN_PUPD_SPEC_SR(95, 0x310, 29, 28, 27),
	MTK_PIN_PUPD_SPEC_SR(96, 0x320, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(97, 0x320, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(98, 0x320, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(99, 0x320, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(100, 0x320, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(101, 0x320, 17, 16, 15),
	MTK_PIN_PUPD_SPEC_SR(102, 0x320, 20, 19, 18),
	MTK_PIN_PUPD_SPEC_SR(103, 0x320, 23, 22, 21),
	MTK_PIN_PUPD_SPEC_SR(104, 0x320, 26, 25, 24),
	MTK_PIN_PUPD_SPEC_SR(105, 0x320, 29, 28, 27),
	MTK_PIN_PUPD_SPEC_SR(106, 0x330, 2, 1, 0),
	MTK_PIN_PUPD_SPEC_SR(107, 0x330, 5, 4, 3),
	MTK_PIN_PUPD_SPEC_SR(108, 0x330, 8, 7, 6),
	MTK_PIN_PUPD_SPEC_SR(109, 0x330, 11, 10, 9),
	MTK_PIN_PUPD_SPEC_SR(110, 0x330, 14, 13, 12),
	MTK_PIN_PUPD_SPEC_SR(111, 0x330, 17, 16, 15),
};

static const struct mtk_pin_ies_smt_set mt8512_smt_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 2, 0x470, 0),
	MTK_PIN_IES_SMT_SPEC(3, 5, 0x470, 1),
	MTK_PIN_IES_SMT_SPEC(6, 7, 0x470, 2),
	MTK_PIN_IES_SMT_SPEC(8, 11, 0x470, 3),
	MTK_PIN_IES_SMT_SPEC(12, 15, 0x470, 4),
	MTK_PIN_IES_SMT_SPEC(16, 19, 0x470, 5),
	MTK_PIN_IES_SMT_SPEC(20, 20, 0x470, 6),
	MTK_PIN_IES_SMT_SPEC(21, 25, 0x470, 7),
	MTK_PIN_IES_SMT_SPEC(26, 27, 0x470, 8),
	MTK_PIN_IES_SMT_SPEC(28, 31, 0x470, 9),
	MTK_PIN_IES_SMT_SPEC(32, 32, 0x470, 10),
	MTK_PIN_IES_SMT_SPEC(33, 39, 0x470, 11),
	MTK_PIN_IES_SMT_SPEC(40, 40, 0x470, 12),
	MTK_PIN_IES_SMT_SPEC(41, 43, 0x470, 13),
	MTK_PIN_IES_SMT_SPEC(44, 47, 0x470, 14),
	MTK_PIN_IES_SMT_SPEC(48, 51, 0x470, 15),
	MTK_PIN_IES_SMT_SPEC(52, 53, 0x470, 16),
	MTK_PIN_IES_SMT_SPEC(54, 57, 0x470, 17),
	MTK_PIN_IES_SMT_SPEC(58, 63, 0x470, 18),
	MTK_PIN_IES_SMT_SPEC(64, 65, 0x470, 19),
	MTK_PIN_IES_SMT_SPEC(66, 67, 0x470, 20),
	MTK_PIN_IES_SMT_SPEC(68, 69, 0x470, 21),
	MTK_PIN_IES_SMT_SPEC(70, 70, 0x470, 22),
	MTK_PIN_IES_SMT_SPEC(71, 71, 0x470, 23),
	MTK_PIN_IES_SMT_SPEC(72, 72, 0x470, 24),
	MTK_PIN_IES_SMT_SPEC(73, 73, 0x470, 25),
	MTK_PIN_IES_SMT_SPEC(74, 74, 0x470, 26),
	MTK_PIN_IES_SMT_SPEC(75, 75, 0x470, 27),
	MTK_PIN_IES_SMT_SPEC(76, 76, 0x470, 28),
	MTK_PIN_IES_SMT_SPEC(77, 77, 0x470, 29),
	MTK_PIN_IES_SMT_SPEC(78, 78, 0x470, 30),
	MTK_PIN_IES_SMT_SPEC(79, 79, 0x470, 31),
	MTK_PIN_IES_SMT_SPEC(80, 80, 0x480, 0),
	MTK_PIN_IES_SMT_SPEC(81, 81, 0x480, 1),
	MTK_PIN_IES_SMT_SPEC(82, 82, 0x480, 2),
	MTK_PIN_IES_SMT_SPEC(83, 83, 0x480, 3),
	MTK_PIN_IES_SMT_SPEC(84, 84, 0x480, 4),
	MTK_PIN_IES_SMT_SPEC(85, 86, 0x480, 5),
	MTK_PIN_IES_SMT_SPEC(87, 87, 0x480, 6),
	MTK_PIN_IES_SMT_SPEC(88, 91, 0x480, 7),
	MTK_PIN_IES_SMT_SPEC(92, 98, 0x480, 8),
	MTK_PIN_IES_SMT_SPEC(99, 101, 0x480, 9),
	MTK_PIN_IES_SMT_SPEC(102, 104, 0x480, 10),
	MTK_PIN_IES_SMT_SPEC(105, 111, 0x480, 11),
	MTK_PIN_IES_SMT_SPEC(112, 115, 0x480, 12),
};

static const struct mtk_pin_ies_smt_set mt8512_ies_set[] = {
	MTK_PIN_IES_SMT_SPEC(0, 2, 0x410, 0),
	MTK_PIN_IES_SMT_SPEC(3, 5, 0x410, 1),
	MTK_PIN_IES_SMT_SPEC(6, 7, 0x410, 2),
	MTK_PIN_IES_SMT_SPEC(8, 11, 0x410, 3),
	MTK_PIN_IES_SMT_SPEC(12, 15, 0x410, 4),
	MTK_PIN_IES_SMT_SPEC(16, 19, 0x410, 5),
	MTK_PIN_IES_SMT_SPEC(20, 20, 0x410, 6),
	MTK_PIN_IES_SMT_SPEC(21, 25, 0x410, 7),
	MTK_PIN_IES_SMT_SPEC(26, 27, 0x410, 8),
	MTK_PIN_IES_SMT_SPEC(28, 31, 0x410, 9),
	MTK_PIN_IES_SMT_SPEC(32, 32, 0x410, 10),
	MTK_PIN_IES_SMT_SPEC(33, 39, 0x410, 11),
	MTK_PIN_IES_SMT_SPEC(40, 40, 0x410, 12),
	MTK_PIN_IES_SMT_SPEC(41, 43, 0x410, 13),
	MTK_PIN_IES_SMT_SPEC(44, 47, 0x410, 14),
	MTK_PIN_IES_SMT_SPEC(48, 51, 0x410, 15),
	MTK_PIN_IES_SMT_SPEC(52, 53, 0x410, 16),
	MTK_PIN_IES_SMT_SPEC(54, 57, 0x410, 17),
	MTK_PIN_IES_SMT_SPEC(58, 63, 0x410, 18),
	MTK_PIN_IES_SMT_SPEC(64, 65, 0x410, 19),
	MTK_PIN_IES_SMT_SPEC(66, 67, 0x410, 20),
	MTK_PIN_IES_SMT_SPEC(68, 69, 0x410, 21),
	MTK_PIN_IES_SMT_SPEC(70, 70, 0x410, 22),
	MTK_PIN_IES_SMT_SPEC(71, 71, 0x410, 23),
	MTK_PIN_IES_SMT_SPEC(72, 72, 0x410, 24),
	MTK_PIN_IES_SMT_SPEC(73, 73, 0x410, 25),
	MTK_PIN_IES_SMT_SPEC(74, 74, 0x410, 26),
	MTK_PIN_IES_SMT_SPEC(75, 75, 0x410, 27),
	MTK_PIN_IES_SMT_SPEC(76, 76, 0x410, 28),
	MTK_PIN_IES_SMT_SPEC(77, 77, 0x410, 29),
	MTK_PIN_IES_SMT_SPEC(78, 78, 0x410, 30),
	MTK_PIN_IES_SMT_SPEC(79, 79, 0x410, 31),
	MTK_PIN_IES_SMT_SPEC(80, 80, 0x420, 0),
	MTK_PIN_IES_SMT_SPEC(81, 81, 0x420, 1),
	MTK_PIN_IES_SMT_SPEC(82, 82, 0x420, 2),
	MTK_PIN_IES_SMT_SPEC(83, 83, 0x420, 3),
	MTK_PIN_IES_SMT_SPEC(84, 84, 0x420, 4),
	MTK_PIN_IES_SMT_SPEC(85, 86, 0x420, 5),
	MTK_PIN_IES_SMT_SPEC(87, 87, 0x420, 6),
	MTK_PIN_IES_SMT_SPEC(88, 91, 0x420, 7),
	MTK_PIN_IES_SMT_SPEC(92, 98, 0x420, 8),
	MTK_PIN_IES_SMT_SPEC(99, 101, 0x420, 9),
	MTK_PIN_IES_SMT_SPEC(102, 104, 0x420, 10),
	MTK_PIN_IES_SMT_SPEC(105, 111, 0x420, 11),
	MTK_PIN_IES_SMT_SPEC(112, 115, 0x420, 12),
};

static const struct mtk_pin_info mt8512_pin_info_eh[] = {
	MTK_PIN_INFO(64, 0x050, 0, 3, 0),
	MTK_PIN_INFO(65, 0x050, 3, 3, 0),
	MTK_PIN_INFO(66, 0x050, 6, 3, 0),
	MTK_PIN_INFO(67, 0x050, 9, 3, 0),
	MTK_PIN_INFO(68, 0x050, 12, 3, 0),
	MTK_PIN_INFO(69, 0x050, 15, 3, 0),
};

static const struct mtk_pin_info mt8512_pin_info_rsel[] = {
	MTK_PIN_INFO(64, 0x060, 0, 2, 0),
	MTK_PIN_INFO(65, 0x060, 2, 2, 0),
	MTK_PIN_INFO(66, 0x060, 4, 2, 0),
	MTK_PIN_INFO(67, 0x060, 6, 2, 0),
	MTK_PIN_INFO(68, 0x060, 8, 2, 0),
	MTK_PIN_INFO(69, 0x060, 10, 2, 0),
};
/* i2c mode pin set pull r1r0 resistance in rsel register */
static void mtk_rsel_r1r0_set_samereg(const struct mtk_pin_info *rsel_infos,
		unsigned int info_num, unsigned int pin,
		unsigned int r1r0)
{
	switch (r1r0) {
	case MTK_RSEL_SET_R1R0_00:
		mtk_pinctrl_update_gpio_value(pctl, pin, 0, info_num, rsel_infos);
		break;
	case MTK_RSEL_SET_R1R0_01:
		mtk_pinctrl_update_gpio_value(pctl, pin, 1, info_num, rsel_infos);
		break;
	case MTK_RSEL_SET_R1R0_10:
		mtk_pinctrl_update_gpio_value(pctl, pin, 2, info_num, rsel_infos);
		break;
	case MTK_RSEL_SET_R1R0_11:
		mtk_pinctrl_update_gpio_value(pctl, pin, 3, info_num, rsel_infos);
		break;
	default:
		break;
	}
}

/* i2c mode pin set driving resistance in eh register */
static int mtk_ehe2e1_set_samereg(struct mtk_pinctrl *pctl,
		const struct mtk_pin_info *eh_infos,
		unsigned int info_num, unsigned int pin,
		unsigned int ehe2e1)
{
	switch (ehe2e1) {
	case MTK_EH_SET_I2C_DRIVE_000:
		mtk_pinctrl_update_gpio_value(pctl, pin, 0, info_num, eh_infos);
		break;
	case MTK_EH_SET_I2C_DRIVE_001:
		mtk_pinctrl_update_gpio_value(pctl, pin, 1, info_num, eh_infos);
		break;
	case MTK_EH_SET_I2C_DRIVE_011:
		mtk_pinctrl_update_gpio_value(pctl, pin, 3, info_num, eh_infos);
		break;
	case MTK_EH_SET_I2C_DRIVE_101:
		mtk_pinctrl_update_gpio_value(pctl, pin, 5, info_num, eh_infos);
		break;
	case MTK_EH_SET_I2C_DRIVE_111:
		mtk_pinctrl_update_gpio_value(pctl, pin, 7, info_num, eh_infos);
		break;
	default:
		break;
	}
	return 0;
}

static const struct mtk_pin_drv_grp *mtk_find_drv_grp_by_pin(
		struct mtk_pinctrl *pctl,  unsigned long pin)
{
	int i;

	for (i = 0; i < pctl->devdata->n_pin_drv_grps; i++) {
		const struct mtk_pin_drv_grp *pin_drv =
				pctl->devdata->pin_drv_grp + i;
		if (pin == pin_drv->pin)
			return pin_drv;
	}

	return NULL;
}

static int mtk_pctl_set_gpio_drving(struct mtk_pinctrl *pctl,
		unsigned int pin, unsigned char driving)
{
	const struct mtk_pin_drv_grp *pin_drv;
	unsigned int val;
	unsigned int bits, mask, shift;
	const struct mtk_drv_group_desc *drv_grp;

	if (pin >= pctl->devdata->npins)
		return -EINVAL;
	if (driving >= MTK_EH_SET_I2C_DRIVE_000 && driving <= MTK_EH_SET_I2C_DRIVE_111)
		return mtk_ehe2e1_set_samereg(pctl, mt8512_pin_info_eh,
			ARRAY_SIZE(mt8512_pin_info_eh), pin, driving);
	/* if i2c pins use MTK_DRIVE_2mA/MTK_DRIVE_4mA/.../MTK_DRIVE_16mA,
	 * it defaults to eh = 0, using generic driving.
	 */
	if (pin >= 64 && pin <= 69)
		mtk_ehe2e1_set_samereg(pctl, mt8512_pin_info_eh,
			ARRAY_SIZE(mt8512_pin_info_eh), pin, MTK_EH_SET_I2C_DRIVE_000);

	pin_drv = mtk_find_drv_grp_by_pin(pctl, pin);
	if (!pin_drv || pin_drv->grp > pctl->devdata->n_grp_cls)
		return -EINVAL;

	drv_grp = pctl->devdata->grp_desc + pin_drv->grp;
	if (driving >= drv_grp->min_drv && driving <= drv_grp->max_drv
		&& !(driving % drv_grp->step)) {
		val = driving / drv_grp->step - 1;
		bits = drv_grp->high_bit - drv_grp->low_bit + 1;
		mask = BIT(bits) - 1;
		shift = pin_drv->bit + drv_grp->low_bit;
		mask <<= shift;
		val <<= shift;
		return regmap_update_bits(pctl->regmap1,
				pin_drv->offset, mask, val);
	}

	return -EINVAL;
}

static int mt8512_ies_smt_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, int value, enum pin_config_param arg)
{
	if (arg == PIN_CONFIG_INPUT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8512_ies_set,
			ARRAY_SIZE(mt8512_ies_set), pin, align, value);
	if (arg == PIN_CONFIG_INPUT_SCHMITT_ENABLE)
		return mtk_pconf_spec_set_ies_smt_range(regmap, mt8512_smt_set,
			ARRAY_SIZE(mt8512_smt_set), pin, align, value);
	return -EINVAL;
}

static int mt8512_spec_ies_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8512_ies_set,
		ARRAY_SIZE(mt8512_ies_set), pin);
}

static int mt8512_spec_smt_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_get_ies_smt_range(regmap, mt8512_smt_set,
		ARRAY_SIZE(mt8512_smt_set), pin);
}

static int mt8512_spec_pull_set(struct regmap *regmap, unsigned int pin,
		unsigned char align, bool isup, unsigned int r1r0)
{
	if (r1r0 >= MTK_RSEL_SET_R1R0_00 && r1r0 <= MTK_RSEL_SET_R1R0_11)
		mtk_rsel_r1r0_set_samereg(mt8512_pin_info_rsel,
			ARRAY_SIZE(mt8512_pin_info_rsel), pin, r1r0);

	return mtk_pctrl_spec_pull_set_samereg(regmap, mt8512_spec_pupd,
		ARRAY_SIZE(mt8512_spec_pupd), pin, align, isup, r1r0);
}

static int mt8512_spec_pull_get(struct regmap *regmap, unsigned int pin)
{
	return mtk_spec_pull_get_samereg(regmap, mt8512_spec_pupd,
		ARRAY_SIZE(mt8512_spec_pupd), pin);
}

static void mt8512_spec_pinmux_set(struct regmap *reg, unsigned int pin,
			unsigned int mode)
{
	unsigned int reg_addr;
	unsigned char bit;
	unsigned int val;
	unsigned int mask = (1L << GPIO_MODE_BITS) - 1;

	reg_addr = ((pin / MAX_GPIO_MODE_PER_REG) << 4) + 0x01E0;
	mode &= mask;
	bit = pin % MAX_GPIO_MODE_PER_REG;
	mask <<= (GPIO_MODE_BITS * bit);
	val = (mode << (GPIO_MODE_BITS * bit));

	regmap_update_bits(reg, reg_addr, mask, val);
}

static const struct mtk_pinctrl_devdata mt8512_pinctrl_data = {
	.pins = mtk_pins_mt8512,
	.npins = ARRAY_SIZE(mtk_pins_mt8512),
	.grp_desc = mt8512_drv_grp,
	.n_grp_cls = ARRAY_SIZE(mt8512_drv_grp),
	.pin_drv_grp = mt8512_pin_drv,
	.n_pin_drv_grps = ARRAY_SIZE(mt8512_pin_drv),
	.pin_mode_grps = mt8512_pin_info_mode,
	.n_pin_mode = ARRAY_SIZE(mt8512_pin_info_mode),
	.spec_ies_smt_set = mt8512_ies_smt_set,
	.spec_ies_get = mt8512_spec_ies_get,
	.spec_smt_get = mt8512_spec_smt_get,
	.spec_pull_set = mt8512_spec_pull_set,
	.spec_pull_get = mt8512_spec_pull_get,
	.spec_pinmux_set = mt8512_spec_pinmux_set,
	.mtk_pctl_set_gpio_drv = mtk_pctl_set_gpio_drving,
	.dir_offset = 0x0140,
	.dout_offset = 0x00A0,
	.din_offset = 0x0000,
	.pinmux_offset = 0x01E0,
	.ies_offset = 0x0410,
	.smt_offset = 0x0470,
	.pullen_offset = 0x0860,
	.pullsel_offset = 0x0900,
	.drv_offset = 0x0710,
	.type1_start = 116,
	.type1_end = 116,
	.port_shf = 4,
	.port_mask = 0x1f,
	.port_align = 4,
	.port_pin_shf = 5,
	.eint_offsets = {
		.name = "mt8512_eint",
		.stat      = 0x000,
		.ack       = 0x040,
		.mask      = 0x080,
		.mask_set  = 0x0c0,
		.mask_clr  = 0x100,
		.sens      = 0x140,
		.sens_set  = 0x180,
		.sens_clr  = 0x1c0,
		.soft      = 0x200,
		.soft_set  = 0x240,
		.soft_clr  = 0x280,
		.pol       = 0x300,
		.pol_set   = 0x340,
		.pol_clr   = 0x380,
		.dom_en    = 0x400,
		.dbnc_ctrl = 0x500,
		.dbnc_set  = 0x600,
		.dbnc_clr  = 0x700,
		.port_mask = 7,
		.ports     = 4,
	},
	.ap_num = 124,
	.db_cnt = 124,
};

static int mtk_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_pctrl_init(pdev, &mt8512_pinctrl_data, NULL);
}

static const struct of_device_id mt8512_pctrl_match[] = {
	{
		.compatible = "mediatek,mt8512-pinctrl",
	},
	{ }
};

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mtk_pinctrl_probe,
	.driver = {
		.name = "mediatek-mt8512-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt8512_pctrl_match,
		.pm = &mtk_eint_pm_ops,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

arch_initcall(mtk_pinctrl_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("Zhiyong Tao <zhiyong.tao@mediatek.com>");
