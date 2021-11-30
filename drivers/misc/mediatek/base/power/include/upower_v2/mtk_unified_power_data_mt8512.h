/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef MTK_UNIFIED_POWER_DATA_MT8512_H
#define MTK_UNIFIED_POWER_DATA_MT8512_H

struct upower_tbl upower_tbl_l_FY = {
	.row = {
		{.cap = 356, .volt = 80000, .dyn_pwr = 35081,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 396, .volt = 80000, .dyn_pwr = 38940,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 435, .volt = 80000, .dyn_pwr = 42858,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 488, .volt = 80000, .dyn_pwr = 48704,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 544, .volt = 80000, .dyn_pwr = 56481,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 629, .volt = 80000, .dyn_pwr = 66245,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 694, .volt = 80000, .dyn_pwr = 74021,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 759, .volt = 80000, .dyn_pwr = 81856,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 789, .volt = 82500, .dyn_pwr = 91218,
			.lkg_pwr = {35267, 35267, 35267, 35267, 35267, 35267} },
		{.cap = 818, .volt = 85000, .dyn_pwr = 101253,
			.lkg_pwr = {36916, 36916, 36916, 36916, 36916, 36916} },
		{.cap = 859, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 898, .volt = 91250, .dyn_pwr = 129470,
			.lkg_pwr = {41159, 41159, 41159, 41159, 41159, 41159} },
		{.cap = 931, .volt = 94375, .dyn_pwr = 145324,
			.lkg_pwr = {43527, 43527, 43527, 43527, 43527, 43527} },
		{.cap = 970, .volt = 97500, .dyn_pwr = 162403,
			.lkg_pwr = {45894, 45894, 45894, 45894, 45894, 45894} },
		{.cap = 999, .volt = 100000, .dyn_pwr = 177051,
			.lkg_pwr = {47788, 47788, 47788, 47788, 47788, 47788} },
		{.cap = 1024, .volt = 102500, .dyn_pwr = 191965,
			.lkg_pwr = {50523, 50523, 50523, 50523, 50523, 50523} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = 16,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {33619} },
		{{0}, {33619} },
		{{0}, {33619} },
		{{0}, {33619} },
		{{0}, {33619} },
		{{0}, {33619} },
	},
};

struct upower_tbl upower_tbl_l_FY2 = {
	.row = {
		{.cap = 671, .volt = 65000, .dyn_pwr = 23159,
			.lkg_pwr = {25527, 25527, 25527, 25527, 25527, 25527} },
		{.cap = 820, .volt = 67500, .dyn_pwr = 30511,
			.lkg_pwr = {26792, 26792, 26792, 26792, 26792, 26792} },
		{.cap = 919, .volt = 69375, .dyn_pwr = 36626,
			.lkg_pwr = {27740, 27740, 27740, 27740, 27740, 27740} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
		{.cap = 1024, .volt = 71875, .dyn_pwr = 45591,
			.lkg_pwr = {29099, 29099, 29099, 29099, 29099, 29099} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = 4,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
	},
};

struct upower_tbl upower_tbl_l_FY3 = {
	.row = {
		{.cap = 425, .volt = 65000, .dyn_pwr = 23159,
			.lkg_pwr = {25527, 25527, 25527, 25527, 25527, 25527} },
		{.cap = 582, .volt = 69375, .dyn_pwr = 36626,
			.lkg_pwr = {27740, 27740, 27740, 27740, 27740, 27740} },
		{.cap = 905, .volt = 80000, .dyn_pwr = 81856,
			.lkg_pwr = {33619, 33619, 33619, 33619, 33619, 33619} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
		{.cap = 1024, .volt = 88125, .dyn_pwr = 114794,
			.lkg_pwr = {38976, 38976, 38976, 38976, 38976, 38976} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = 4,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
		{{0}, {25527} },
	},
};


#endif /* UNIFIED_POWER_DATA_MT8512H */
