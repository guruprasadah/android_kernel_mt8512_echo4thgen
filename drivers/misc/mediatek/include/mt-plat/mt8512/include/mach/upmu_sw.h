/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MT6397_PMIC_UPMU_SW_H_
#define _MT6397_PMIC_UPMU_SW_H_

#include <linux/module.h>
#include <mach/upmu_hw.h>

#define PMIC_ISINK_RSV2_ISINK0_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK0_SHIFT 13
#define PMIC_ISINK_RSV2_ISINK1_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK1_SHIFT 14
#define PMIC_ISINK_RSV2_ISINK2_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK2_SHIFT 15

/* =============================================================== */
extern unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val,
					  unsigned int MASK,
					  unsigned int SHIFT);
extern unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val,
					unsigned int MASK, unsigned int SHIFT);
extern u32 upmu_get_reg_value(u32 reg);
extern void upmu_set_reg_value(u32 reg, u32 reg_val);

static inline void upmu_set_rg_source_ch0_norm_sel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(unsigned int)(AUXADC_CON14), (unsigned int)(val),
		(unsigned int)(PMIC_RG_SOURCE_CH0_NORM_SEL_MASK),
		(unsigned int)(PMIC_RG_SOURCE_CH0_NORM_SEL_SHIFT));
}

static inline void upmu_set_rg_source_ch0_lbat_sel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		(unsigned int)(AUXADC_CON14), (unsigned int)(val),
		(unsigned int)(PMIC_RG_SOURCE_CH0_LBAT_SEL_MASK),
		(unsigned int)(PMIC_RG_SOURCE_CH0_LBAT_SEL_SHIFT));
}

static inline void upmu_set_rg_buf_pwd_on(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON0),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_BUF_PWD_ON_MASK),
				    (unsigned int)(PMIC_RG_BUF_PWD_ON_SHIFT));
}

static inline void upmu_set_rg_adc_pwd_on(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON0),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_ADC_PWD_ON_MASK),
				    (unsigned int)(PMIC_RG_ADC_PWD_ON_SHIFT));
}

static inline void upmu_set_rg_buf_pwd_b(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON0),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_BUF_PWD_B_MASK),
				    (unsigned int)(PMIC_RG_BUF_PWD_B_SHIFT));
}

static inline void upmu_set_rg_adc_pwd_b(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON0),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_ADC_PWD_B_MASK),
				    (unsigned int)(PMIC_RG_ADC_PWD_B_SHIFT));
}

static inline void upmu_set_rg_auxadc_chsel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON1),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_AUXADC_CHSEL_MASK),
				    (unsigned int)(PMIC_RG_AUXADC_CHSEL_SHIFT));
}

static inline void upmu_set_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(CHR_CON7),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_BATON_TDET_EN_MASK),
				    (unsigned int)(PMIC_BATON_TDET_EN_SHIFT));
}

static inline void upmu_set_rg_vbuf_calen(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON12),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_VBUF_CALEN_MASK),
				    (unsigned int)(PMIC_RG_VBUF_CALEN_SHIFT));
}

static inline void upmu_set_rg_spl_num(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON0),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_SPL_NUM_MASK),
				    (unsigned int)(PMIC_RG_SPL_NUM_SHIFT));
}

static inline void upmu_set_rg_auxadc_start(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON1),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_AUXADC_START_MASK),
				    (unsigned int)(PMIC_RG_AUXADC_START_SHIFT));
}

static inline unsigned int upmu_get_rg_adc_rdy_c0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC0), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C0_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C0_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC0), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C0_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C0_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC1), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C1_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C1_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC1), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C1_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C1_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC2), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C2_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C2_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC2), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C2_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C2_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC3), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C3_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C3_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC3), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C3_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C3_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC4), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C4_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C4_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC4), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C4_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C4_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC5), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C5_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C5_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC5), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C5_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C5_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC6), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C6_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C6_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC6), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C6_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C6_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC7), (&val),
				  (unsigned int)(PMIC_RG_ADC_RDY_C7_MASK),
				  (unsigned int)(PMIC_RG_ADC_RDY_C7_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(AUXADC_ADC7), (&val),
				  (unsigned int)(PMIC_RG_ADC_OUT_C7_MASK),
				  (unsigned int)(PMIC_RG_ADC_OUT_C7_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC8), (&val),
		(unsigned int)(PMIC_RG_ADC_RDY_WAKEUP_PCHR_MASK),
		(unsigned int)(PMIC_RG_ADC_RDY_WAKEUP_PCHR_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC8), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_WAKEUP_PCHR_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_WAKEUP_PCHR_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC9), (&val),
		(unsigned int)(PMIC_RG_ADC_RDY_WAKEUP_SWCHR_MASK),
		(unsigned int)(PMIC_RG_ADC_RDY_WAKEUP_SWCHR_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC9), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_WAKEUP_SWCHR_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_WAKEUP_SWCHR_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c0_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC11), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C0_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C0_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c1_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC12), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C1_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C1_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c2_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC13), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C2_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C2_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c3_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC14), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C3_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C3_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c4_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC15), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C4_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C4_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c5_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC16), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C5_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C5_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c6_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC17), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C6_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C6_TRIM_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c7_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		(unsigned int)(AUXADC_ADC18), (&val),
		(unsigned int)(PMIC_RG_ADC_OUT_C7_TRIM_MASK),
		(unsigned int)(PMIC_RG_ADC_OUT_C7_TRIM_SHIFT));

	return val;
}

