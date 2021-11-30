/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#include "nandx_util.h"
#include "nandx_core.h"
#include "nand_chip.h"
#include "nand_device.h"
#include "nfi.h"
#include "nand_base.h"
#include "nfi/nfiecc.h"
#include "nfi/nfi_base.h"

static void  dump_nfi_regs(struct nfi *nfi)
{
	struct nfi_base *nb = nfi_to_base(nfi);
	u32 *addr =  nb->res.nfi_regs;
	u32 i;

	for (i = 0; i< 0x200; i++) {
		pr_debug("nfi reg addr:0x%x, value:0x%x\n", i * 4, readl(addr));
		addr++;
	}
}

static int nand_chip_read_page(struct nand_chip *chip,
			       struct nand_ops *ops,
			       int count)
{
	struct nand_base *nand = chip->nand;
	int i, ret_min = 0, ret_max = 0;
	int row, col, sectors;
	u8 *data, *oob;

	chip->status.corrected = 0;
	chip->status.failed = 0;

	for (i = 0; i < count; i++) {
		row = ops[i].row;
		col = ops[i].col;

		nand->addressing(nand, &row, &col);
		ops[i].status = nand->read_page(nand, row);
		if (ops[i].status < 0) {
			pr_err("%s %d: status %d, row %d, col %d\n",
				__func__, __LINE__, ops[i].status, row, col);
			dump_nfi_regs(nand->nfi);
			ret_min = min_t(int, ret_min, ops[i].status);
			continue;
		}

		data = ops[i].data;
		oob = ops[i].oob;
		sectors = ops[i].len / chip->sector_size;
		ops[i].status = nand->read_data(nand, row, col,
						sectors, data, oob);
		if (ops[i].status < 0) {
			if (ops[i].status == -ENANDREAD)
				chip->status.failed++;
			pr_err("%s %d: status %d, row %d, col %d, sec %d\n",
				__func__, __LINE__, ops[i].status,
				row, col, sectors);
			dump_nfi_regs(nand->nfi);
		} else if (ops[i].status > 0) {
			chip->status.corrected += ops[i].status;
		}

		ret_max = max_t(int, ret_max, ops[i].status);
		ret_min = min_t(int, ret_min, ops[i].status);
	}

	return ret_min < 0 ? ret_min : ret_max;
}

static int nand_chip_write_page(struct nand_chip *chip,
				struct nand_ops *ops,
				int count)
{
	struct nand_base *nand = chip->nand;
	struct nand_device *dev = nand->dev;
	int i, ret = 0;
	int row, col;
	u8 *data, *oob;

	for (i = 0; i < count; i++) {
		row = ops[i].row;
		col = ops[i].col;

		nand->addressing(nand, &row, &col);

		ops[i].status = nand->write_enable(nand);
		if (ops[i].status) {
			pr_err("Write Protect at %x!\n", row);
			ops[i].status = -ENANDWP;
			dump_nfi_regs(nand->nfi);
			return -ENANDWP;
		}

		data = ops[i].data;
		oob = ops[i].oob;
		ops[i].status = nand->program_data(nand, row, col, data, oob);
		if (ops[i].status < 0) {
			pr_err("%s %d: status %d, row %d, col %d\n",
				__func__, __LINE__, ops[i].status, row, col);
			dump_nfi_regs(nand->nfi);
			ret = ops[i].status;
			continue;
		}

		ops[i].status = nand->program_page(nand, row);
		if (ops[i].status < 0) {
			pr_err("%s %d: status %d, row %d, col %d\n",
				__func__, __LINE__, ops[i].status, row, col);
			dump_nfi_regs(nand->nfi);
			ret = ops[i].status;
			continue;
		}

		ops[i].status = nand->read_status(nand);
		if (ops[i].status & dev->status->program_fail) {
			ops[i].status = -ENANDWRITE;
			pr_err("%s %d: write fail, row %d, col %d\n",
				__func__, __LINE__, row, col);
		}

		ret = min_t(int, ret, ops[i].status);
	}

	return ret;
}

