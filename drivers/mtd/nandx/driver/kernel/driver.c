/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include "nandx_core.h"
#include "nandx_util.h"
#include "bbt.h"

typedef int (*func_nandx_operation)(u8 *, u8 *, u64, size_t);

struct nandx_clk {
	struct clk *nfi_clk;
	struct clk *nfi_hclk;
	struct clk *nfi2x_clk_sel;
	struct clk *nfi2x_clk_parent;
	struct clk *ecc_clk;
	struct clk *snfi_clk_sel;
	struct clk *snfi_parent_52m;
	struct clk *snfi_parent_91m;
};

struct nandx_nfc {
	struct nandx_info info;
	struct nandx_clk clk;
	struct nfi_resource *res;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_drive_high;

	struct mutex lock;
};

#ifdef CONFIG_MTD_NAND_MTK_WORN_BAD
static struct nandx_worn_bad_test g_worn_bad_test = {0};
#endif

static void nandx_get_device(struct mtd_info *mtd)
{
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;

	pm_runtime_get_sync(mtd->dev.parent);

	mutex_lock(&nfc->lock);
}

static void nandx_release_device(struct mtd_info *mtd)
{
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;

	mutex_unlock(&nfc->lock);

	pm_runtime_mark_last_busy(mtd->dev.parent);
	pm_runtime_put_autosuspend(mtd->dev.parent);
}

static int nandx_enable_clk(struct nandx_clk *clk)
{
	int ret;

	ret = clk_prepare_enable(clk->nfi_clk);
	if (ret) {
		pr_info("failed to enable nfi clk\n");
		return ret;
	}

	ret = clk_prepare_enable(clk->nfi_hclk);
	if (ret) {
		pr_info("failed to enable nfi hclk\n");
		goto disable_nfi_clk;
	}

	ret = clk_prepare_enable(clk->ecc_clk);
	if (ret) {
		pr_info("failed to enable ecc clk\n");
		goto disable_nfi_hlk;
	}

	ret = clk_prepare_enable(clk->nfi2x_clk_sel);
	if (ret) {
		pr_info("failed to enable nfi2x clk sel\n");
		goto disable_ecc_clk;
	}

	ret = clk_prepare_enable(clk->snfi_clk_sel);
	if (ret) {
		pr_info("failed to enable snfi clk sel\n");
		goto disable_nfi2x_clk_sel;
	}

	return 0;

disable_nfi2x_clk_sel:
	clk_disable_unprepare(clk->nfi2x_clk_sel);
disable_ecc_clk:
	clk_disable_unprepare(clk->ecc_clk);
disable_nfi_hlk:
	clk_disable_unprepare(clk->nfi_hclk);
disable_nfi_clk:
	clk_disable_unprepare(clk->nfi_clk);

	return ret;
}

static void nandx_disable_clk(struct nandx_clk *clk)
{
	clk_disable_unprepare(clk->ecc_clk);
	clk_disable_unprepare(clk->nfi_clk);
	clk_disable_unprepare(clk->nfi_hclk);
}

#ifdef CONFIG_MTD_NAND_MTK_WORN_BAD
/**
 * mtk_nfc_check_worn_bad_test - check the page addr is simulate worn
 *
 * This applies to check the page addr is simulate worn out or not.
 */
int mtk_nfc_check_worn_bad_test(void)
{
	if (g_worn_bad_test.sim_op_mode == WRITE_MODE ||
				g_worn_bad_test.sim_op_mode == ERASE_MODE)
		return g_worn_bad_test.sim_worn_page_addr;
	else
		return -1;
}
#else
int mtk_nfc_check_worn_bad_test(void)
{
	return -1;
}
#endif

#ifdef CONFIG_MTD_NAND_MTK_WORN_BAD
/**
 * mtk_nfc_set_worn_op - set worn out page and worn out mode
 * @mtd: MTD device structure
 * @ofs: offset in a partition.
 * @op_mode: worn out operation mode which is write mode or erase mode.
 *
 * This applies to simulate worn bad test for erase and program only.
 */
