#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include "hellow_ioctl.h"

#define KERNEL_SECTOR_SIZE 512

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ashmrtn");
MODULE_DESCRIPTION("test hello world");

static char* target_device_path = "";
module_param(target_device_path, charp, 0);

const char* const flag_names[] = {
  "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
  "meta", "prio", "discard", "secure", "write same", "no idle", "fua", "flush",
  "read ahead", "throttled", "sorted", "soft barrier", "no merge", "started",
  "don't prep", "queued", "elv priv", "failed", "quiet", "preempt", "alloced",
  "copy user", "flush seq", "io stat", "mixed merge", "kernel", "pm", "end",
  "nr bits"
};

struct disk_write_op {
  struct disk_write_op_meta metadata;
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
  // Pointer to log entry to be sent to user-land next.
  struct disk_write_op* current_log_write;
} Device;

static void free_logs(void) {
  struct disk_write_op* w = Device.writes;
  struct disk_write_op* tmp_w;
  while (w != NULL) {
    kfree(w->data);
    tmp_w = w;
    w = w->next;
    kfree(tmp_w);
  }
  Device.current_write = NULL;
  Device.writes = NULL;
  Device.current_log_write = NULL;
}

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
    case HWM_GET_LOG_META:
      //printk(KERN_INFO "hwm: getting next log entry meta\n");
      if (Device.current_log_write == NULL) {
        printk(KERN_WARNING "hwm: no log entry here \n");
        return -ENODATA;
      }
      if (!access_ok(VERIFY_WRITE, (void*) arg,
            sizeof(struct disk_write_op_meta))) {
        // TODO(ashmrtn): Find right error code.
        printk(KERN_WARNING "hwm: bad user land memory pointer in log entry"
            " size\n");
        return -EFAULT;
      }
      // Copy metadata.
      unsigned int not_copied = sizeof(struct disk_write_op_meta);
      while (not_copied != 0) {
        unsigned int offset = sizeof(struct disk_write_op_meta) - not_copied;
        not_copied = copy_to_user((void*) (arg + offset),
            &(Device.current_log_write->metadata) + offset, not_copied);
      }
      break;
    case HWM_GET_LOG_DATA:
      //printk(KERN_INFO "hwm: getting log entry data\n");
      if (Device.current_log_write == NULL) {
        printk(KERN_WARNING "hwm: no log entries to report data for\n");
        return -ENODATA;
      }
      if (!access_ok(VERIFY_WRITE, (void*) arg,
            Device.current_log_write->metadata.size)) {
        // TODO(ashmrtn): Find right error code.
        return -EFAULT;
      }

      // Copy written data.
      not_copied = Device.current_log_write->metadata.size;
      while (not_copied != 0) {
        unsigned int offset =
          Device.current_log_write->metadata.size - not_copied;
        not_copied = copy_to_user((void*) (arg + offset),
            Device.current_log_write->data + offset, not_copied);
      }
      break;
    case HWM_NEXT_ENT:
      //printk(KERN_INFO "hwm: moving to next log entry\n");
      if (Device.current_log_write == NULL) {
        printk(KERN_WARNING "hwm: no next log entry\n");
        return -ENODATA;
      }
      Device.current_log_write = Device.current_log_write->next;
      break;
    case HWM_CLR_LOG:
      printk(KERN_INFO "hwm: clearing data logs\n");
      free_logs();
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

static void print_rw_flags(unsigned long rw, unsigned long flags) {
  printk(KERN_INFO "\traw rw flags: 0x%.4lx\n", rw);
  int i;
  for (i = __REQ_WRITE; i < __REQ_NR_BITS; i++) {
    if (rw & (1ULL << i)) {
      printk(KERN_INFO "\t%s\n", flag_names[i]);
    }
  }
  /*
  printk(KERN_INFO "\traw flags flags: %lx\n", flags);
  for (i = __REQ_WRITE; i < __REQ_NR_BITS; i++) {
    if (flags & (1ULL << i)) {
      printk(KERN_INFO "\t%s\n", flag_names[i]);
    }
  }
  */
}

