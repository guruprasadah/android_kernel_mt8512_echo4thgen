/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#ifdef CONFIG_ROOTFS_CHECK
#include "../../init/rootfs_check/rootfs_check.h"
#endif

#ifdef CONFIG_MTD_GPT_BACKUP
#define MAX_BOOT_COPIES	4
#else
#define MAX_BOOT_COPIES	2
#endif

/* GPT Signature should be 0x5452415020494645 */
#define GPT_SIGNATURE_1            0x54524150
#define GPT_SIGNATURE_2            0x20494645

/* GPT Offsets */
#define HEADER_SIZE_OFFSET         12
#define HEADER_CRC_OFFSET          16
#define PRIMARY_HEADER_OFFSET      24
#define BACKUP_HEADER_OFFSET       32
#define FIRST_USABLE_LBA_OFFSET    40
#define LAST_USABLE_LBA_OFFSET     48
#define PARTITION_ENTRIES_OFFSET   72
#define PARTITION_COUNT_OFFSET     80
#define PENTRY_SIZE_OFFSET         84
#define PARTITION_CRC_OFFSET       88

#define ENTRY_SIZE                 0x80

#define UNIQUE_GUID_OFFSET         16
#define FIRST_LBA_OFFSET           32
#define LAST_LBA_OFFSET            40
#define ATTRIBUTE_FLAG_OFFSET      48
#define PARTITION_NAME_OFFSET      56

#define MAX_GPT_NAME_SIZE          72
#define PARTITION_TYPE_GUID_SIZE   16
#define UNIQUE_PARTITION_GUID_SIZE 16
#define NUM_PARTITIONS             128

#define GET_LWORD_FROM_BYTE(x)     (*(unsigned int *)(x))
#define GET_LLWORD_FROM_BYTE(x)    (*(unsigned long long *)(x))
#define GET_LONG(x)                (*(uint32_t *)(x))
#define PUT_LONG(x, y)             (*((uint32_t *)(x)) = y)
#define PUT_LONG_LONG(x, y)        (*((unsigned long long *)(x)) = y)
#define ROUNDUP(x, y)              ((((x)+(y)-1)/(y))*(y))

struct chs {
	uint8_t c;
	uint8_t h;
	uint8_t s;
};

struct mbr_part {
	uint8_t status;
	struct chs start;
	uint8_t type;
	struct chs end;
	uint32_t lba_start;
	uint32_t lba_length;
};

struct gpt_header {
	uint64_t first_usable_lba;
	uint64_t backup_header_lba;
	uint32_t partition_entry_size;
	uint32_t header_size;
	uint32_t max_partition_count;
};

static int validate_mbr_partition(struct mtd_info *master, const struct mbr_part *part)
{
	uint32_t lba_size;
	uint32_t tmp_lba_start;
	uint32_t tmp_lba_length;
	uint64_t tmp_size;
	uint32_t tmp_ret;

#ifdef CONFIG_MTD_GPT_FIXED_LBS
	lba_size = 4096;
#else
	if (mtd_type_is_nand(master))
		lba_size = master->writesize;
	else
		lba_size = 512;
#endif

	/* check for invalid types */
	if (part->type == 0)
		return -1;
	/* check for invalid status */
	if (part->status != 0x80 && part->status != 0x00)
		return -1;

	/* make sure the range fits within the device */
	tmp_lba_start = part->lba_start;
	tmp_size = master->size;
	tmp_ret = do_div(tmp_size, lba_size);
	if (tmp_lba_start >= tmp_size)
		return -1;

	tmp_size = master->size;
	tmp_lba_length = part->lba_length;
	tmp_ret = do_div(tmp_size, lba_size);
	if ((tmp_lba_start + tmp_lba_length) > tmp_size)
		return -1;

	return 0;
}

/*
 * Parse the gpt header and get the required header fields
 * Return 0 on valid signature
 */
static int partition_parse_gpt_header(unsigned char *buffer, struct gpt_header *header)
{
	/* Check GPT Signature */
	if (((uint32_t *) buffer)[0] != GPT_SIGNATURE_2 ||
	    ((uint32_t *) buffer)[1] != GPT_SIGNATURE_1)
		return 1;

	header->header_size = GET_LWORD_FROM_BYTE(&buffer[HEADER_SIZE_OFFSET]);
	header->backup_header_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[BACKUP_HEADER_OFFSET]);
	header->first_usable_lba =
	    GET_LLWORD_FROM_BYTE(&buffer[FIRST_USABLE_LBA_OFFSET]);
	header->max_partition_count =
	    GET_LWORD_FROM_BYTE(&buffer[PARTITION_COUNT_OFFSET]);
	header->partition_entry_size =
	    GET_LWORD_FROM_BYTE(&buffer[PENTRY_SIZE_OFFSET]);

	return 0;
}