static int nand_chip_erase_block(struct nand_chip *chip,
				 struct nand_ops *ops,
				 int count)
{
	struct nand_base *nand = chip->nand;
	struct nand_device *dev = nand->dev;
	int i, ret = 0;
	int row, col;

	for (i = 0; i < count; i++) {
		row = ops[i].row;
		col = ops[i].col;

		nand->addressing(nand, &row, &col);

		ops[i].status = nand->write_enable(nand);
		if (ops[i].status) {
			pr_err("Write Protect at %x!\n", row);
			ops[i].status = -ENANDWP;
			return -ENANDWP;
		}

		ops[i].status = nand->erase_block(nand, row);
		if (ops[i].status < 0) {
			ret = ops[i].status;
			pr_err("%s %d:erase fail, status %d, row %d, col %d\n",
				__func__, __LINE__, ops[i].status, row, col);
			continue;
		}

		ops[i].status = nand->read_status(nand);
		if (ops[i].status & dev->status->erase_fail) {
			ops[i].status = -ENANDERASE;
			pr_err("%s %d: erase fail, row %d, col %d\n",
				__func__, __LINE__, row, col);
		}

		ret = min_t(int, ret, ops[i].status);
	}

	return ret;
}

/* read first bad mark on spare */
static int nand_chip_is_bad_block(struct nand_chip *chip,
				  struct nand_ops *ops,
				  int count)
{
	int i, ret, value;
	int status = 0;
	u8 *data;

	/* Disable ECC */
	value = 0;
	ret = chip->chip_ctrl(chip, NFI_CTRL_ECC, &value);
	if (ret)
		return ret;

	ret = chip->read_page(chip, ops, count);
	if (ret)
		return ret;

	for (i = 0; i < count; i++) {
		data = ops[i].data;

		if (data[chip->page_size] != 0xff) {
			ops[i].status = -ENANDBAD;
			status = -ENANDBAD;
		} else {
			ops[i].status = 0;
		}
	}

	/* Enable ECC */
	value = 1;
	ret = chip->chip_ctrl(chip, NFI_CTRL_ECC, &value);
	if (ret)
		return ret;

	return status;
}

static int nand_chip_ctrl(struct nand_chip *chip, int cmd, void *args)
{
	return -EOPNOTSUPP;
}

static int nand_chip_suspend(struct nand_chip *chip)
{
	struct nand_base *nand = chip->nand;

	return nand->nfi->suspend(nand->nfi);
}

static int nand_chip_resume(struct nand_chip *chip)
{
	struct nand_base *nand = chip->nand;

	return nand->nfi->resume(nand->nfi);
}

struct nand_chip *nand_chip_init(struct nfi_resource *res)
{
	struct nand_chip *chip;
	struct nand_base *nand;
	struct nfi *nfi;

	chip = mem_alloc(1, sizeof(struct nand_chip));
	if (!chip) {
		pr_info("nand chip alloc fail!\n");
		return NULL;
	}

	nfi = nfi_init(res);
	if (!nfi) {
		pr_info("nfi init fail!\n");
		goto nfi_err;
	}

	nand = nand_base_init(NULL, nfi);
	if (!nand) {
		pr_info("nand base init fail!\n");
		goto base_err;
	}

	chip->nand = (void *)nand;
	chip->read_page = nand_chip_read_page;
	chip->write_page = nand_chip_write_page;
	chip->erase_block = nand_chip_erase_block;
	chip->is_bad_block = nand_chip_is_bad_block;
	chip->chip_ctrl = nand_chip_ctrl;
	chip->suspend = nand_chip_suspend;
	chip->resume = nand_chip_resume;

	nand = nand_init(chip);
	if (!nand)
		goto nand_err;

	chip->nand = (void *)nand;
	chip->plane_num = nand->dev->plane_num;
	chip->block_num = nand_total_blocks(nand->dev);
	chip->block_size = nand->dev->block_size;
	chip->block_pages = nand_block_pages(nand->dev);
	chip->page_size = nand->dev->page_size;
	chip->oob_size = nfi->fdm_size * div_down(chip->page_size,
						  nfi->sector_size);
	chip->sector_size = nfi->sector_size;
	chip->sector_spare_size = nfi->sector_spare_size;
	chip->min_program_pages = nand->dev->min_program_pages;
	chip->ecc_strength = nfi->ecc_strength;
	chip->ecc_parity_size = nfi->ecc_parity_size;
	chip->fdm_ecc_size = nfi->fdm_ecc_size;
	chip->fdm_reg_size = nfi->fdm_size;
	chip->ids = nand->dev->id;

	return chip;

nand_err:
	mem_free(nand);
base_err:
	nfi_exit(nfi);
nfi_err:
	mem_free(chip);
	return NULL;
}

void nand_chip_exit(struct nand_chip *chip)
{
	nand_exit(chip->nand);
	mem_free(chip);
}
