/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#ifndef __NANDX_OS_H__
#define __NANDX_OS_H__

#include <linux/io.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/timekeeping.h>
#include <linux/dma-mapping.h>
#include <linux/compiler-gcc.h>

#define NANDX_BULK_IO_USE_DRAM 0

#define nandx_event_create()     NULL
#define nandx_event_destroy(event)
#define nandx_event_complete(event)
#define nandx_event_init(event)
#define nandx_event_wait_complete(event, timeout)        true

#define nandx_irq_register(dev, irq, irq_handler, name, data) \
	devm_request_irq(dev, irq, (irq_handler_t)irq_handler, 0, name, data)

static inline void *mem_alloc(u32 count, u32 size)
{
	return kmalloc(count * size, GFP_KERNEL | __GFP_ZERO);
}

static inline void mem_free(void *mem)
{
	kfree(mem);
}

static inline u64 get_current_time_us(void)
{
	return ktime_get_ns() >> 10;
}

static inline u32 nandx_dma_map(void *dev, void *buf, u64 len,
				enum nand_dma_operation op)
{
	return dma_map_single(dev, buf, len,
			      (op == NDMA_FROM_DEV) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
}

static inline void nandx_dma_unmap(void *dev, void *buf, void *addr,
				   u64 len,
				   enum nand_dma_operation op)
{
	dma_unmap_single(dev, (dma_addr_t)addr, len,
			 (op == NDMA_FROM_DEV) ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
}

#endif /* __NANDX_OS_H__ */