static int gpt_add_part(struct mtd_info *master, struct gpt_header *gpthdr,
			uint8_t *buf, u32 lba_size, struct mtd_partition *part,
			uint32_t part_index, int *boot_flag)
{
	unsigned char type_guid[PARTITION_TYPE_GUID_SIZE];
	unsigned char type_guid_next[PARTITION_TYPE_GUID_SIZE];
	unsigned char *name;
	unsigned char UTF16_name[MAX_GPT_NAME_SIZE];
	uint64_t first_lba, last_lba, size, part_attrs;
	unsigned int n;
	uint32_t part_entry_cnt;
	uint32_t entry_size = gpthdr->partition_entry_size;

	memcpy(&type_guid, buf, PARTITION_TYPE_GUID_SIZE);

	part_entry_cnt = lba_size / ENTRY_SIZE;
	if (type_guid[0] == 0 && type_guid[1] == 0)
		return -1;

	memcpy(&type_guid_next,	&buf[entry_size], PARTITION_TYPE_GUID_SIZE);

	part_attrs = GET_LLWORD_FROM_BYTE(&buf[ATTRIBUTE_FLAG_OFFSET]);

	first_lba = GET_LLWORD_FROM_BYTE(&buf[FIRST_LBA_OFFSET]);
	last_lba = GET_LLWORD_FROM_BYTE(&buf[LAST_LBA_OFFSET]);
	/* invalid partition if end addr is less than or equals to start addr */
	if (last_lba <= first_lba) {
		dev_err(&master->dev, "invalid partition length.\n");
		return -1;
	}
#ifdef CONFIG_MTD_GPT_FIXED_LBS
	/* Last partition entry */
	if (type_guid_next[0] == 0 && type_guid_next[1] == 0)
		last_lba = master->size / lba_size - 513;

	/* Bootloader partition */
	if (part_attrs == 4) {
#ifdef CONFIG_MTD_GPT_BACKUP
		first_lba -= 4;
		if (*boot_flag)
			return 0;
		*boot_flag = 1;
#endif
		first_lba /= (lba_size / master->writesize);
	}
#endif
	if (part_attrs == 4) {
#ifdef CONFIG_MTD_GPT_BACKUP
		first_lba -= 4;
		*boot_flag = 1;
#endif
	}
	size = last_lba - first_lba + 1;

	memset(&UTF16_name, 0x00, MAX_GPT_NAME_SIZE);
	memcpy(UTF16_name, &buf[PARTITION_NAME_OFFSET], MAX_GPT_NAME_SIZE);

	/*
	 * Currently partition names in *.xml are UTF-8 and lowercase
	 * Only supporting english for now so removing 2nd byte of UTF-16
	 */
	name = kzalloc(MAX_GPT_NAME_SIZE, GFP_KERNEL);
	for (n = 0; n < MAX_GPT_NAME_SIZE / 2; n++)
		name[n] = UTF16_name[n * 2];

	dev_dbg(&master->dev, "partition(%s) first_lba(%lld), last_lba(%lld), size(%lld)\n",
		name, first_lba, last_lba, size);

	part->name = name;
	part->offset = first_lba * lba_size;
	part->mask_flags = 0;
	part->size = (last_lba - first_lba + 1) * lba_size;

	#ifdef CONFIG_ROOTFS_CHECK
	if (part_index == get_rootfs_mtd_num()) {
		pr_notice("[rootfs_check]set mtd num %d as read-only\n",
			part_index);
		part->mask_flags = MTD_WRITEABLE;
	}
	#endif

	return 0;
}

static int gpt_read_entries(struct mtd_info *master, struct gpt_header *gpthdr,
			uint8_t *buf, u32 lba_size, uint64_t gpt_offset,
			struct mtd_partition *parts, uint32_t *curr_part)
{
	uint32_t part_entry_cnt, part_lba_cnt;
	uint64_t partition_0, part_addr = 0;
	size_t bytes_read = 0;
	unsigned int i, j;
	int boot_flag = 0;
	int err = 0;

	part_entry_cnt = lba_size / ENTRY_SIZE;
	part_lba_cnt = (ROUNDUP(gpthdr->max_partition_count, part_entry_cnt)) /
			part_entry_cnt;
	partition_0 = GET_LLWORD_FROM_BYTE(&buf[PARTITION_ENTRIES_OFFSET]);
	part_addr = gpt_offset + (partition_0 * lba_size);

