// SPDX-License-Identifier: GPL-2.0
/*
 * Support for a block device hacked into shared memory for David's RV64 design.
 * Based on n64cart.c.
 *
 * Copyright (c) 2025 David Sheffield
 * Copyright (c) 2021 Lauri Kasanen
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

enum {
	PI_DRAM_REG = 0,
	PI_CART_REG,
	PI_READ_REG,
	PI_WRITE_REG,
	PI_STATUS_REG,
};

#define PI_STATUS_DMA_BUSY	(1 << 0)
#define PI_STATUS_IO_BUSY	(1 << 1)

#define CART_DOMAIN		0x10000000
#define CART_MAX		0x1FFFFFFF

#define MIN_ALIGNMENT		8

static u64 start = 1UL<<30, size = 1UL<<24;

/*
 * Process a single bvec of a bio.
 */
static bool rv64block_do_bvec(struct device *dev, struct bio_vec *bv, u32 pos)
{
	dma_addr_t dma_addr;
	const u32 bstart = pos + start;

	/* Alignment check */
	WARN_ON_ONCE((bv->bv_offset & (MIN_ALIGNMENT - 1)) ||
		     (bv->bv_len & (MIN_ALIGNMENT - 1)));

	return false;
#if 0
	dma_addr = dma_map_bvec(dev, bv, DMA_FROM_DEVICE, 0);
	if (dma_mapping_error(dev, dma_addr))
		return false;

	rv64block_wait_dma();

	rv64block_write_reg(PI_DRAM_REG, dma_addr);
	rv64block_write_reg(PI_CART_REG, (bstart | CART_DOMAIN) & CART_MAX);
	rv64block_write_reg(PI_WRITE_REG, bv->bv_len - 1);

	rv64block_wait_dma();

	dma_unmap_page(dev, dma_addr, bv->bv_len, DMA_FROM_DEVICE);
	return true;
#endif
}

static void rv64block_submit_bio(struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	struct device *dev = bio->bi_bdev->bd_disk->private_data;
	u32 pos = bio->bi_iter.bi_sector << SECTOR_SHIFT;

	bio_for_each_segment(bvec, bio, iter) {
		if (!rv64block_do_bvec(dev, &bvec, pos)) {
			bio_io_error(bio);
			return;
		}
		pos += bvec.bv_len;
	}

	bio_endio(bio);
}

static const struct block_device_operations rv64block_fops = {
	.owner		= THIS_MODULE,
	.submit_bio	= rv64block_submit_bio,
};


static int __init rv64block_init(void) {
  return -ENODEV;  
#if 0
  printk(KERN_INFO "HERE %s:%d\n", __PRETTY_FUNCTION__, __LINE__);
  
  struct queue_limits lim = {
    .physical_block_size	= 4096,
    .logical_block_size	= 4096,
  };
  struct gendisk *disk;
  int err = -ENOMEM;
  
  if (!start || !size) {
    pr_err("start or size not specified\n");
    return -ENODEV;
  }
  
  if (size & 4095) {
    pr_err("size must be a multiple of 4K\n");
    return -ENODEV;
  }
  
  disk = blk_alloc_disk(&lim, NUMA_NO_NODE);
  if (IS_ERR(disk)) {
    err = PTR_ERR(disk);
    goto out;
  }
  
  disk->first_minor = 0;
  disk->flags = GENHD_FL_NO_PART;
  disk->fops = &rv64block_fops;
  disk->private_data = NULL;
  strcpy(disk->disk_name, "rv64block");
  
  set_capacity(disk, size >> SECTOR_SHIFT);
  set_disk_ro(disk, 1);
  
  err = add_disk(disk);
  if (err)
    goto out_cleanup_disk;
  
  pr_info("rv64block: %lu kb disk\n", size / 1024);
  
  return 0;

out_cleanup_disk:
	put_disk(disk);
out:
	return err;
#endif
}

module_init(rv64block_init);

MODULE_AUTHOR("David Sheffield (sheffield.david@gmail.com)");
MODULE_DESCRIPTION("Driver for the RV64 block device");
MODULE_LICENSE("GPL");
