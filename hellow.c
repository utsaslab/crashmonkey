#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>

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

struct page_node {
  unsigned long page;
  unsigned int len;
  unsigned int offset;
  struct page_node* next;
};

struct disk_write_op {
  unsigned long bi_flags;
  unsigned long bi_rw;
  struct page_node* pages;
  struct page_node* current_page;
  struct disk_write_op* next;
};

struct epoch_list_node {
  struct disk_write_op* writes;
  struct disk_write_op* current_write;
  struct epoch_list_node* next;
};

static int major_num = 0;

static struct hwm_device {
  unsigned long size;
  spinlock_t lock;
  u8* data;
  struct gendisk* gd;
  struct block_device* target_dev;
  struct epoch_list_node* epochs;
  struct epoch_list_node* current_epoch;
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
  if (bio->bi_rw & REQ_FLUSH ||
      bio->bi_rw & REQ_FUA || bio->bi_rw & REQ_FLUSH_SEQ) {
    // Start a new epoch with the disk logs.
  } else if (bio->bi_rw & REQ_WRITE || bio->bi_rw & REQ_DISCARD) {
    // Log data to disk logs.
    struct disk_write_op* write =
      kmalloc(sizeof(struct disk_write_op), GFP_NOIO);
    if (write == NULL) {
      printk(KERN_WARNING "hwm: unable to make new write node\n");
      goto passthrough;
    }
    memset(write, 0, sizeof(struct disk_write_op));
    write->bi_flags = bio->bi_flags;
    write->bi_rw = bio->bi_rw;

    if (Device.current_epoch->current_write == NULL) {
      // This is the first write for this epoch.
      Device.current_epoch->writes = write;
    } else {
      Device.current_epoch->current_write->next = write;
    }
    Device.current_epoch->current_write = write;

    int i = 0;
    struct bio_vec* vec;
    bio_for_each_segment(vec, bio, i) {
      printk(KERN_INFO "hwm: making new page for segment of data\n");
      struct page_node* new_page_node =
        kmalloc(sizeof(struct page_node), GFP_NOIO);
      if (new_page_node == NULL) {
        printk(KERN_WARNING "hwm: unable to get new page node\n");
        kfree(write);
        goto passthrough;
      }
      memset(new_page_node, 0, sizeof(struct page_node));
      new_page_node->page = get_zeroed_page(GFP_NOIO);
      if (new_page_node->page == 0) {
        kfree(new_page_node);
        kfree(write);
        printk(KERN_WARNING "hwm: unable to get page to copy write data to\n");
        goto passthrough;
      }
      void* bio_data = kmap(vec->bv_page);
      copy_page((void*) new_page_node->page, bio_data);
      kunmap(bio_data);
      if (write->current_page != NULL) {
        write->current_page->next = new_page_node;
      } else {
        write->pages = new_page_node;
      }
      write->current_page = new_page_node;
      new_page_node->len = vec->bv_len;
      new_page_node->offset = vec->bv_offset;

      /*
      // Sanity check which prints data copied to the log.
      char* data = kmalloc(new_page_node->len + 1, GFP_NOIO);
      strncpy(data, (const char*) (new_page_node->page + new_page_node->offset),
          new_page_node->len);
      printk(KERN_INFO "hwm: copied data:\n~~~\n%s\n~~~\n", data);
      kfree(data);
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

static int __init hello_init(void) {
  printk(KERN_INFO "hwm: Hello World from module\n");
  // Get memory for our starting disk epoch node.
  Device.epochs = kmalloc(sizeof(struct epoch_list_node), GFP_NOIO);
  if (Device.epochs == NULL) {
    printk(KERN_WARNING "hwm: unable to get memory for epochs\n");
    goto out;
  }
  memset(Device.epochs, 0, sizeof(struct epoch_list_node));
  Device.current_epoch = Device.epochs;

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
  struct disk_write_op* w;
  struct disk_write_op* tmp_w;
  struct page_node* p;
  struct page_node* tmp_p;
  struct epoch_list_node* tmp_e;
  struct epoch_list_node* e = Device.epochs;
  while (e != NULL) {
    w = e->writes;
    while (w != NULL) {
      p = w->pages;
      while (p != NULL) {
        free_page(p->page);
        tmp_p = p;
        p = p->next;
        kfree(tmp_p);
      }
      tmp_w = w;
      w = w->next;
      kfree(tmp_w);
    }
    tmp_e = e;
    e = e->next;
    kfree(tmp_e);
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