static int mtk_nfc_set_worn_op(struct mtd_info *mtd, loff_t ofs, u32 op_mode)
{
	struct mtd_info *master_mtd = mtd_get_part_master(mtd);
	struct nandx_nfc *nfc = (struct nandx_nfc *)master_mtd->priv;
	u32 page, page_shift;
	u64 offset;

	if (ofs < 0 || ofs >= mtd->size)
		return -EINVAL;

	page_shift = ffs((u32)nfc->info.page_size) - 1;
	offset = mtd_get_part_offset(mtd);
	page = (offset >> page_shift) + (ofs >> page_shift);

	g_worn_bad_test.sim_worn_page_addr = page;
	g_worn_bad_test.sim_op_mode = op_mode;

	return 0;
}

int mtk_nfc_set_write_worn_page(struct mtd_info *mtd, loff_t ofs)
{
	return mtk_nfc_set_worn_op(mtd, ofs, WRITE_MODE);
}
EXPORT_SYMBOL_GPL(mtk_nfc_set_write_worn_page);

int mtk_nfc_set_erase_worn_block(struct mtd_info *mtd, loff_t ofs)
{
	return mtk_nfc_set_worn_op(mtd, ofs, ERASE_MODE);
}
EXPORT_SYMBOL_GPL(mtk_nfc_set_erase_worn_block);
#endif

static int mtk_nfc_ooblayout_free(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *oob_region)
{
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;
	u32 eccsteps;

	eccsteps = div_down(mtd->writesize, mtd->ecc_step_size);

	if (section >= eccsteps)
		return -EINVAL;

	oob_region->length = nfc->info.fdm_reg_size - nfc->info.fdm_ecc_size;
	oob_region->offset = section * nfc->info.fdm_reg_size
		+ nfc->info.fdm_ecc_size;

	return 0;
}

static int mtk_nfc_ooblayout_ecc(struct mtd_info *mtd, int section,
				 struct mtd_oob_region *oob_region)
{
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;
	u32 eccsteps;

	if (section)
		return -EINVAL;

	eccsteps = div_down(mtd->writesize, mtd->ecc_step_size);
	oob_region->offset = nfc->info.fdm_reg_size * eccsteps;
	oob_region->length = mtd->oobsize - oob_region->offset;

	return 0;
}

static const struct mtd_ooblayout_ops mtk_nfc_ooblayout_ops = {
	.free = mtk_nfc_ooblayout_free,
	.ecc = mtk_nfc_ooblayout_ecc,
};

struct nfc_compatible {
	enum mtk_ic_version ic_ver;

	u32 clock_1x;
	u32 *clock_2x;
	int clock_2x_num;

	int min_oob_req;
};

static const struct nfc_compatible nfc_compats_mt8512 = {
	.ic_ver = NANDX_MT8512,
	.clock_1x = 26000000,
	.clock_2x = NULL,
	.clock_2x_num = 8,
	.min_oob_req = 1,
};

static const struct of_device_id ic_of_match[] = {
	{.compatible = "mediatek,mt8512-nfc", .data = &nfc_compats_mt8512},
	{}
};

static const char * const part_types[] = {"gptpart", "ofpart", NULL};

