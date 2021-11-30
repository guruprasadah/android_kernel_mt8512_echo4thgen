/*
 * Register settings for TAS5805M Audio Amplifier as mono and stereo
 *
 * Copyright (C) 2019 Amazon Lab126 Inc - http://www.lab126.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/regmap.h>

static const struct reg_sequence tas5805m_init_stereo[] = {
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x03, 0x03 },
	{ 0x78, 0x80 },
	{ 0x33, 0x03 },
};

int tas5805m_reset_platform(struct snd_soc_codec *codec, int enable);