/*
 * Ram backed block device driver.
 *
 * Copyright (C) 2007 Nick Piggin
 * Copyright (C) 2007 Novell Inc.
 *
 * Parts derived from drivers/block/rd.c, and drivers/block/loop.c, copyright
 * of their respective owners.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include "disk_wrapper_ioctl.h"
#include "bio_alias.h"

#define SECTOR_SHIFT        9
#define PAGE_SECTORS_SHIFT  (PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS        (1 << PAGE_SECTORS_SHIFT)
#define DEFAULT_COW_RD_SIZE 512000
#define DEVICE_NAME         "cow_brd"

/*
 * Each block ramdisk device has a radix_tree brd_pages of pages that stores
 * the pages containing the block device's contents. A brd page's ->index is
 * its offset in PAGE_SIZE units. This is similar to, but in no way connected
 * with, the kernel's pagecache or buffer cache (which sit above our block
 * device).
 */
struct brd_device {
  int   brd_number;
  struct brd_device *parent_brd;

  // Denotes whether or not a cow_ram is writable and snapshots are active.
  bool  is_writable;
  bool  is_snapshot;

  struct request_queue  *brd_queue;
  struct gendisk    *brd_disk;
  struct list_head  brd_list;

  /*
   * Backing store of pages and lock to protect it. This is the contents
   * of the block device.
   */
  spinlock_t    brd_lock;
  struct radix_tree_root  brd_pages;
};

/*
 * Look up and return a brd's page for a given sector.
 */
static DEFINE_MUTEX(brd_mutex);
static struct page *brd_lookup_page(struct brd_device *brd, sector_t sector)
{
  pgoff_t idx;
  struct page *page;

  /*
   * The page lifetime is protected by the fact that we have opened the
   * device node -- brd pages will never be deleted under us, so we
   * don't need any further locking or refcounting.
   *
   * This is strictly true for the radix-tree nodes as well (ie. we
   * don't actually need the rcu_read_lock()), however that is not a
   * documented feature of the radix-tree API so it is better to be
   * safe here (we don't have total exclusion from radix tree updates
   * here, only deletes).
   */
  rcu_read_lock();
  idx = sector >> PAGE_SECTORS_SHIFT; /* sector to page index */
  page = radix_tree_lookup(&brd->brd_pages, idx);
  rcu_read_unlock();

  BUG_ON(page && page->index != idx);

  return page;
}

/*
 * Look up and return a brd's page for a given sector.
 * If one does not exist, allocate an empty page, and insert that. Then
 * return it.
 */
static struct page *brd_insert_page(struct brd_device *brd, sector_t sector)
{
  pgoff_t idx;
  struct page *page;
  gfp_t gfp_flags;
  void *dst, *parent_src = NULL;
  struct page *parent_page = NULL;

  page = brd_lookup_page(brd, sector);
  if (page)
    return page;

  /*
   * Must use NOIO because we don't want to recurse back into the
   * block or filesystem layers from page reclaim.
   *
   * Cannot support XIP and highmem, because our ->direct_access
   * routine for XIP must return memory that is always addressable.
   * If XIP was reworked to use pfns and kmap throughout, this
   * restriction might be able to be lifted.
   */
  gfp_flags = GFP_NOIO | __GFP_ZERO;
#ifndef CONFIG_BLK_DEV_XIP
  gfp_flags |= __GFP_HIGHMEM;
#endif
  page = alloc_page(gfp_flags);
  if (!page)
    return NULL;

  if (radix_tree_preload(GFP_NOIO)) {
    __free_page(page);
    return NULL;
  }

  spin_lock(&brd->brd_lock);
  idx = sector >> PAGE_SECTORS_SHIFT;
  page->index = idx;
  if (radix_tree_insert(&brd->brd_pages, idx, page)) {
    __free_page(page);
    page = radix_tree_lookup(&brd->brd_pages, idx);
    BUG_ON(!page);
    BUG_ON(page->index != idx);
  }
  spin_unlock(&brd->brd_lock);

  radix_tree_preload_end();

  // Copy over the data in the parent's page if it exists.
  if (brd->parent_brd) {
    parent_page = brd_lookup_page(brd->parent_brd, sector);
    // This page may not have originally existed in the parent.
    if (parent_page) {
      dst = kmap_atomic(page);
      parent_src = kmap_atomic(parent_page);
      memcpy(dst, parent_src, PAGE_SIZE);
      kunmap_atomic(parent_src);
      kunmap_atomic(dst);
    }
  }

  return page;
}