static inline void upmu_set_rg_vbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON12),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_VBUF_EN_MASK),
				    (unsigned int)(PMIC_RG_VBUF_EN_SHIFT));
}

static inline void upmu_set_rg_vbuf_byp(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int)(AUXADC_CON12),
				    (unsigned int)(val),
				    (unsigned int)(PMIC_RG_VBUF_BYP_MASK),
				    (unsigned int)(PMIC_RG_VBUF_BYP_SHIFT));
}

static inline unsigned int upmu_get_cid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int)(CID), (&val),
				  (unsigned int)(PMIC_CID_MASK),
				  (unsigned int)(PMIC_CID_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rgs_bc11_cmp_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (CHR_CON18),
				  (&val),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_MASK),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_SHIFT));

	return val;
}

static inline void upmu_set_rg_bc11_vsrc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON18),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_BC11_VSRC_EN_MASK),
			(unsigned int) (PMIC_RG_BC11_VSRC_EN_SHIFT));
}

static inline void upmu_set_rg_bc11_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_RST_MASK),
				    (unsigned int) (PMIC_RG_BC11_RST_SHIFT));
}

static inline void upmu_set_rg_bc11_bb_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON18),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_BC11_BB_CTRL_MASK),
			(unsigned int) (PMIC_RG_BC11_BB_CTRL_SHIFT));
}

static inline void upmu_set_rg_bc11_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON19),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_BC11_BIAS_EN_MASK),
			(unsigned int) (PMIC_RG_BC11_BIAS_EN_SHIFT));
}

static inline void upmu_set_rg_bc11_ipu_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_SHIFT));
}

static inline void upmu_set_rg_bc11_ipd_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_SHIFT));
}

static inline void upmu_set_rg_bc11_cmp_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_SHIFT));
}

static inline void upmu_set_rg_bc11_vref_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON19),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_BC11_VREF_VTH_MASK),
			(unsigned int) (PMIC_RG_BC11_VREF_VTH_SHIFT));
}

static inline void upmu_set_rg_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_EN_MASK),
				    (unsigned int) (PMIC_RG_BATON_EN_SHIFT));
}

static inline unsigned int upmu_get_rgs_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (CHR_CON7),
				  (&val),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_MASK),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_SHIFT));

	return val;
}

static inline unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHRDET_MASK),
				  (unsigned int) (PMIC_RGS_CHRDET_SHIFT));

	return val;
}

static inline void upmu_set_rg_vcdt_hv_vth(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (CHR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_SHIFT));
}

static inline unsigned int upmu_get_rgs_vcdt_hv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_SHIFT));

	return val;
}

static inline unsigned int upmu_get_fg_current_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (FGADC_CON8),
				  (&val),
				  (unsigned int) (PMIC_FG_CURRENT_OUT_MASK),
				  (unsigned int) (PMIC_FG_CURRENT_OUT_SHIFT));

	return val;
}

static inline unsigned int upmu_get_fg_car_35_32(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (FGADC_CON1),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_35_32_MASK),
				  (unsigned int) (PMIC_FG_CAR_35_32_SHIFT));

	return val;
}

static inline unsigned int upmu_get_fg_car_31_16(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (FGADC_CON2),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_31_16_MASK),
				  (unsigned int) (PMIC_FG_CAR_31_16_SHIFT));

	return val;
}

static inline unsigned int upmu_get_fg_car_15_00(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface((unsigned int) (FGADC_CON3),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_15_00_MASK),
				  (unsigned int) (PMIC_FG_CAR_15_00_SHIFT));

	return val;
}

static inline void upmu_set_rg_fgadc_ana_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_FGADC_ANA_CK_PDN_MASK),
			(unsigned int) (PMIC_RG_FGADC_ANA_CK_PDN_SHIFT));
}

static inline void upmu_set_rg_fgadc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
			(unsigned int) (val),
			(unsigned int) (PMIC_RG_FGADC_CK_PDN_MASK),
			(unsigned int) (PMIC_RG_FGADC_CK_PDN_SHIFT));
}

#endif