static int nand_operation(struct mtd_info *mtd, loff_t addr, size_t len,
	      size_t *retlen, uint8_t *data, uint8_t *oob, bool read)
{
	struct nandx_split64 split = {0};
	func_nandx_operation operation;
	struct chip_status status;
	u64 block_oobs, val, align;
	uint8_t *databuf, *oobbuf;
	struct nandx_nfc *nfc;
	bool readoob;
	int ret = 0, ret_min = 0, ret_max = 0;

	nfc = (struct nandx_nfc *)mtd->priv;
	databuf = data;
	oobbuf = oob;

	readoob = data ? false : true;
	block_oobs = div_up(mtd->erasesize, mtd->writesize) * mtd->oobavail;
	align = readoob ? block_oobs : mtd->erasesize;

	operation = read ? nandx_read : nandx_write;

	nandx_split(&split, addr, len, val, align);

	if (split.head_len) {
		ret = operation((u8 *) databuf, oobbuf, addr, split.head_len);
		if (ret < 0) {
			pr_err("%s %d: read %d, ret %d\n", __func__,
			       __LINE__, read, ret);
			if (!read) {
				if (ret == -ENANDWRITE)
					ret = -EIO;

				return ret;
                        }

			if (read && (ret == -ENANDREAD))
				ret = -EBADMSG;
		}

		if (read) {
			nandx_ioctl(CHIP_CTRL_GET_STATUS, &status);
			mtd->ecc_stats.corrected += status.corrected;
			mtd->ecc_stats.failed += status.failed;
		}

		ret_min = min_t(int, ret_min, ret);
		ret_max = max_t(int, ret_max, ret);

		if (databuf)
			databuf += split.head_len;

		if (oobbuf)
			oobbuf += split.head_len;

		addr += split.head_len;
		*retlen += split.head_len;
	}

	if (split.body_len) {
		while (div_up(split.body_len, align)) {
			ret = operation((u8 *) databuf, oobbuf, addr, align);
			if (ret < 0) {
				pr_err("%s %d: read %d, ret %d\n", __func__,
				       __LINE__, read, ret);
				if (!read) {
					if (ret == -ENANDWRITE)
						ret = -EIO;

					return ret;
	                        }

				if (read && (ret == -ENANDREAD))
					ret = -EBADMSG;
	                }
	                ret_min = min_t(int, ret_min, ret);
			ret_max = max_t(int, ret_max, ret);

			if (databuf) {
				databuf += mtd->erasesize;
				split.body_len -= mtd->erasesize;
				*retlen += mtd->erasesize;
			}

			if (oobbuf) {
				oobbuf += block_oobs;
				split.body_len -= block_oobs;
				*retlen += block_oobs;
			}

			addr += mtd->erasesize;
		}

		if (read) {
			nandx_ioctl(CHIP_CTRL_GET_STATUS, &status);
			mtd->ecc_stats.corrected += status.corrected;
			mtd->ecc_stats.failed += status.failed;
		}
	}

	if (split.tail_len) {
		ret = operation((u8 *) databuf, oobbuf, addr, split.tail_len);
		if (ret < 0) {
			pr_err("%s %d: read %d, ret %d\n", __func__,
			       __LINE__, read, ret);
			if (!read) {
				if (ret == -ENANDWRITE)
					ret = -EIO;

				return ret;
                        }

			if (read && (ret == -ENANDREAD))
				ret = -EBADMSG;
		}

		if (read) {
			nandx_ioctl(CHIP_CTRL_GET_STATUS, &status);
			mtd->ecc_stats.corrected += status.corrected;
			mtd->ecc_stats.failed += status.failed;
		}

		ret_min = min_t(int, ret_min, ret);
		ret_max = max_t(int, ret_max, ret);
		*retlen += split.tail_len;
	}

	return ret_min < 0 ? ret_min : ret_max;
}

static int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
	      size_t *retlen, uint8_t *buf)
{
	int ret;

	nandx_get_device(mtd);
	ret = nand_operation(mtd, from, len, retlen, buf, NULL, true);
	if (ret >= mtd->bitflip_threshold)
		pr_err("read from: 0x%llx, len: %d, bitflips: %d exceed threshold %d\n",
			    from, len, ret, mtd->bitflip_threshold);

	nandx_release_device(mtd);

	return ret;
}

static int nand_write(struct mtd_info *mtd, loff_t to, size_t len,
	       size_t *retlen, const uint8_t *buf)
{
	int ret;

	nandx_get_device(mtd);
	ret = nand_operation(mtd, to, len, retlen, (uint8_t *)buf,
		NULL, false);
	nandx_release_device(mtd);
	return ret;
}

int nand_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	size_t retlen;
	int ret;

	nandx_get_device(mtd);
	ret = nand_operation(mtd, from, ops->ooblen, &retlen, NULL,
		ops->oobbuf, true);
	nandx_release_device(mtd);
	return ret;
}