static void brd_free_page(struct brd_device *brd, sector_t sector)
{
  struct page *page;
  pgoff_t idx;

  spin_lock(&brd->brd_lock);
  idx = sector >> PAGE_SECTORS_SHIFT;
  page = radix_tree_delete(&brd->brd_pages, idx);
  spin_unlock(&brd->brd_lock);
  if (page)
    __free_page(page);
}

static void brd_zero_page(struct brd_device *brd, sector_t sector)
{
  struct page *page;

  page = brd_lookup_page(brd, sector);
  if (page)
    clear_highpage(page);
}

/*
 * Free all backing store pages and radix tree. This must only be called when
 * there are no other users of the device.
 */
#define FREE_BATCH 16
static void brd_free_pages(struct brd_device *brd)
{
  unsigned long pos = 0;
  struct page *pages[FREE_BATCH];
  int nr_pages;

  do {
    int i;

    nr_pages = radix_tree_gang_lookup(&brd->brd_pages,
        (void **)pages, pos, FREE_BATCH);

    for (i = 0; i < nr_pages; i++) {
      void *ret;

      BUG_ON(pages[i]->index < pos);
      pos = pages[i]->index;
      ret = radix_tree_delete(&brd->brd_pages, pos);
      BUG_ON(!ret || ret != pages[i]);
      __free_page(pages[i]);
    }

    pos++;

    /*
     * This assumes radix_tree_gang_lookup always returns as
     * many pages as possible. If the radix-tree code changes,
     * so will this have to.
     */
  } while (nr_pages == FREE_BATCH);
}

/*
 * copy_to_brd_setup must be called before copy_to_brd. It may sleep.
 */
static int copy_to_brd_setup(struct brd_device *brd, sector_t sector, size_t n)
{
  unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
  size_t copy;

  copy = min_t(size_t, n, PAGE_SIZE - offset);
  if (!brd_insert_page(brd, sector))
    return -ENOMEM;
  if (copy < n) {
    sector += copy >> SECTOR_SHIFT;
    if (!brd_insert_page(brd, sector))
      return -ENOMEM;
  }
  return 0;
}

static void discard_from_brd(struct brd_device *brd,
      sector_t sector, size_t n)
{
  while (n >= PAGE_SIZE) {
    /*
     * Don't want to actually discard pages here because
     * re-allocating the pages can result in writeback
     * deadlocks under heavy load.
     */
    if (0)
      brd_free_page(brd, sector);
    else
      brd_zero_page(brd, sector);
    sector += PAGE_SIZE >> SECTOR_SHIFT;
    n -= PAGE_SIZE;
  }
}

/*
 * Copy n bytes from src to the brd starting at sector. Does not sleep.
 */
static void copy_to_brd(struct brd_device *brd, const void *src,
      sector_t sector, size_t n)
{
  struct page *page, *parent_page;
  void *dst, *parent_src;
  unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
  size_t copy;

  copy = min_t(size_t, n, PAGE_SIZE - offset);
  page = brd_lookup_page(brd, sector);
  BUG_ON(!page);

  dst = kmap_atomic(page);
  memcpy(dst + offset, src, copy);
  kunmap_atomic(dst);

  if (copy < n) {
    src += copy;
    sector += copy >> SECTOR_SHIFT;
    copy = n - copy;
    page = brd_lookup_page(brd, sector);
    BUG_ON(!page);

    dst = kmap_atomic(page);
    // Copy over the rest of the page from the parent brd if it exists.
    if (brd->parent_brd) {
      parent_page = brd_lookup_page(brd->parent_brd, sector);
      // This page may not have originally existed in the parent.
      if (parent_page) {
        parent_src = kmap_atomic(parent_page);
        memcpy(dst + copy, parent_src + copy, PAGE_SIZE - copy);
        kunmap_atomic(parent_src);
      }
    }

    memcpy(dst, src, copy);
    kunmap_atomic(dst);
  }
}

/*
 * Copy n bytes to dst from the brd starting at sector. Does not sleep.
 */
