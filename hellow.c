#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "hellow_ioctl.h"

#define KERNEL_SECTOR_SIZE 512
#define TARGET_DEVICE_PATH "/dev/vdb"

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

struct disk_write_op {
  unsigned long bi_flags;
  unsigned long bi_rw;
  sector_t write_sector;
  unsigned int size;
  void* data;
  struct disk_write_op* next;
};

static int major_num = 0;

static struct hwm_device {
  unsigned long size;
  spinlock_t lock;
  u8* data;
  struct gendisk* gd;
  bool log_on;
  struct block_device* target_dev;
  // Pointer to first write op in the chain.
  struct disk_write_op* writes;
  // Pointer to last write op in the chain.
  struct disk_write_op* current_write;
} Device;

// TODO(ashmrtn): Add mutexes/locking to make thread-safe.
static int hellow_ioctl(struct block_device* bdev, fmode_t mode,
    unsigned int cmd, unsigned long arg) {
  int ret = 0;

  switch (cmd) {
    case HWM_LOG_OFF:
      printk(KERN_INFO "hwm: turning off data logging\n");
      Device.log_on = false;
      break;
    case HWM_LOG_ON:
      printk(KERN_INFO "hwm: turning on data logging\n");
      Device.log_on = true;
      break;
    default:
      ret = -EINVAL;
  }
  return ret;
}

// The device operations structure.
static const struct block_device_operations hellow_ops = {
  .owner   = THIS_MODULE,
  .ioctl   = hellow_ioctl,
};

static void print_rw_flags(unsigned long rw) {
  int i;
  for (i = __REQ_WRITE; i < __REQ_NR_BITS; i++) {
    if (rw & (1ULL << i)) {
      printk(KERN_INFO "\t%s\n", flag_names[i]);
    }
  }
}

// TODO(ashmrtn): Currently not thread safe/reentrant. Make it so.
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
  // Log information about writes, fua, and flush/flush_seq events in kernel
  // memory.
  // TODO(ashmrtn): Add support for discard operations.
  // TODO(ashmrtn): Add support for flush/fua operations.
  if (Device.log_on) {
    if (bio->bi_rw & REQ_FLUSH ||
        bio->bi_rw & REQ_FUA || bio->bi_rw & REQ_FLUSH_SEQ) {
      // Start a new epoch with the disk logs.
    } else if (bio->bi_rw & REQ_WRITE || bio->bi_rw & REQ_DISCARD) {
      // Log data to disk logs.
      struct disk_write_op* write =
        kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
      if (write == NULL) {
        printk(KERN_WARNING "hwm: unable to make new write node\n");
        goto passthrough;
      }
      write->bi_flags = bio->bi_flags;
      write->bi_rw = bio->bi_rw;
      write->write_sector = bio->bi_sector;
      write->size = bio->bi_size;

      if (Device.current_write == NULL) {
        // This is the first write in the log.
        Device.writes = write;
      } else {
        // Some write(s) was/were already made so add this to the back of the
        // chain and update pointers.
        Device.current_write->next = write;
      }
      Device.current_write = write;

      write->data = kmalloc(write->size, GFP_NOIO);
      if (write->data == NULL) {
        printk(KERN_WARNING "hwm: unable to get memory for data logging\n");
        kfree(write);
        goto passthrough;
      }
      int copied_data = 0;

      int i = 0;
      struct bio_vec* vec;
      bio_for_each_segment(vec, bio, i) {
        printk(KERN_INFO "hwm: making new page for segment of data\n");

        void* bio_data = kmap(vec->bv_page);
        memcpy((void*) (write->data + copied_data), bio_data + vec->bv_offset,
            vec->bv_len);
        kunmap(bio_data);
        copied_data += vec->bv_len;
      }

      // Sanity check which prints data copied to the log.
      char* data = kzalloc(write->size, GFP_NOIO);
      strncpy(data, (const char*) (write->data), write->size);
      printk(KERN_INFO "hwm: copied data:\n~~~\n%s\n~~~\n", data);
      kfree(data);
    }
  }

 passthrough:
  // Pass request off to normal device driver.
  hwm = (struct hwm_device*) q->queuedata;
  target_queue = hwm->target_dev->bd_queue;
  bio->bi_bdev = hwm->target_dev;
  target_queue->make_request_fn(target_queue, bio);
}

static int __init hello_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");
  // Get memory for our starting disk epoch node.
  Device.log_on = false;
  //Device.log_on = true;

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

static void free_logs(void) {
  struct disk_write_op* w = Device.writes;
  struct disk_write_op* tmp_w;
  while (w != NULL) {
    kfree(w->data);
    tmp_w = w;
    w = w->next;
    kfree(tmp_w);
  }
}

static void __exit hello_cleanup(void) {
  free_logs();
  blkdev_put(Device.target_dev, FMODE_READ | FMODE_WRITE);
  blk_cleanup_queue(Device.gd->queue);
  del_gendisk(Device.gd);
  put_disk(Device.gd);
  unregister_blkdev(major_num, "hwm");

  printk(KERN_INFO "hwm: Cleaning up\n");
}

module_init(hello_init);
module_exit(hello_cleanup);