int nand_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	size_t retlen;
	int ret;

	nandx_get_device(mtd);
	ret = nand_operation(mtd, to, ops->ooblen, &retlen, NULL,
		ops->oobbuf, false);
	nandx_release_device(mtd);
	return ret;
}

static int nand_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct nandx_nfc *nfc;
	u32 block_size;
	int ret = 0;

	nandx_get_device(mtd);

	nfc = (struct nandx_nfc *)mtd->priv;
	block_size = nfc->info.block_size;
	instr->state = MTD_ERASING;

	while (instr->len) {
		if (bbt_is_bad(&nfc->info, instr->addr)) {
			pr_info("block(0x%llx) is bad, not erase\n",
				instr->addr);
			instr->state = MTD_ERASE_FAILED;
			goto erase_exit;
		} else {
			ret = nandx_erase(instr->addr, block_size);
			if (ret < 0) {
				instr->state = MTD_ERASE_FAILED;
				goto erase_exit;
				pr_info("erase fail at blk %llu, ret:%d\n",
					instr->addr, ret);
			}
		}
		instr->addr += block_size;
		instr->len -= block_size;
	}

	instr->state = MTD_ERASE_DONE;

erase_exit:
	ret = instr->state == MTD_ERASE_DONE ? 0 : -EIO;
	/* Do mtd call back function */
	if (!ret)
		mtd_erase_callback(instr);

	nandx_release_device(mtd);

	return ret;
}

int nand_is_bad(struct mtd_info *mtd, loff_t ofs)
{
	struct nandx_nfc *nfc;
	int ret;

	nfc = (struct nandx_nfc *)mtd->priv;
	nandx_get_device(mtd);

	ret = bbt_is_bad(&nfc->info, ofs);
	nandx_release_device(mtd);

	return ret;
}

int nand_mark_bad(struct mtd_info *mtd, loff_t ofs)
{
	struct nandx_nfc *nfc;
	int ret;

	nfc = (struct nandx_nfc *)mtd->priv;
	nandx_get_device(mtd);
	pr_info("%s, %d\n", __func__, __LINE__);
	ret = bbt_mark_bad(&nfc->info, ofs);

	nandx_release_device(mtd);

	return ret;
}

static int nand_block_unmarkbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;
	struct erase_info einfo;
	int ret;

	/* If not bad, return success and do nothing */
	ret = nand_is_bad(mtd, ofs);
	if (!ret)
		return 0;

	/* Unmark bad in BBT */
	ret = nand_unmarkbad_bbt(&nfc->info, ofs);
	if (ret)
		return 0;

	/* Erase block to clear bad block mark in OOB area */
	memset(&einfo, 0, sizeof(einfo));
	einfo.mtd = mtd;
	einfo.addr = ofs;
	einfo.len = mtd->erasesize;
	ret = nand_erase(mtd, &einfo);

	/* If it's bad block, updata bbt*/
	if (ret) {
		bbt_mark_bad(&nfc->info, ofs);
		return ret;
	}

	if (!ret)
		mtd->ecc_stats.badblocks--;

	return ret;
}

void nand_sync(struct mtd_info *mtd)
{
	nandx_get_device(mtd);
	nandx_sync();
	nandx_release_device(mtd);
}

static struct mtd_info *mtd_info_create(struct platform_device *pdev,
		struct nandx_nfc *nfc)
{
	struct mtd_info *mtd;
	int ret;

	mtd = mem_alloc(1, sizeof(struct mtd_info));
	if (!mtd)
		return NULL;

	ret = nandx_ioctl(CORE_CTRL_NAND_INFO, &nfc->info);
	if (ret) {
		pr_info("fail to get nand info (%d)!\n", ret);
		mem_free(mtd);
		return NULL;
	}