static void copy_from_brd(void *dst, struct brd_device *brd, sector_t sector,
    size_t n)
{
  struct page *page;
  void *src;
  unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
  size_t copy;

  copy = min_t(size_t, n, PAGE_SIZE - offset);
  page = brd_lookup_page(brd, sector);
  if (page) {
    // Present in the new radix tree so this page has been modified.
    src = kmap_atomic(page);
    memcpy(dst, src + offset, copy);
    kunmap_atomic(src);
  } else if (brd->parent_brd &&
      (page = brd_lookup_page(brd->parent_brd, sector))) {
    // Present in the old radix tree so this page has not been modified.
    src = kmap_atomic(page);
    memcpy(dst, src + offset, copy);
    kunmap_atomic(src);
  } else {
    // Page doesn't exist in either radix tree so it must never have been
    // written.
    memset(dst, 0, copy);
  }

  if (copy < n) {
    dst += copy;
    sector += copy >> SECTOR_SHIFT;
    copy = n - copy;
    page = brd_lookup_page(brd, sector);
    if (page) {
      // Present in the new radix tree so this page has been modified.
      src = kmap_atomic(page);
      memcpy(dst, src, copy);
      kunmap_atomic(src);
    } else if (brd->parent_brd &&
        (page = brd_lookup_page(brd->parent_brd, sector))) {
      // Present in the old radix tree so this page has not been modified.
      src = kmap_atomic(page);
      memcpy(dst, src + offset, copy);
      kunmap_atomic(src);
    } else {
      // Page doesn't exist in either radix tree so it must never have been
      // written.
      memset(dst, 0, copy);
    }
  }
}

/*
 * Process a single bvec of a bio.
 */
static int brd_do_bvec(struct brd_device *brd, struct page *page,
      unsigned int len, unsigned int off, int rw,
      sector_t sector)
{
  void *mem;
  int err = 0;

  if (rw != READ) {
    err = copy_to_brd_setup(brd, sector, len);
    if (err)
      goto out;
  }

  mem = kmap_atomic(page);
  if (rw == READ) {
    copy_from_brd(mem + off, brd, sector, len);
    flush_dcache_page(page);
  } else {
    flush_dcache_page(page);
    copy_to_brd(brd, mem + off, sector, len);
  }
  kunmap_atomic(mem);

out:
  return err;
}

static void brd_make_request(struct request_queue *q, struct bio *bio)
{
  struct block_device *bdev = bio->bi_bdev;
  struct brd_device *brd = bdev->bd_disk->private_data;
  int rw;
  sector_t sector;
  int err = -EIO;

  sector = bio->BI_SECTOR;
  if (bio_end_sector(bio) > get_capacity(bdev->bd_disk)) {
    printk(KERN_INFO DEVICE_NAME ": cow_brd%d past end of disk, EIO\n",
        brd->brd_number);
    goto out;
  }

  if ((bio->bi_rw & WRITE || bio->bi_rw & REQ_DISCARD) && !brd->is_writable) {
    printk(KERN_INFO DEVICE_NAME ": cow_brd%d not writable, EIO\n",
        brd->brd_number);
    goto out;
  }

  if (unlikely(bio->bi_rw & REQ_DISCARD)) {
    err = 0;
    discard_from_brd(brd, sector, bio->BI_SIZE);
    goto out;
  }

  rw = bio_rw(bio);
  if (rw == READA) {
    rw = READ;
  }

  #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
  struct bio_vec *bvec;
  int iter;
  bio_for_each_segment(bvec, bio, iter) {
    unsigned int len = bvec->bv_len;
    err = brd_do_bvec(brd, bvec->bv_page, len,
          bvec->bv_offset, rw, sector);
    if (err) {
      break;
    }
    sector += len >> SECTOR_SHIFT;
  }
  #else
  struct bio_vec bvec;
  struct bvec_iter iter;
  bio_for_each_segment(bvec, bio, iter) {
    unsigned int len = bvec.bv_len;
    err = brd_do_bvec(brd, bvec.bv_page, len,
          bvec.bv_offset, rw, sector);
    if (err) {
      break;
    }
    sector += len >> SECTOR_SHIFT;
  }
  #endif

out:
  BIO_ENDIO(bio, err);
}

#ifdef CONFIG_BLK_DEV_XIP
static int brd_direct_access(struct block_device *bdev, sector_t sector,
      void **kaddr, unsigned long *pfn)
{
  struct brd_device *brd = bdev->bd_disk->private_data;
  struct page *page;

  if (!brd)
    return -ENODEV;
  if (sector & (PAGE_SECTORS-1))
    return -EINVAL;
  if (sector + PAGE_SECTORS > get_capacity(bdev->bd_disk))
    return -ERANGE;
  page = brd_insert_page(brd, sector);
  if (!page)
    return -ENOMEM;
  *kaddr = page_address(page);
  *pfn = page_to_pfn(page);

  return 0;
}
#endif

