/*******************************************************************************
 * DSP HAL device driver board-specific functions
 *
 * Copyright 2020 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/gpl-2.0.html>.
 *
 *******************************************************************************/

typedef enum amzn_dsp_fw_load {
	DSP_FW_LOAD_UNLOADED = 0,
	DSP_FW_LOAD_IN_PROCESS,
	DSP_FW_LOAD_ERROR,
	DSP_FW_LOAD_COMPLETE,
} amzn_dsp_fw_load_t;

amzn_dsp_fw_load_t amzn_dsp_fw_load_status(void);