	mtd->priv = nfc;
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;
	mtd->name = "MTK-Nand";
	mtd->writesize = nfc->info.page_size;
	mtd->erasesize = nfc->info.block_size;
	mtd->oobsize = nfc->info.oob_size;
	mtd->size = nfc->info.total_size;
	mtd->type = MTD_NANDFLASH;
	mtd->flags = MTD_CAP_NANDFLASH;
	mtd->_erase = nand_erase;
	mtd->_point = NULL;
	mtd->_unpoint = NULL;
	mtd->_read = nand_read;
	mtd->_write = nand_write;
	mtd->_read_oob = nand_read_oob;
	mtd->_write_oob = nand_write_oob;
	mtd->_sync = nand_sync;
	mtd->_lock = NULL;
	mtd->_unlock = NULL;
	mtd->_block_isbad = nand_is_bad;
	mtd->_block_markbad = nand_mark_bad;
	mtd->_block_unmarkbad = nand_block_unmarkbad;
	mtd->writebufsize = mtd->writesize;

	mtd_set_ooblayout(mtd, &mtk_nfc_ooblayout_ops);

	mtd->ecc_strength = nfc->info.ecc_strength;
	mtd->ecc_step_size = nfc->info.sector_size;

	if (!mtd->bitflip_threshold)
		mtd->bitflip_threshold = (mtd->ecc_strength * 3) / 4;

	pr_info("%s %d: mtd:bitflip_threshold %d, mtd:ecc_strength %d\n",
		__func__, __LINE__, mtd->bitflip_threshold, mtd->ecc_strength);

	return mtd;
}

static int get_platform_res(struct platform_device *pdev,
			struct nandx_nfc *nfc)
{
	void __iomem *nfi_base, *ecc_base;
	const struct of_device_id *of_id;
	struct nfc_compatible *compat;
	struct nfi_resource *res;
	u32 nfi_irq, ecc_irq;
	struct device *dev;
	int ret = 0;

	res = mem_alloc(1, sizeof(struct nfi_resource));
	if (!res)
		return -ENOMEM;

	nfc->res = res;
	dev = &pdev->dev;

	nfi_base = of_iomap(dev->of_node, 0);
	ecc_base = of_iomap(dev->of_node, 1);
	nfi_irq = irq_of_parse_and_map(dev->of_node, 0);
	ecc_irq = irq_of_parse_and_map(dev->of_node, 1);

	of_id = of_match_node(ic_of_match, pdev->dev.of_node);
	if (!of_id) {
		ret = -EINVAL;
		goto freeres;
	}
	compat = (struct nfc_compatible *)of_id->data;

	nfc->pinctrl = devm_pinctrl_get(dev);
	nfc->pins_drive_high = pinctrl_lookup_state(nfc->pinctrl,
						    "state_drive_high");

