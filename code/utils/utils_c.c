//
// Created by Fábio Domingues on 19/07/17.
//

#include "utils_c.h"
#include <linux/blk_types.h>

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

bool c_has_write_flag(struct disk_write_op_meta *m) {
  return (m->bi_rw & REQ_WRITE) && true;
}
