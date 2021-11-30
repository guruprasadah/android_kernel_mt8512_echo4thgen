/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef MT_SPOWER_CPU_H
#define MT_SPOWER_CPU_H

#define VSIZE 9
#define TSIZE 18

/***************************/
/* "(WAT -8.6%)	   */
/* Leakage Power"	   */
/***************************/
#define CA53_TABLE_0                                                           \
	/**/ {650, 700, 750, 800, 850, 900, 950, 1000, 1050, \
		20, 23, 26, 29, 33, 37, 41, 46, 52, 58, \
		25, 27, 30, 34, 38, 43, 48, 54, 61, 69, \
		30, 32, 36, 40, 45, 51, 57, 64, 72, 81, \
		35, 37, 42, 47, 53, 60, 67, 75, 84, 95, \
		40, 44, 49, 55, 62, 70, 79, 88, 99, 112, \
		45, 52, 58, 65, 73, 82, 92, 104, 117, 131, \
		50, 61, 68, 77, 86, 97, 108, 122, 137, 154, \
		55, 71, 80, 90, 101, 113, 127, 143, 161, 181, \
		60, 84, 94, 105, 119, 133, 150, 168, 189, 212, \
		65, 98, 110, 124, 139, 157, 176, 198, 222, 249, \
		70, 115, 130, 146, 164, 184, 207, 232, 261, 293, \
		75, 136, 152, 171, 192, 216, 243, 273, 306, 344, \
		80, 159, 179, 201, 226, 254, 285, 320, 360, 404, \
		85, 187, 210, 236, 265, 298, 335, 376, 423, 475, \
		90, 220, 247, 277, 312, 350, 393, 442, 496, 558, \
		95, 258, 290, 326, 366, 411, 462, 519, 583, 655, \
		100, 303, 341, 383, 430, 483, 543, 610, 685, 770, \
		105, 356, 400, 450, 505, 567, 638, 716, 805, 904}

/******************/
/* "(WAT -3%)	  */
/* Leakage Power" */
/******************/
#define CA53_TABLE_1                                                           \
	/**/ {650, 700, 750, 800, 850, 900, 950, 1000, 1050, \
		20, 7, 8, 9, 10, 11, 13, 14, 16, 18, \
		25, 8, 9, 11, 12, 13, 15, 17, 19, 21, \
		30, 10, 11, 12, 14, 16, 18, 20, 22, 25, \
		35, 12, 13, 15, 16, 18, 21, 23, 26, 29, \
		40, 14, 15, 17, 19, 22, 24, 27, 31, 34, \
		45, 16, 18, 20, 23, 25, 28, 32, 36, 40, \
		50, 19,  21, 24, 27, 30, 33, 38, 42, 47, \
		55, 22, 25, 28, 31, 35, 39, 44, 50, 56, \
		60, 26, 29, 33, 37, 41, 46, 52, 58, 65, \
		65, 30, 34, 38, 43, 48, 54, 61, 68, 77, \
		70, 36, 40, 45, 50, 57, 64, 72, 80, 90, \
		75, 42, 47, 53, 59, 67, 75, 84, 94, 106, \
		80, 49, 55, 62, 70, 78, 88, 99, 111, 125, \
		85, 58, 65, 73, 82, 92, 103, 116, 130, 146, \
		90, 68, 76, 86, 96, 108, 121, 136, 153, 172, \
		95, 80, 89, 100, 113, 127, 142, 160, 180, 202, \
		100, 93, 105, 118, 133, 149, 167, 188, 211, 237, \
		105, 110, 123, 139, 156, 174, 197, 221, 248, 279}

/******************/
/* "(WAT -2.5%)	  */
/* Leakage Power" */
/******************/
#define CA53_TABLE_2                                                           \
	/**/ {650, 700, 750, 800, 850, 900, 950, 1000, 1050, \
		20, 2, 3, 3, 3, 4, 4, 5, 5, 6, \
		25, 3, 3, 4, 4, 4, 5, 6, 6, 7, \
		30, 3, 4, 4, 5, 5, 6, 7, 7, 8, \
		35, 4, 4, 5, 5, 6, 7, 8, 9, 10, \
		40, 5, 5, 6, 6, 7, 8, 9, 10, 11, \
		45, 5, 6, 7, 8, 8, 9, 11, 12, 13, \
		50, 6, 7, 8, 9, 10, 12, 13, 14, 16, \
		55, 7, 8, 9, 10, 12, 13, 15, 17, 19, \
		60, 9, 10, 11, 12, 14, 15, 17, 19, 22, \
		65, 10,  11, 13, 14, 16, 18, 20, 23, 26, \
		70, 12, 13, 15, 17, 19, 21, 24, 27, 30, \
		75, 14, 16, 18, 20, 22, 25, 28, 31, 35, \
		80, 16, 18, 21, 23, 26, 29, 33, 37, 42, \
		85, 19, 22, 24, 27, 31, 34, 39, 43, 49, \
		90, 23, 25, 29, 32, 36, 40, 45, 51, 57, \
		95, 27, 30, 33, 38, 42, 47, 53, 60, 67, \
		100, 31, 35, 39, 44, 50, 56, 63, 70, 79, \
		105, 37, 41, 46, 52, 58, 66, 74, 83, 93}

int ca53_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	CA53_TABLE_0, CA53_TABLE_1, CA53_TABLE_2,
};

struct spower_raw_t {
	int vsize;
	int tsize;
	int table_size;
	int *table[];
};

struct spower_raw_t ca53_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&ca53_data[0], (int *)&ca53_data[1],
		  (int *)&ca53_data[2]},
};

struct vrow_t {
	int mV[VSIZE];
};

struct trow_t {
	int deg;
	int mA[VSIZE];
};


struct sptbl_t {
	int vsize;
	int tsize;
	/* array[VSIZE + TSIZE + (VSIZE*TSIZE)]; */
	int *data;
	/* pointer to voltage row of data */
	struct vrow_t *vrow;
	/* pointer to temperature row of data */
	struct trow_t *trow;
};

#define trow(tab, ti)		((tab)->trow[ti])
#define mA(tab, vi, ti)	((tab)->trow[ti].mA[vi])
#define mV(tab, vi)		((tab)->vrow[0].mV[vi])
#define deg(tab, ti)		((tab)->trow[ti].deg)
#define vsize(tab)		((tab)->vsize)
#define tsize(tab)		((tab)->tsize)
#define tab_validate(tab)	((tab)->data != NULL)

static inline void spower_tab_construct
	(struct sptbl_t (*tab)[], struct spower_raw_t *raw)
{
	int i;
	struct sptbl_t *ptab = (struct sptbl_t *)tab;

	for (i = 0; i < raw->table_size; i++) {
		ptab->vsize = raw->vsize;
		ptab->tsize = raw->tsize;
		ptab->data = raw->table[i];
		ptab->vrow = (struct vrow_t *)ptab->data;
		ptab->trow = (struct trow_t *)(ptab->data + ptab->vsize);
		ptab++;
	}
}

#define MAX_TABLE_SIZE 5

/**
 * @argument
 * dev: the enum of MT_SPOWER_xxx
 * voltage: the operating voltage
 * degree: the Tj
 * @return
 *  -1, means sptab is not yet ready.
 *  other value: the mW of leakage value.
 **/
extern int mt_spower_get_leakage(int dev, int voltage, int degree);

#endif