	/* Read GPT Entries */
	for (i = 0; i < part_lba_cnt; i++) {
		err = mtd_read(master, part_addr + (i * lba_size), lba_size,
				&bytes_read, buf);
		if (err < 0)
			break;

		for (j = 0; j < part_entry_cnt; j++) {
			int ret;

			ret = gpt_add_part(master, gpthdr,
				&buf[j * gpthdr->partition_entry_size],
				lba_size, &parts[*curr_part], *curr_part,
				&boot_flag);
			if (ret) {
				i = part_lba_cnt * part_entry_cnt;
				break;
			}
			*curr_part = *curr_part + 1;

			dev_dbg(&master->dev,
				"gpt there are <%d> parititons.\n",
				*curr_part);
		}
	}

	if (err < 0)
		dev_err(&master->dev, "GPT: read failed reading partition entries.\n");

	return err;
}

int gpt_parse(struct mtd_info *master,
			     const struct mtd_partition **pparts,
			     struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts = NULL;
	uint32_t curr_part = 0;
	int err;
	uint8_t *buf;
	size_t bytes_read = 0;
	unsigned int i, k;
	int gpt_partitions_exist = 0;
	struct mbr_part part[4];
	struct gpt_header gpthdr = {0, 0, 0, 0, 0};
	uint64_t offset = 0;
	u32 lba_size;
	int copy = 0, copy_cnt = MAX_BOOT_COPIES;

	dev_dbg(&master->dev, "GPT: enter gpt parser...\n");

#ifdef CONFIG_MTD_GPT_FIXED_LBS
	lba_size = 4096;
#else
	if (mtd_type_is_nand(master))
		lba_size = master->writesize;
	else
		lba_size = 512;
#endif

	buf = kzalloc(lba_size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	for (copy = 0; copy < copy_cnt; copy++) {
#ifdef CONFIG_MTD_GPT_BACKUP
		offset += (copy * 256) * master->writesize;
#endif
		err = mtd_read(master, offset, lba_size, &bytes_read, buf);
		if (err < 0)
			continue;

		for (k = 0; k < lba_size; k += 8) {
			dev_dbg(&master->dev, "+%d: %x %x %x %x  %x %x %x %x\n",
				k, buf[k], buf[k+1], buf[k+2], buf[k+3],
				buf[k+4], buf[k+5], buf[k+6], buf[k+7]);
		}

		/* look for the aa55 tag */
		if (buf[510] != 0x55 || buf[511] != 0xaa) {
			dev_err(&master->dev,
				"GPT: not find aa55 @ 510,511 copy:%d\n",
				copy);
			continue;
		}

		/* see if a partition table makes sense here */
		memcpy(part, buf + 446, sizeof(part));

		/* validate each of the partition entries */
		for (i = 0; i < 4; i++) {
			if (validate_mbr_partition(master, &part[i]) >= 0) {
				/*
				 * Type 0xEE indicates end of MBR and
				 * GPT partitions exist
				 */
				if (part[i].type == 0xee) {
					gpt_partitions_exist = 1;
					break;
				}
			}
		}

		if (gpt_partitions_exist == 0) {
			dev_err(&master->dev, "GPT: not find GPT\n");
			continue;
		}

		err = mtd_read(master, lba_size + offset, lba_size,
				&bytes_read, buf);
		if (err < 0)
			continue;

		err = partition_parse_gpt_header(buf, &gpthdr);
		if (err) {
#ifndef CONFIG_MTD_GPT_BACKUP
			dev_warn(&master->dev, "GPT: Read GPT header fail, try to check the backup gpt.\n");
			err = mtd_read(master, master->size - lba_size,
					lba_size, &bytes_read, buf);
			if (err < 0) {
				dev_err(&master->dev, "GPT: Could not read backup gpt.\n");
				goto freebuf;
			}

			err = partition_parse_gpt_header(buf, &gpthdr);
			if (err) {
				dev_err(&master->dev, "GPT: Primary and backup signatures invalid.\n");
				goto freebuf;
			}
#endif
		}

		if (parts == NULL) {
			parts = kcalloc(gpthdr.max_partition_count,
					sizeof(struct mtd_partition),
					GFP_KERNEL);
			if (parts == NULL)
				goto freebuf;
		}

		err = gpt_read_entries(master, &gpthdr, buf, lba_size, offset,
					parts, &curr_part);
		if (err >= 0) {
			pr_notice("Found GPT @%d total:%d\n", copy, copy_cnt);
			break;
		}
	}

	if (err < 0)
		goto freeparts;

	*pparts = parts;
	kfree(buf);
	return curr_part;

freeparts:
	kfree(parts);
freebuf:
	kfree(buf);
	return 0;
};

static struct mtd_part_parser gpt_parser = {
	.owner = THIS_MODULE,
	.parse_fn = gpt_parse,
	.name = "gptpart",
};

static int __init gptpart_init(void)
{
	register_mtd_parser(&gpt_parser);
	return 0;
}

static void __exit gptpart_exit(void)
{
	deregister_mtd_parser(&gpt_parser);
}

module_init(gptpart_init);
module_exit(gptpart_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPT partitioning for flash memories");
