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


static u8* disk_image = NULL;
static u64 start = 1UL<<30, size = 1UL<<24;

/*
 * Process a single bvec of a bio.
 */
static bool rv64block_do_bvec(struct device *dev, struct bio_vec *bv, u32 pos, int wr)
{
  char *buffer = bvec_kmap_local(bv);
  printk(KERN_INFO "pos = %u, wr = %d,offset = %u, len = %u", pos, wr, bv->bv_offset, bv->bv_len);
  if(wr) {
    memcpy(disk_image+pos, buffer, bv->bv_len);
  }
  else {
    memcpy(buffer, disk_image+pos, bv->bv_len);
  }
  kunmap_local(buffer);
  
  return true;
}

static void rv64block_submit_bio(struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	struct device *dev = bio->bi_bdev->bd_disk->private_data;
	u32 pos = bio->bi_iter.bi_sector << SECTOR_SHIFT;
	
	bio_for_each_segment(bvec, bio, iter) {
	  if (!rv64block_do_bvec(dev, &bvec, pos, bio_data_dir(bio) == WRITE)) {
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
  //return -ENODEV;  
#if 1
  struct queue_limits lim = {
    .physical_block_size= 512,
    .logical_block_size	= 512,
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
  
  err = add_disk(disk);
  if (err)
    goto out_cleanup_disk;
  
  pr_info("rv64block: %lu kb disk\n", size / 1024);
  disk_image = kzalloc(size, GFP_KERNEL);  
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
