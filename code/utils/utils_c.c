//
// Created by Fábio Domingues on 19/07/17.
//

#include <assert.h>
#include <linux/blk_types.h>
#include <linux/version.h>
#include <string.h>

#include "utils_c.h"

  static const char* const flag_names[] = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
    "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
    "meta", "prio", "discard", "secure", "write same", "no idle", "fua",
    "flush", "read ahead", "throttled", "sorted", "soft barrier", "no merge",
    "started", "don't prep", "queued", "elv priv", "failed", "quiet", "preempt",
    "alloced", "copy user", "flush seq", "io stat", "mixed merge", "kernel",
    "pm", "end", "nr bits"
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
    "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
    "meta", "prio", "discard", "secure", "write same", "no idle", "fua",
    "flush", "read ahead", "throttled", "sorted", "soft barrier", "no merge",
    "started", "don't prep", "queued", "elv priv", "failed", "quiet", "preempt",
    "alloced", "copy user", "flush seq", "io stat", "mixed merge", "pm",
    "hashed", "mq_inflight", "no timeout", "nr bits"
#else
#error "Unsupported kernel version: CrashMonkey has not been tested with " \
       "your kernel version."
#endif
};

static char const checkpoint_name[] = "checkpoint";

bool c_is_async_write(struct disk_write_op_meta *m) {
  return !((m->bi_rw & REQ_SYNC) ||
            (m->bi_rw & REQ_FUA) ||
            (m->bi_rw & REQ_FLUSH) ||
            (m->bi_rw & REQ_FLUSH_SEQ) ||
            (m->bi_rw & REQ_SOFTBARRIER)) &&
            (m->bi_rw & REQ_WRITE);
}

bool c_is_barrier_write(struct disk_write_op_meta *m) {
  return ((m->bi_rw & REQ_FUA) ||
          (m->bi_rw & REQ_FLUSH) ||
          (m->bi_rw & REQ_FLUSH_SEQ) ||
          (m->bi_rw & REQ_SOFTBARRIER)) &&
          (m->bi_rw & REQ_WRITE);
}

bool c_is_meta(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_META) && true;
}

bool c_is_checkpoint(struct disk_write_op_meta *m) {
  return (m->bi_rw & HWM_CHECKPOINT_FLAG);
}

bool c_has_write_flag(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_WRITE) && true;
}

bool c_has_flush_flag(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_FLUSH) && true;
}

bool c_has_flush_seq_flag(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_FLUSH_SEQ) && true;
}

bool c_has_FUA_flag(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_FUA) && true;
}

void c_set_flush_flag(struct disk_write_op_meta *m) {
  m->bi_rw = (m->bi_rw | REQ_FLUSH); 
}

void c_set_flush_seq_flag(struct disk_write_op_meta *m) {
 m->bi_rw = (m->bi_rw | REQ_FLUSH_SEQ); 
}

void c_clear_flush_flag(struct disk_write_op_meta *m) {
  m->bi_rw = (m->bi_rw & ~(REQ_FLUSH));  
}

void c_clear_flush_seq_flag(struct disk_write_op_meta *m) {
  m->bi_rw = (m->bi_rw & ~(REQ_FLUSH_SEQ));
}

void c_flags_to_string(long long flags, char *buf, unsigned int buf_len) {
  unsigned int tmp_size;
  unsigned int i;
  // Start with remaining one less than length because we need a null
  // terminating character.
  unsigned int remaining = buf_len - 1;
  // To find if we have enough space to concatonate, we need to make sure we
  // have at least three more slots in the buffer, one for the trailing ',', one
  // for the trailing ' ', and one for the null character. When checking the
  // size of a string, we can check that it is the remaining size - 2 because
  // this will tell us if it is longer than the remaining size - 3 which is the
  // max we can handle.
  if (flags & HWM_CHECKPOINT_FLAG) {
    tmp_size = strnlen(checkpoint_name, remaining - 2);
    assert(tmp_size <= remaining - 3);
    strcat(buf, checkpoint_name);
    strcat(buf, ", ");
    remaining -= tmp_size - 3;
  }

  for (i = __REQ_WRITE; i < __REQ_NR_BITS; i++) {
    if (flags & (1ULL << i)) {
      tmp_size = strnlen(flag_names[i], remaining - 2);
      assert(tmp_size <= remaining - 3);
      strcat(buf, flag_names[i]);
      strcat(buf, ", ");
      remaining -= tmp_size - 3;
    }
  }
}
