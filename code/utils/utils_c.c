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

bool c_is_journal(struct disk_write_op_meta *m){
  return (m->write_sector >= 0x2F8 && m->write_sector <=0xAF6) && true;
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
