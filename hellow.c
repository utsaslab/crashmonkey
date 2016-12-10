#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define KERNEL_SECTOR_SIZE 512
#define TARGET_DEVICE_PATH "/dev/test_disk/backup"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ashmrtn");
MODULE_DESCRIPTION("test hello world");

const char* const flag_names[] = {
  "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
  "meta", "prio", "discard", "secure", "write same", "no idle", "fua", "flush",
  "read ahead", "throttled", "sorted", "soft barrier", "no merge", "started",
  "don't prep", "queued", "elv priv", "failed", "quiet", "preempt", "alloced",
  "copy user", "flush seq", "io stat", "mixed merge", "kernel", "pm", "end",
  "nr bits"
};

static int major_num = 0;

static struct hwm_device {
  unsigned long size;
  spinlock_t lock;
  u8* data;
  struct gendisk* gd;
  struct block_device* target_dev;
} Device;

// The device operations structure.
static const struct block_device_operations hellow_ops = {
  .owner       = THIS_MODULE
  //.ioctl       = blk_wrapper_ioctl
};

static void print_rw_flags(unsigned long rw) {
  int i;
  for (i = __REQ_WRITE; i < __REQ_NR_BITS; i++) {
    if (rw & (1ULL << i)) {
      printk(KERN_INFO "\t%s\n", flag_names[i]);
    }
  }
}

static void hellow_bio(struct request_queue* q, struct bio* bio) {
  struct hwm_device* hwm;
  struct request_queue* target_queue;

  printk(KERN_INFO "hwm: passing request to normal block device driver\n");
  if (bio->bi_bdev->bd_contains != bio->bi_bdev) {
    printk(KERN_INFO "hwm: writing to partition starting at sector %x\n",
        bio->bi_bdev->bd_part->start_sect);
  }
  printk(KERN_INFO "hwm: bio rw has flags:\n");
  print_rw_flags(bio->bi_rw);
  /*
  if (bio->bi_rw & REQ_WRITE) {
    printk(KERN_INFO "hwm: bio marked as write\n");
  }
  printk(KERN_INFO "hwm: bio flagged with %lx and rw is %lx\n", bio->bi_flags,
      bio->bi_rw);
  */
  hwm = (struct hwm_device*) q->queuedata;
  target_queue = hwm->target_dev->bd_queue;
  bio->bi_bdev = hwm->target_dev;
  target_queue->make_request_fn(target_queue, bio);
}

static int __init hello_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");

  // Get registered.
  major_num = register_blkdev(major_num, "hwm");
  if (major_num <= 0) {
    printk(KERN_WARNING "hwm: unable to get major number\n");
    goto out;
  }

  Device.target_dev = blkdev_get_by_path(TARGET_DEVICE_PATH,
      FMODE_READ | FMODE_WRITE, &Device);
  if (!Device.target_dev) {
    printk(KERN_WARNING "hwm: unable to grab underlying device\n");
    goto out;
  }

  // Set up our internal device.
  spin_lock_init(&Device.lock);

  // And the gendisk structure.
  Device.gd = alloc_disk(1);
  if (!Device.gd) {
    goto out;
  }

  Device.gd->private_data = &Device;
  Device.gd->major = major_num;
  Device.gd->first_minor = Device.target_dev->bd_disk->first_minor;
  Device.gd->minors = Device.target_dev->bd_disk->minors;
  set_capacity(Device.gd, get_capacity(Device.target_dev->bd_disk));
  strcpy(Device.gd->disk_name, "hwm");
  Device.gd->fops = &hellow_ops;

  // Get a request queue.
  Device.gd->queue = blk_alloc_queue(GFP_KERNEL);
  if (Device.gd->queue == NULL) {
    goto out;
  }
  blk_queue_make_request(Device.gd->queue, hellow_bio);
  Device.gd->queue->queuedata = &Device;
  //blk_queue_hardsect_size(Queue, hardsect_size);

  add_disk(Device.gd);

  printk(KERN_NOTICE "hwm: initialized\n");
  return 0;

  out:
    unregister_blkdev(major_num, "hwm");
    return -ENOMEM;
}

static void __exit hello_cleanup(void) {
  blkdev_put(Device.target_dev, FMODE_READ | FMODE_WRITE);
  blk_cleanup_queue(Device.gd->queue);
  del_gendisk(Device.gd);
  put_disk(Device.gd);
  unregister_blkdev(major_num, "hwm");

  printk(KERN_INFO "hwm: Cleaning up\n");
}

module_init(hello_init);
module_exit(hello_cleanup);