static int brd_ioctl(struct block_device *bdev, fmode_t mode,
      unsigned int cmd, unsigned long arg)
{
  int error = 0;
  struct brd_device *brd = bdev->bd_disk->private_data;
  struct brd_device *snapshots;

  switch (cmd) {
    case COW_BRD_SNAPSHOT:
      if (brd->is_snapshot) {
        return -ENOTTY;
      }
      brd->is_writable = false;
      break;
    case COW_BRD_UNSNAPSHOT:
      if (brd->is_snapshot) {
        return -ENOTTY;
      }
      brd->is_writable = true;
      break;
    case COW_BRD_RESTORE_SNAPSHOT:
      if (!brd->is_snapshot) {
        return -ENOTTY;
      }
      brd_free_pages(brd);
      break;
    case COW_BRD_WIPE:
      if (brd->is_snapshot) {
        return -ENOTTY;
      }
      // Assumes no snapshots are being used right now.
      brd_free_pages(brd);
      break;
    default:
      error = -ENOTTY;
  }

  return error;
}

static const struct block_device_operations brd_fops = {
  .owner =    THIS_MODULE,
  .ioctl =    brd_ioctl,
#ifdef CONFIG_BLK_DEV_XIP
  .direct_access =  brd_direct_access,
#endif
};

/*
 * And now the modules code and kernel interface.
 */
int major_num = 0;
static int num_disks = 1;
static int num_snapshots = 1;
int disk_size = DEFAULT_COW_RD_SIZE;
static int max_part;
static int part_shift;
module_param(num_disks, int, S_IRUGO);
MODULE_PARM_DESC(num_disks, "Maximum number of ram block devices");
module_param(num_snapshots, int, S_IRUGO);
MODULE_PARM_DESC(num_snapshots, "Number of ram block snapshot devices where "
    "each disk gets it's own snapshot");
module_param(disk_size, int, S_IRUGO);
MODULE_PARM_DESC(disk_size, "Size of each RAM disk in kbytes.");
module_param(max_part, int, S_IRUGO);
MODULE_PARM_DESC(max_part, "Maximum number of partitions per RAM disk");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(RAMDISK_MAJOR);

#ifndef MODULE
/* Legacy boot options - nonmodular */
static int __init ramdisk_size(char *str)
{
  rd_size = simple_strtol(str, NULL, 0);
  return 1;
}
__setup("ramdisk_size=", ramdisk_size);
#endif

/*
 * The device scheme is derived from loop.c. Keep them in synch where possible
 * (should share code eventually).
 */
static LIST_HEAD(brd_devices);
static DEFINE_MUTEX(brd_devices_mutex);

static struct brd_device *brd_alloc(int i)
{
  struct brd_device *brd;
  struct gendisk *disk;

  brd = kzalloc(sizeof(*brd), GFP_KERNEL);
  if (!brd)
    goto out;
  brd->brd_number   = i;

  // True on disks until "snapshot" ioctl is called.
  brd->is_writable  = true;
  brd->is_snapshot  = i >= num_disks;

  spin_lock_init(&brd->brd_lock);
  INIT_RADIX_TREE(&brd->brd_pages, GFP_ATOMIC);

  brd->brd_queue = blk_alloc_queue(GFP_KERNEL);
  if (!brd->brd_queue)
    goto out_free_dev;
  blk_queue_make_request(brd->brd_queue, brd_make_request);
  blk_queue_max_hw_sectors(brd->brd_queue, 1024);
  blk_queue_bounce_limit(brd->brd_queue, BLK_BOUNCE_ANY);

  brd->brd_queue->limits.discard_granularity = PAGE_SIZE;
  brd->brd_queue->limits.max_discard_sectors = UINT_MAX;
  brd->brd_queue->limits.discard_zeroes_data = 1;
  queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, brd->brd_queue);

  disk = brd->brd_disk = alloc_disk(1 << part_shift);
  if (!disk)
    goto out_free_queue;
  disk->major   = major_num;
  disk->first_minor = i << part_shift;
  disk->fops    = &brd_fops;
  disk->private_data  = brd;
  disk->queue   = brd->brd_queue;
  disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
  if (brd->is_snapshot) {
    sprintf(disk->disk_name, "cow_ram_snapshot%d_%d", i / num_disks,
        i % num_disks);
  } else {
    sprintf(disk->disk_name, "cow_ram%d", i);
  }
  set_capacity(disk, disk_size * 2);

  return brd;

out_free_queue:
  blk_cleanup_queue(brd->brd_queue);
out_free_dev:
  kfree(brd);
out:
  return NULL;
}

static void brd_free(struct brd_device *brd)
{
  put_disk(brd->brd_disk);
  blk_cleanup_queue(brd->brd_queue);
  brd_free_pages(brd);
  kfree(brd);
}

static struct brd_device *brd_init_one(int i)
{
  struct brd_device *brd, *parent_brd;

  list_for_each_entry(brd, &brd_devices, brd_list) {
    if (brd->brd_number == i) {
      goto out;
    }
  }

