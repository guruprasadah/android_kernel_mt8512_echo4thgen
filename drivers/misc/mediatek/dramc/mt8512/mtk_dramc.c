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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
/* #include <mach/mtk_clkmgr.h> */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <asm/setup.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/dma.h>
#include <mt-plat/sync_write.h>
#ifdef DVFS_READY
#include <mtk_spm_vcore_dvfs.h>
#endif

#include "mtk_dramc.h"
#include "dramc.h"
#ifdef EMI_READY
#include "mt_emi_api.h"
#endif

#include <mt-plat/aee.h>
#include <mt-plat/mtk_chip.h>

void __iomem *DRAMC_NAO_BASE_ADDR;
void __iomem *DRAMC_AO_BASE_ADDR;
void __iomem *DDRPHY_AO_CHA_BASE_ADDR;

unsigned int DRAM_TYPE;

/*extern bool spm_vcorefs_is_dvfs_in_porgress(void);*/
#define Reg_Sync_Writel(addr, val)	 writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))

#define DRAMC_TAG "[DRAMC]"
#define DRAMC_RSV_TAG "[DRAMC_RSV]"

#define dramc_info(format, ...)	pr_debug(DRAMC_TAG format, ##__VA_ARGS__)

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned long mem_ptr;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;
	int ret = 1;

	ptr = vmalloc(MEM_TEST_SIZE);

	if (!ptr) {
		/*dramc_info("fail to vmalloc\n");*/
		/*ASSERT(0);*/
		ret = -24;
		goto fail;
	}

	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	/* dramc_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr); */
	dramc_info("Test DRAM start address %p\n", ptr);
	dramc_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			/* return -1; */
			ret = -1;
			goto fail;
		} else
			MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -2; */
			ret = -2;
			goto fail;
		} else
			MEM32_BASE[i] = 0x00;
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		MEM8_BASE[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (MEM8_BASE[i] != pattern8++) {
			/* return -3; */
			ret = -3;
			goto fail;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			/* return -4; */
			ret = -4;
			goto fail;
		}
		pattern8 += 2;
	}

	/* === Verify pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		MEM16_BASE[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (MEM16_BASE[i] != pattern16++) {
			/* return -5; */
			ret = -5;
			goto fail;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			/* return -6; */
			ret = -6;
			goto fail;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			/* return -7; */
			ret = -7;
			goto fail;
		} else {
			MEM32_BASE[i] = 0xa5a5a5a5;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			/* return -8; */
			ret = -8;
			goto fail;
		} else {
			MEM8_BASE[i * 4] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			/* return -9; */
			ret = -9;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 2] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			/* return -10; */
			ret = -10;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 1] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with */
	/* 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			/* return -11; */
			ret = -11;
			goto fail;
		} else {
			MEM8_BASE[i * 4 + 3] = 0x00;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			/* return -12; */
			ret = -12;
			goto fail;
		} else {
			MEM16_BASE[i * 2 + 1] = 0xffff;
		}
	}

	/* === Read Check then Fill Memory with ffff */
	/* Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			/* return -13; */
			ret = -13;
			goto fail;
		} else {
			MEM16_BASE[i * 2] = 0xffff;
		}
	}
	/*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			/* return -14; */
			ret = -14;
			goto fail;
		}
	}

	/************************************************
	 * Additional verification
	 ************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++)
		MEM_BASE[i] = PATTERN1;

	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];

		if (value != PATTERN1) {
			/* return -15; */
			ret = -15;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -16; */
			ret = -16;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -17; */
			ret = -17;
			goto fail;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			/* return -18; */
			ret = -18;
			goto fail;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			/* return -19; */
			ret = -19;
			goto fail;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)MEM_BASE;
	while (mem_ptr < ((unsigned long)MEM_BASE + (size << 2))) {
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *)mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *)mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *)mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *)mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *)mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *)mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			/* return -20; */
			ret = -20;
			goto fail;
		}
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	MEM8_BASE[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;

		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			MEM8_BASE[waddr8] = pattern8 + 1;
		if (MEM8_BASE[raddr8] != pattern8) {
			/* return -21; */
			ret = -21;
			goto fail;
		}
		pattern8++;
	}

	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	MEM16_BASE[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			MEM16_BASE[i + 1] = pattern16 + 1;
		if (MEM16_BASE[i] != pattern16) {
			/* return -22; */
			ret = -22;
			goto fail;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	MEM32_BASE[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			MEM32_BASE[i + 1] = pattern32 + 1;
		if (MEM32_BASE[i] != pattern32) {
			/* return -23; */
			ret = -23;
			goto fail;
		}
		pattern32++;
	}
	dramc_info("complex R/W mem test pass\n");

fail:
	vfree(ptr);
	return ret;
}


unsigned int get_shuffle_status(void)
{
	return (Reg_Readl(DRAMC_REG_SHUSTATUS) & 0x6) >> 1;
	/* HPM = 0, LPM = 1, ULPM = 2; */
}

int dram_get_type(void)
{
	unsigned int type;

	type = (Reg_Readl(DRAMC_AO_ARBCTL) >> 10) & 0x7;

	switch (type) {
	case 0:
		return DRAM_TYPE_PSRAM;
	case 1:
		return DRAM_TYPE_LPDDR3;
	case 2:
		return DRAM_TYPE_LPDDR4;
	default:
		return -1;
	}
}
EXPORT_SYMBOL(dram_get_type);

/* Because DRAM data rate in RG value isn't accurate
 * and also can be changed by PCW, so need to map.
 * It's for DVFS standard dram date rate.
 * data_rate: MHz, upper_range, lower_range: MHz.
 */
static int get_mapped_dram_rate(int dram_type, int rough_data_rate, int increment, int decrement/* < 0*/)
{
	static int lp4_rough_data_rate[] =	{3198, 3068, 2392, 1599, 1534, 1196};
	static int lp4_standard_data_rate[] =	{3200, 3068, 2400, 1600, 1534, 1200};
	int i;
	unsigned int arr_size = sizeof(lp4_rough_data_rate) / sizeof(lp4_rough_data_rate[0]);

	if (dram_type == DRAM_TYPE_LPDDR4) {
		int upper, lower;

		/* if the data rate is in rough data rate array, just return the standard data rate. */
		for (i = 0; i < arr_size; ++i) {
			if (lp4_rough_data_rate[i] == rough_data_rate)
				return lp4_standard_data_rate[i];
		}

		/* note: decrement should be < 0 */
		if (decrement > 0)
			decrement = -decrement;
		/* use range compare */
		upper = rough_data_rate + increment;
		lower = rough_data_rate + decrement;

		for (i = 0; i < arr_size; ++i) {
			if (lp4_standard_data_rate[i] >= lower && lp4_standard_data_rate[i] <= upper)
				return lp4_standard_data_rate[i];
		}

		return 0;
	}

	return 0;
}

unsigned int get_dram_data_rate(void)
{
	int shu_level;
	void *pll5, *pll8, *ca_cmd6;
	int pcw, pre_div, post_div, ck_div4;
	int temp_freq, data_rate, rough_data_rate;

	shu_level = (Reg_Readl(DRAMC_REG_SHUSTATUS) >> 1) & 0x3;
	pll5 = DDRPHY_SHU1_PLL5 + SHU_GRP_DDRPHY_OFFSET * shu_level;
	pll8 = DDRPHY_SHU1_PLL8 + SHU_GRP_DDRPHY_OFFSET * shu_level;
	ca_cmd6 = DDRPHY_SHU1_CA_CMD6 + SHU_GRP_DDRPHY_OFFSET * shu_level;

	pcw = (Reg_Readl(pll5) >> 16) & 0xFFFF;
	pre_div = (Reg_Readl(pll8) >> 18) & 0x3;
	post_div = (Reg_Readl(pll8) & 0x7);
	ck_div4 = (Reg_Readl(ca_cmd6) >> 27) & 0x1;

	temp_freq = ((52 >> pre_div) * (pcw >> 8)) >> post_div;
	rough_data_rate = temp_freq >> ck_div4;

#if 1
	if (DRAM_TYPE == DRAM_TYPE_LPDDR3) {
		if (rough_data_rate == 1859)
			data_rate = 1866;
		else if (rough_data_rate == 1599)
			data_rate = 1600;
		else if (rough_data_rate == 1534)
			data_rate = 1534;
		else if (rough_data_rate == 1196)
			data_rate = 1200;
		else
			data_rate = 0;
	} else if ((DRAM_TYPE == DRAM_TYPE_LPDDR4)
			/* || (DRAM_TYPE == TYPE_LPDDR4X)*/) {
		/* 130 is just a estimate value, can changed by demand */
		data_rate = get_mapped_dram_rate(DRAM_TYPE_LPDDR4, rough_data_rate, 130, -130);
	} else
		data_rate = 0;
#else
	data_rate = 0;
	switch (pcw) {
	case 0x7B00:
		if (pre_div == 1  && post_div == 0 && ck_div4 == 0)
			data_rate = 3200;
		break;
	case 0x5C00:
		if (pre_div == 1  && post_div == 0 && ck_div4 == 0)
			data_rate = 2400;
		else if (pre_div == 1  && post_div == 0 && ck_div4 == 1)
			data_rate = 1200;
		break;
	}
#endif

	return data_rate;
}
EXPORT_SYMBOL(get_dram_data_rate);

unsigned int dram_get_data_rate_by_shu_level(int shu_level)
{
	void *pll5, *pll8, *ca_cmd6;
	int pcw, pre_div, post_div, ck_div4;
	unsigned int temp_freq, data_rate, rough_data_rate;

	if (shu_level < 0 || shu_level > 3) {
		pr_info("wrong shu_level:%d, changed to %d.\n", shu_level, 0);
		shu_level = 0;
	}

	pll5 = DDRPHY_SHU1_PLL5 + SHU_GRP_DDRPHY_OFFSET * shu_level;
	pll8 = DDRPHY_SHU1_PLL8 + SHU_GRP_DDRPHY_OFFSET * shu_level;
	ca_cmd6 = DDRPHY_SHU1_CA_CMD6 + SHU_GRP_DDRPHY_OFFSET * shu_level;

	pcw = (Reg_Readl(pll5) >> 16) & 0xFFFF;
	pre_div = (Reg_Readl(pll8) >> 18) & 0x3;
	post_div = (Reg_Readl(pll8) & 0x7);
	ck_div4 = (Reg_Readl(ca_cmd6) >> 27) & 0x1;

	temp_freq = ((52 >> pre_div) * (pcw >> 8)) >> post_div;
	rough_data_rate = temp_freq >> ck_div4;

#if 1
		if (DRAM_TYPE == DRAM_TYPE_LPDDR3) {
			if (rough_data_rate == 1859)
				data_rate = 1866;
			else if (rough_data_rate == 1599)
				data_rate = 1600;
			else if (rough_data_rate == 1534)
				data_rate = 1534;
			else if (rough_data_rate == 1196)
				data_rate = 1200;
			else
				data_rate = 0;
		} else if ((DRAM_TYPE == DRAM_TYPE_LPDDR4)
			/* || (DRAM_TYPE == TYPE_LPDDR4X)*/) {
			if (rough_data_rate == 3198)
				data_rate = 3200;
			else if (rough_data_rate == 3068)
				data_rate = 3068;
			else if (rough_data_rate == 2392)
				data_rate = 2400;
			else if (rough_data_rate == 1599)
				data_rate = 1600;
			else if (rough_data_rate == 1534)
				data_rate = 1534;
			else if (rough_data_rate == 1196)
				data_rate = 1200;
			else
				data_rate = 0;
		} else
			data_rate = 0;
#else
		data_rate = 0;
		switch (pcw) {
		case 0x7B00:
			if (pre_div == 1  && post_div == 0 && ck_div4 == 0)
				data_rate = 3200;
			break;
		case 0x5C00:
			if (pre_div == 1  && post_div == 0 && ck_div4 == 0)
				data_rate = 2400;
			else if (pre_div == 1  && post_div == 0 && ck_div4 == 1)
				data_rate = 1200;
			break;
		}
#endif

	pr_warn("pcw:0x%x, pre_div:%d, post_div:%d, ck_div4:%d,",
			pcw, pre_div, post_div, ck_div4);
	pr_warn("data_rate:%uMbps, rough_data_rate:%uMbps\n",
			data_rate, rough_data_rate);

	return data_rate;
}


int dram_steps_freq(int dram_type, int step)
{
	int freq = -1;

	switch (step) {
	case 0:
		if (dram_type == DRAM_TYPE_LPDDR3)
			freq = 1866;
		else if (dram_type == DRAM_TYPE_LPDDR4)
			freq = 3200;
		break;
	case 1:
		if (dram_type == DRAM_TYPE_LPDDR3)
			freq = 1600;
		else if (dram_type == DRAM_TYPE_LPDDR4)
			freq = 2400;
		break;
	case 2:
		if (dram_type == DRAM_TYPE_LPDDR3)
			freq = 1200;
		else if (dram_type == DRAM_TYPE_LPDDR4)
			freq = 1200;
		break;
	default:
		return -1;
	}
	return freq;
}
EXPORT_SYMBOL(dram_steps_freq);

static char *dram_type_to_string(int type)
{
	switch (type) {
	case 0:
		return "PSRAM";
	case 1:
		return "LPDDR3";
	case 2:
		return "LPDDR4";
	default:
		return "unknown dram type!";
	}

	return "unknown dram type!";
}

static ssize_t dram_type_show(struct device_driver *driver, char *buf)
{
	int type;

	type = dram_get_type();
	return snprintf(buf, PAGE_SIZE, "dram type:%s\n",
				dram_type_to_string(type));
}

DRIVER_ATTR(dram_type, 0444, dram_type_show, NULL);


static ssize_t dram_date_rate_show(struct device_driver *driver, char *buf)
{
	unsigned int data_rate;
	unsigned int shu0_data_rate, shu1_data_rate, shu2_data_rate;

	data_rate = get_dram_data_rate();
	shu0_data_rate = dram_get_data_rate_by_shu_level(0);
	shu1_data_rate = dram_get_data_rate_by_shu_level(1);
	shu2_data_rate = dram_get_data_rate_by_shu_level(2);

	return snprintf(buf, PAGE_SIZE,
		"cur dram data rate:%u, shu0:%u, shu1:%u, shu2:%u\n",
		data_rate, shu0_data_rate, shu1_data_rate, shu2_data_rate);
}

DRIVER_ATTR(dram_data_rate, 0444, dram_date_rate_show, NULL);


static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = Binning_DRAM_complex_mem_test();
	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
}