// TODO(ashmrtn): Currently not thread safe/reentrant. Make it so.
static void hellow_bio(struct request_queue* q, struct bio* bio) {
  struct hwm_device* hwm;
  struct request_queue* target_queue;

  /*
  printk(KERN_INFO "hwm: bio rw of size %u headed for 0x%lx (sector 0x%lx)"
      " has flags:\n", bio->bi_size, bio->bi_sector * 512, bio->bi_sector);
  print_rw_flags(bio->bi_rw, bio->bi_flags);
  */
  // Log information about writes, fua, and flush/flush_seq events in kernel
  // memory.
  if (Device.log_on) {
    if (bio->bi_rw & REQ_FLUSH ||
        bio->bi_rw & REQ_FUA || bio->bi_rw & REQ_FLUSH_SEQ ||
        bio->bi_rw & REQ_WRITE || bio->bi_rw & REQ_DISCARD ||
        bio->bi_flags & REQ_FLUSH || bio->bi_flags & REQ_FUA ||
        bio->bi_flags & REQ_WRITE) {

      //printk(KERN_INFO "hwm: logging above bio\n");
      printk(KERN_INFO "hwm: bio rw of size %u headed for 0x%lx (sector 0x%lx)"
          " has flags:\n", bio->bi_size, bio->bi_sector * 512, bio->bi_sector);
      print_rw_flags(bio->bi_rw, bio->bi_flags);

      // Log data to disk logs.
      struct disk_write_op* write =
        kzalloc(sizeof(struct disk_write_op), GFP_NOIO);
      if (write == NULL) {
        printk(KERN_WARNING "hwm: unable to make new write node\n");
        goto passthrough;
      }
      write->metadata.bi_flags = bio->bi_flags;
      write->metadata.bi_rw = bio->bi_rw;
      write->metadata.write_sector = bio->bi_sector;
      write->metadata.size = bio->bi_size;

      if (Device.current_write == NULL) {
        // This is the first write in the log.
        Device.writes = write;
        // Set the first write in the log so that it's picked up later.
        Device.current_log_write = write;
      } else {
        // Some write(s) was/were already made so add this to the back of the
        // chain and update pointers.
        Device.current_write->next = write;
      }
      Device.current_write = write;

      write->data = kmalloc(write->metadata.size, GFP_NOIO);
      if (write->data == NULL) {
        printk(KERN_WARNING "hwm: unable to get memory for data logging\n");
        kfree(write);
        goto passthrough;
      }
      int copied_data = 0;

      int i = 0;
      struct bio_vec* vec;
      bio_for_each_segment(vec, bio, i) {
        //printk(KERN_INFO "hwm: making new page for segment of data\n");

        memcpy((void*) (write->data + copied_data), vec->bv_page + vec->bv_offset,
            vec->bv_len);
        copied_data += vec->bv_len;
      }

      // Sanity check which prints data copied to the log.
      /*
      printk(KERN_INFO "hwm: copied %ld bytes of from %lx data:"
          "\n~~~\n%s\n~~~\n",
          write->metadata.size, write->metadata.write_sector * 512,
          write->data);
      */
    }
  }

 passthrough:
  // Pass request off to normal device driver.
  hwm = (struct hwm_device*) q->queuedata;
  target_queue = hwm->target_dev->bd_queue;
  bio->bi_bdev = hwm->target_dev;
  target_queue->make_request_fn(target_queue, bio);
}

// TODO(ashmrtn): Fix error when wrong device path is passed.
static int __init hellow_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");
  if (strlen(target_device_path) == 0) {
    return -ENOTTY;
  }
  printk(KERN_INFO "hwm: Wrapping device %s\n", target_device_path);
  // Get memory for our starting disk epoch node.
  Device.log_on = false;

  // Get registered.
  major_num = register_blkdev(major_num, "hwm");
  if (major_num <= 0) {
    printk(KERN_WARNING "hwm: unable to get major number\n");
    goto out;
  }

  Device.target_dev = blkdev_get_by_path(target_device_path,
      FMODE_READ, &Device);
  if (!Device.target_dev) {
    printk(KERN_WARNING "hwm: unable to grab underlying device\n");
    goto out;
  }
  if (!Device.target_dev->bd_queue) {
    printk(KERN_WARNING "hwm: attempt to wrap device with no request queue\n");
    goto out;
  }
  if (!Device.target_dev->bd_queue->make_request_fn) {
    printk(KERN_WARNING "hwm: attempt to wrap device with no "
        "make_request_fn\n");
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
  // Make this queue have the same flags as the queue we're feeding into.
  Device.gd->queue->flush_flags = Device.target_dev->bd_queue->flush_flags;
  Device.gd->queue->queue_flags = Device.target_dev->bd_queue->queue_flags;
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
  free_logs();
  blkdev_put(Device.target_dev, FMODE_READ);
  blk_cleanup_queue(Device.gd->queue);
  del_gendisk(Device.gd);
  put_disk(Device.gd);
  unregister_blkdev(major_num, "hwm");

  printk(KERN_INFO "hwm: Cleaning up bye!\n");
}

module_init(hellow_init);
module_exit(hello_cleanup);