	nfc->clk.nfi_clk = devm_clk_get(dev, "nfi_clk");
	if (IS_ERR(nfc->clk.nfi_clk)) {
		pr_info("no nfi_clk\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.nfi_hclk = devm_clk_get(dev, "nfi_hclk");
	if (IS_ERR(nfc->clk.nfi_hclk)) {
		pr_info("no nfi_hclk\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.nfi2x_clk_sel = devm_clk_get(dev, "nfi2x_sel");
	if (IS_ERR(nfc->clk.nfi2x_clk_sel)) {
		pr_info("no nfi2x_clk_sel\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.nfi2x_clk_parent = devm_clk_get(dev, "nfi2x_clk_parent");
	if (IS_ERR(nfc->clk.nfi2x_clk_parent)) {
		pr_info("no nfi2x clk parent\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.ecc_clk = devm_clk_get(dev, "ecc_clk");
	if (IS_ERR(nfc->clk.ecc_clk)) {
		pr_info("no ecc_clk\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.snfi_clk_sel = devm_clk_get(dev, "spinfi_sel");
	if (IS_ERR(nfc->clk.snfi_clk_sel)) {
		pr_info("no snfi clk sel\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.snfi_parent_52m = devm_clk_get(dev, "spinfi_parent_52m");
	if (IS_ERR(nfc->clk.snfi_parent_52m)) {
		pr_info("no snfi_parent_52m\n");
		ret = -EINVAL;
		goto freeres;
	}

	nfc->clk.snfi_parent_91m = devm_clk_get(dev, "spinfi_parent_91m");
	if (IS_ERR(nfc->clk.snfi_parent_91m)) {
		pr_info("no snfi_parent_91m\n");
		ret = -EINVAL;
		goto freeres;
	}

	res->ic_ver = (enum mtk_ic_version)(compat->ic_ver);
	res->nfi_regs = (void *)nfi_base;
	res->nfi_irq_id = nfi_irq;
	res->ecc_regs = (void *)ecc_base;
	res->ecc_irq_id = ecc_irq;
	res->dev = dev;
	res->clock_1x = compat->clock_1x;
	res->clock_2x = compat->clock_2x;
	res->clock_2x_num = compat->clock_2x_num;

	return ret;

freeres:
	mem_free(res);

	return ret;
}

static ssize_t nand_ids_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;

	nfc = (struct nandx_nfc *)mtd->priv;

	return snprintf(buf, 8, "%llx\n", nfc->info.ids);
}
static DEVICE_ATTR(nand_ids, 0444, nand_ids_show, NULL);

static ssize_t bbt_goodblocks_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;

	nfc = (struct nandx_nfc *)mtd->priv;

	return snprintf(buf, PAGE_SIZE, "%x\n", nfc->info.bbt_goodblocks);
}
static DEVICE_ATTR(bbt_goodblocks, 0444, bbt_goodblocks_show, NULL);

static ssize_t bb_total_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;
	u32 bb_worn = 0, bb_factory = 0;

	nfc = (struct nandx_nfc *)mtd->priv;

	get_bad_block(&nfc->info, &bb_worn, &bb_factory, NULL);

	return snprintf(buf, PAGE_SIZE, "%d\n", bb_worn + bb_factory);
}
static DEVICE_ATTR(bb_total, 0444, bb_total_show, NULL);

static ssize_t bbt_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;

	nfc = (struct nandx_nfc *)mtd->priv;

	return get_bad_block(&nfc->info, NULL, NULL, buf);
}
static DEVICE_ATTR(bbtshow, 0444, bbt_show, NULL);

static ssize_t bb_worn_total_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;
	u32 bb_worn = 0;

	nfc = (struct nandx_nfc *)mtd->priv;

	get_bad_block(&nfc->info, &bb_worn, NULL, NULL);

	return snprintf(buf, PAGE_SIZE, "%d\n", bb_worn);
}
static DEVICE_ATTR(bb_worn_total, 0444, bb_worn_total_show, NULL);

static ssize_t bb_factory_total_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;
	u32 bb_factory = 0;

	nfc = (struct nandx_nfc *)mtd->priv;

	get_bad_block(&nfc->info, NULL, &bb_factory, NULL);

	return snprintf(buf, PAGE_SIZE, "%d\n", bb_factory);
}
static DEVICE_ATTR(bb_factory_total, 0444, bb_factory_total_show, NULL);

static struct attribute *mtk_nand_attrs[] = {
	&dev_attr_nand_ids.attr,
	&dev_attr_bbt_goodblocks.attr,
	&dev_attr_bb_total.attr,
	&dev_attr_bbtshow.attr,
	&dev_attr_bb_worn_total.attr,
	&dev_attr_bb_factory_total.attr,
	NULL,
};

static const struct attribute_group mtk_nand_attr_group = {
	.attrs = mtk_nand_attrs,
};

static int nand_probe(struct platform_device *pdev)
{
	struct mtd_info *mtd;
	struct nandx_nfc *nfc;
	int arg = 1;
	int ret;

	pr_err("nandx last modify time  2020-07-29-18-00!\n");

	nfc = mem_alloc(1, sizeof(struct nandx_nfc));
	if (!nfc)
		return -ENOMEM;

	ret = get_platform_res(pdev, nfc);
	if (ret)
		goto release_nfc;

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		pr_info("fail to set dma mask (%d)!\n", ret);
		goto release_res;
	}

	ret = nandx_enable_clk(&nfc->clk);
	if (ret)
		goto release_res;

	ret = nandx_init(nfc->res);
	if (ret) {
		pr_info("nandx init error (%d)!\n", ret);
		goto disable_clk;
	}

	if (!IS_ERR(nfc->pinctrl) && !IS_ERR(nfc->pins_drive_high)) {
		clk_set_parent(nfc->clk.snfi_clk_sel, nfc->clk.snfi_parent_91m);
		clk_set_parent(nfc->clk.nfi2x_clk_sel,
			       nfc->clk.nfi2x_clk_parent);
		pinctrl_select_state(nfc->pinctrl, nfc->pins_drive_high);

		arg = SPI_NAND_DEFAULT_DELAY_MODE;
		ret = nandx_ioctl(SNFI_CTRL_DELAY_MODE, &arg);
		if (ret)
			goto release_res;
	}

	arg = 1;
	nandx_ioctl(NFI_CTRL_DMA, &arg);
	nandx_ioctl(NFI_CTRL_ECC, &arg);
	nandx_ioctl(NFI_CTRL_BAD_MARK_SWAP, &arg);

	mtd = mtd_info_create(pdev, nfc);
	if (!mtd) {
		ret = -ENOMEM;
		goto disable_clk;
	}

	mutex_init(&nfc->lock);

	ret = scan_bbt(&nfc->info);
	if (ret) {
		pr_info("bbt init error (%d)!\n", ret);
		goto release_mtd;
	}

	platform_set_drvdata(pdev, mtd);
	mtd->priv = nfc;

	ret = mtd_device_parse_register(mtd, part_types, NULL, NULL, 0);
	if (ret) {
		pr_info("mtd parse partition error! ret:%d\n", ret);
		mtd_device_unregister(mtd);
		goto release_mtd;
	}

	get_bbt_goodblocks_num(&nfc->info);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/* Add device attribute groups */
	ret = sysfs_create_group(&pdev->dev.kobj, &mtk_nand_attr_group);
	if (ret) {
		pr_info("failed to create attribute group\n");
		goto release_mtd;
	}

	return 0;

release_mtd:
	mem_free(mtd);
disable_clk:
	pm_runtime_disable(&pdev->dev);
	nandx_disable_clk(&nfc->clk);
release_res:
	mem_free(nfc->res);
release_nfc:
	mem_free(nfc);

	pr_info("%s: probe err %d\n", __func__, ret);
	return ret;
}

static int nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct nandx_nfc *nfc;

	mtd_device_unregister(mtd);
	nfc = (struct nandx_nfc *)mtd->priv;
	nandx_disable_clk(&nfc->clk);

	pm_runtime_get_sync(&pdev->dev);

	mem_free(nfc->res);
	mem_free(nfc);
	mem_free(mtd);
	return 0;
}

#ifdef CONFIG_PM
static int nandx_runtime_suspend(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc = (struct nandx_nfc *)mtd->priv;
	int ret;

	ret = nandx_suspend();
	nandx_disable_clk(&nfc->clk);

	return ret;
}

static int nandx_runtime_resume(struct device *dev)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);
	struct nandx_nfc *nfc;
	int ret;

	nfc = (struct nandx_nfc *)mtd->priv;

	ret = nandx_enable_clk(&nfc->clk);
	if (ret)
		return ret;

	ret = nandx_resume();
	return ret;
}
#endif

static const struct dev_pm_ops nfc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(nandx_runtime_suspend, nandx_runtime_resume, NULL)
};

static struct platform_driver nand_driver = {
	.probe = nand_probe,
	.remove = nand_remove,
	.driver = {
		   .name = "mtk-nand",
		   .owner = THIS_MODULE,
		   .of_match_table = ic_of_match,
		   .pm = &nfc_dev_pm_ops,
		   },
};
MODULE_DEVICE_TABLE(of, mtk_nfc_id_table);

static int __init nand_init(void)
{
	return platform_driver_register(&nand_driver);
}

static void __exit nand_exit(void)
{
	platform_driver_unregister(&nand_driver);
}
module_init(nand_init);
module_exit(nand_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK Nand Flash Controller Driver");
MODULE_AUTHOR("MediaTek");