static ssize_t complex_mem_test_store(struct device_driver *driver,
const char *buf, size_t count)
{
	/*snprintf(buf, "do nothing\n");*/
	return count;
}

static ssize_t read_dram_data_rate_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		" DRAM data rate = %d\n", get_dram_data_rate());
}

static ssize_t read_dram_data_rate_store(struct device_driver *driver,
const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(emi_clk_mem_test, 0664,
complex_mem_test_show, complex_mem_test_store);

DRIVER_ATTR(read_dram_data_rate, 0664,
read_dram_data_rate_show, read_dram_data_rate_store);


static int dram_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i;
	struct resource *res;
	void __iomem *base_temp[3];

	pr_info("[DRAMC] module probe.\n");

	for (i = 0; i < (sizeof(base_temp) / sizeof(*base_temp)); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		base_temp[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base_temp[i])) {
			dramc_info("unable to map %d base\n", i);
			return -EINVAL;
		}
	}

	DDRPHY_AO_CHA_BASE_ADDR = base_temp[0];
	DRAMC_AO_BASE_ADDR = base_temp[1];
	DRAMC_NAO_BASE_ADDR = base_temp[2];

	DRAM_TYPE = dram_get_type();

	dramc_info("dram type =%d\n", DRAM_TYPE);
	dramc_info("Dram Data Rate = %u\n", get_dram_data_rate());
	dramc_info("shuffle_status = %d\n", get_shuffle_status());

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_emi_clk_mem_test);
	if (ret) {
		dramc_info("fail to create emi_clk_mem_test sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_read_dram_data_rate);
	if (ret) {
		dramc_info("fail to create read_dram_data_rate sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_dram_data_rate);
	if (ret) {
		dramc_info("fail to create dram_data_rate sysfs files\n");
		return ret;
	}

	ret = driver_create_file(pdev->dev.driver,
	&driver_attr_dram_type);
	if (ret) {
		dramc_info("fail to create dram_type sysfs files\n");
		return ret;
	}

	return 0;
}

static int dram_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dram_of_ids[] = {
	{.compatible = "mediatek,mt8512-dramc",},
	{}
};
#endif

static struct platform_driver dram_test_drv = {
	.probe = dram_probe,
	.remove = dram_remove,
	.driver = {
		.name = "dram_test",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dram_of_ids,
#endif
		},
};

/* int __init dram_test_init(void) */
static int __init dram_test_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&dram_test_drv);
	if (ret) {
		dramc_info("init fail, ret 0x%x\n", ret);
		return ret;
	}

	return ret;
}

static void __exit dram_test_exit(void)
{
	platform_driver_unregister(&dram_test_drv);
}

postcore_initcall(dram_test_init);
module_exit(dram_test_exit);

void *mt_dramc_chn_base_get(int channel)
{
	return DRAMC_AO_BASE_ADDR;
}
EXPORT_SYMBOL(mt_dramc_chn_base_get);

void *mt_dramc_nao_chn_base_get(int channel)
{
	return DRAMC_NAO_BASE_ADDR;
}
EXPORT_SYMBOL(mt_dramc_nao_chn_base_get);

void *mt_ddrphy_chn_base_get(int channel)
{
	switch (channel) {
	case 0:
		return DDRPHY_AO_CHA_BASE_ADDR;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(mt_ddrphy_chn_base_get);

MODULE_DESCRIPTION("MediaTek DRAMC Driver v0.1");
