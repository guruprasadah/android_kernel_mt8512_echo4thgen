/*
 * ak5704.h  --  audio driver for ak5704
 *
 * Copyright (C) 2018 Asahi Kasei Microdevices Corporation
 *	Author				  Date		  Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *						19/03/29	1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the	License, or (at your
 *	option) any later version.
 *
 */

#ifndef AK5704_H
#define AK5704_H

//#define AK5704_VCOM_OFF	 //VCOM selector off
//#define AK5704_AVDDL_1P8V  //AVDD 1.8V

#define AK5704_00_FLOW_CONTROLL						0x00
#define AK5704_01_POWER_MANAGEMENT_1					0x01
#define AK5704_02_POWER_MANAGEMENT_2					0x02
#define AK5704_03_POWER_MANAGEMENT_3					0x03
#define AK5704_04_MIC_INPUT_MIC_POWER_SETTING			0x04
#define AK5704_05_MIC_AMPLIFER_1_GAIN				0x05
#define AK5704_06_MIC_AMPLIFER_2_GAIN				0x06
#define AK5704_07_DIGITAL_MIC_SETTING				0x07
#define AK5704_08_CLOCK_MODE_SELECT					0x08
#define AK5704_09_PLL_CLK_SOURCE_SELECT				0x09
#define AK5704_0A_PLL_REF_CLK_DIVIDER_1				0x0A
#define AK5704_0B_PLL_REF_CLK_DIVIDER_2				0x0B
#define AK5704_0C_PLL_FB_CLK_DIVIDER_1				0x0C
#define AK5704_0D_PLL_FB_CLK_DIVIDER_2				0x0D
#define AK5704_0E_AUDIO_IF_FORMAL					0x0E
#define AK5704_0F_PHASE_CORRECTION_1A				0x0F
#define AK5704_10_PHASE_CORRECTION_1B				0x10
#define AK5704_11_PHASE_CORRECTION_2A				0x11
#define AK5704_12_PHASE_CORRECTION_2B				0x12
#define AK5704_13_ADC_HIGH_PASS_FILTER				0x13
#define AK5704_14_DIGITAL_FILTER_SELECT				0x14
#define AK5704_15_MIC_SENSITIVITY_ADJ_1A				0x15
#define AK5704_16_MIC_SENSITIVITY_ADJ_1B				0x16
#define AK5704_17_MIC_SENSITIVITY_ADJ_2A				0x17
#define AK5704_18_MIC_SENSITIVITY_ADJ_2B				0x18
#define AK5704_19_FILTER_1_SELECT					0x19
#define AK5704_1A_FILTER_2_SELECT					0x1A
#define AK5704_1B_VAD_SETTING_1						0x1B
#define AK5704_1C_VAD_SETTING_2						0x1C
#define AK5704_1D_VAD_SETTING_3						0x1D
#define AK5704_1E_VAD_SETTING_4						0x1E
#define AK5704_1F_VAD_SETTING_5						0x1F
#define AK5704_20_VAD_SETTING_6						0x20
#define AK5704_21_VAD_SETTING_7						0x21
#define AK5704_22_VAD_SETTING_8						0x22
#define AK5704_23_VAD_SETTING_9						0x23
#define AK5704_24_VAD_SETTING_10					0x24
#define AK5704_25_ALC_SELECT						0x25
#define AK5704_26_ALC_CONTROLL_1					0x26
#define AK5704_27_INPUT_DIGITAL_VOLUME_1A			0x27
#define AK5704_28_INPUT_DIGITAL_VOLUME_1B			0x28
#define AK5704_29_ALC_1_REFERENCE_LEVEL				0x29
#define AK5704_2A_ALC_1_CONTROLL					0x2A
#define AK5704_2B_INPUT_DIGITAL_VOLUME_2A			0x2B
#define AK5704_2C_INPUT_DIGITAL_VOLUME_2B			0x2C
#define AK5704_2D_ALC_2_REFERENCE_LEVEL				0x2D
#define AK5704_2E_ALC_2_CONTROLL					0x2E
#define AK5704_2F_HPF_1_COEFFICIENT_A				0x2F
#define AK5704_30_HPF_1_COEFFICIENT_A				0x30
#define AK5704_31_HPF_1_COEFFICIENT_B				0x31
#define AK5704_32_HPF_1_COEFFICIENT_B				0x32
#define AK5704_33_LPF_1_COEFFICIENT_A				0x33
#define AK5704_34_LPF_1_COEFFICIENT_A				0x34
#define AK5704_35_LPF_1_COEFFICIENT_B				0x35
#define AK5704_36_LPF_1_COEFFICIENT_B				0x36
#define AK5704_37_HPF_2_COEFFICIENT_A				0x37
#define AK5704_38_HPF_2_COEFFICIENT_A				0x38
#define AK5704_39_HPF_2_COEFFICIENT_B				0x39
#define AK5704_3A_HPF_2_COEFFICIENT_B				0x3A
#define AK5704_3B_LPF_2_COEFFICIENT_A				0x3B
#define AK5704_3C_LPF_2_COEFFICIENT_A				0x3C
#define AK5704_3D_LPF_2_COEFFICIENT_B				0x3D
#define AK5704_3E_LPF_2_COEFFICIENT_B				0x3E
#define AK5704_3F_VAHPF_COEFFICIENT_A				0x3F
#define AK5704_40_VAHPF_COEFFICIENT_A				0x40
#define AK5704_41_VAHPF_COEFFICIENT_B				0x41
#define AK5704_42_VAHPF_COEFFICIENT_B				0x42
#define AK5704_43_VALPF_COEFFICIENT_A				0x43
#define AK5704_44_VALPF_COEFFICIENT_A				0x44
#define AK5704_45_VALPF_COEFFICIENT_B				0x45
#define AK5704_46_VALPF_COEFFICIENT_B				0x46
#define AK5704_MAX_REGISTER							AK5704_46_VALPF_COEFFICIENT_B


#define AK5704_DIF					0x03
#define AK5704_DIF_I2S_MODE			(0 << 0)
#define AK5704_DIF_MSB_MODE			(1 << 0)
#define AK5704_DIF_PCM_S_MODE			(2 << 0)
#define AK5704_DIF_PCM_L_MODE			(3 << 0)

#define AK5704_TDM					0x0c
#define AK5704_TDM_32_64			(0 << 2)
#define AK5704_TDM_TDM128			(1 << 2)
#define AK5704_TDM_TDM256			(2 << 2)
#define AK5704_TDM_TDM512			(3 << 2)

#define AK5704_MSN						0x20
#define AK5704_MSN_ON					(1 << 5)
#define AK5704_MSN_OFF					(0 << 5)

//PLL
#define PLLOUT_CLOCK_48  61440000
#define PLLOUT_CLOCK_44  56448000

// 1 = power up
#define AK5704_PMPFIL1_MASK 0x1
#define AK5704_PMPFIL2_MASK 0x2

// differential inputs
#define AK5704_DIFF_INPUT_ALL_MASK 0xF0

#endif /* AK5704_H */