  brd = brd_alloc(i);
  if (brd) {
    add_disk(brd->brd_disk);
    list_add_tail(&brd->brd_list, &brd_devices);

    if (i >= num_disks) {
      // Set the parent pointer for this device.
      list_for_each_entry(parent_brd, &brd_devices, brd_list) {
        if (parent_brd->brd_number == i % num_disks) {
          brd->parent_brd = parent_brd;
          break;
        }
      }
    } else {
      brd->parent_brd = NULL;
    }
  }
out:
  return brd;
}

static void brd_del_one(struct brd_device *brd)
{
  list_del(&brd->brd_list);
  del_gendisk(brd->brd_disk);
  brd_free(brd);
}

static struct kobject *brd_probe(dev_t dev, int *part, void *data)
{
  struct brd_device *brd;
  struct kobject *kobj;

  mutex_lock(&brd_devices_mutex);
  brd = brd_init_one(MINOR(dev) >> part_shift);
  kobj = brd ? get_disk(brd->brd_disk) : NULL;
  mutex_unlock(&brd_devices_mutex);

  *part = 0;
  return kobj;
}

static int __init brd_init(void)
{
  int i;
  const int nr = num_disks * (1 + num_snapshots);
  unsigned long range;
  struct brd_device *brd, *next, *parent_brd;

  major_num = register_blkdev(major_num, DEVICE_NAME);
  if (major_num <= 0) {
    printk(KERN_WARNING DEVICE_NAME ": unable to get major number\n");
    return -EIO;
  }

  /*
   * brd module now has a feature to instantiate underlying device
   * structure on-demand, provided that there is an access dev node.
   * However, this will not work well with user space tool that doesn't
   * know about such "feature".  In order to not break any existing
   * tool, we do the following:
   *
   * (1) if rd_nr is specified, create that many upfront, and this
   *     also becomes a hard limit.
   * (2) if rd_nr is not specified, create CONFIG_BLK_DEV_RAM_COUNT
   *     (default 16) rd device on module load, user can further
   *     extend brd device by create dev node themselves and have
   *     kernel automatically instantiate actual device on-demand.
   */

  part_shift = 0;
  if (max_part > 0) {
    part_shift = fls(max_part);

    /*
     * Adjust max_part according to part_shift as it is exported
     * to user space so that user can decide correct minor number
     * if [s]he want to create more devices.
     *
     * Note that -1 is required because partition 0 is reserved
     * for the whole disk.
     */
    max_part = (1UL << part_shift) - 1;
  }

  if ((1UL << part_shift) > DISK_MAX_PARTS) {
    return -EINVAL;
  }

  if (nr > 1UL << (MINORBITS - part_shift)) {
    return -EINVAL;
  }

  range = nr << part_shift;

  // The first num_disks devices are the actual disks, the rest are snapshot
  // devices.
  for (i = 0; i < nr; i++) {
    brd = brd_alloc(i);
    if (!brd) {
      goto out_free;
    }
    list_add_tail(&brd->brd_list, &brd_devices);

    // Set the parent pointer for this device.
    if (i >= num_disks) {
      list_for_each_entry(parent_brd, &brd_devices, brd_list) {
        if (parent_brd->brd_number == i % num_disks) {
          brd->parent_brd = parent_brd;
          break;
        }
      }
    } else {
      brd->parent_brd = NULL;
    }
  }

  /* point of no return */

  list_for_each_entry(brd, &brd_devices, brd_list)
    add_disk(brd->brd_disk);

  blk_register_region(MKDEV(RAMDISK_MAJOR, 0), range,
          THIS_MODULE, brd_probe, NULL, NULL);

  printk(KERN_INFO DEVICE_NAME ": module loaded with %d disks and %d snapshots"
      "\n", num_disks, num_disks * num_snapshots);
  return 0;

out_free:
  list_for_each_entry_safe(brd, next, &brd_devices, brd_list) {
    list_del(&brd->brd_list);
    brd_free(brd);
  }
  unregister_blkdev(major_num, DEVICE_NAME);

  return -ENOMEM;
}

static void __exit brd_exit(void)
{
  unsigned long range;
  struct brd_device *brd, *next;

  range = (num_disks * (1 + num_snapshots)) << part_shift;

  list_for_each_entry_safe(brd, next, &brd_devices, brd_list)
    brd_del_one(brd);

  blk_unregister_region(MKDEV(major_num, 0), range);
  unregister_blkdev(major_num, DEVICE_NAME);
  printk(KERN_INFO DEVICE_NAME ": module unloaded\n");
}

module_init(brd_init);
module_exit(brd_exit);

