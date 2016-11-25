#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define KERNEL_SECTOR_SIZE 512

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ashmrtn");
MODULE_DESCRIPTION("test hello world");

static int major_num = 0;
//module_param(major_num, int, 0);
static int hardsect_size = 512;
//module_param(hardsect_size, int, 0);
static int nsectors = 2;  /* How big the drive is */
//module_param(nsectors, int, 0);

static struct hwm_device {
  unsigned long size;
  spinlock_t lock;
  u8* data;
  struct gendisk* gd;
} Device;

// The device operations structure.
static const struct block_device_operations hellow_ops = {
  .owner       = THIS_MODULE
  //.ioctl       = blk_wrapper_ioctl
};

static void hellow_bio(struct request_queue* q, struct bio* bio) {
  printk(KERN_INFO "hwm: Ending bio request\n");
  bio_endio(bio, 0);
}

static int __init hello_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");

  // Get registered.
  major_num = register_blkdev(major_num, "hwm");
  if (major_num <= 0) {
    printk(KERN_WARNING "hwm: unable to get major number\n");
    goto out_unregister;
  }

  // Set up our internal device.
  spin_lock_init(&Device.lock);
  Device.size = nsectors * hardsect_size;
  Device.data = vmalloc(Device.size);
  if (Device.data == NULL) {
    return -ENOMEM;
  }

  // And the gendisk structure.
  Device.gd = alloc_disk(1);
  if (!Device.gd) {
    goto out;
  }

  Device.gd->private_data = &Device;
  Device.gd->major = major_num;
  Device.gd->first_minor = 0;
  Device.gd->minors = 1;
  set_capacity(Device.gd, nsectors * (hardsect_size / KERNEL_SECTOR_SIZE));
  strcpy(Device.gd->disk_name, "hwm0");
  Device.gd->fops = &hellow_ops;

  // Get a request queue.
  Device.gd->queue = blk_alloc_queue_node(GFP_KERNEL, NUMA_NO_NODE);
  if (Device.gd->queue == NULL) {
    goto out;
  }
  blk_queue_make_request(Device.gd->queue, hellow_bio);
  //blk_queue_hardsect_size(Queue, hardsect_size);

  add_disk(Device.gd);

  printk(KERN_NOTICE "hwm: initialized\n");
  return 0;

  out:
    vfree(Device.data);
  out_unregister:
    unregister_blkdev(major_num, "hwm");
    return -ENOMEM;
}

static void __exit hello_cleanup(void) {
  printk(KERN_INFO "hwm: Cleaning up\n");
}

module_init(hello_init);
module_exit(hello_cleanup);
