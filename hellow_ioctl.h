#ifndef HELLOW_IOCTL_H
#define HELLOW_IOCTL_H

#define HWM_LOG_OFF           0xff00
#define HWM_LOG_ON            0xff01
#define HWM_GET_LOG_ENT_SIZE  0xff02
#define HWM_GET_LOG_ENT       0xff03
#define HWM_CLR_LOG           0xff04

// For ease of transferring data to user-land.
struct disk_write_op_meta {
  unsigned long bi_flags;
  unsigned long bi_rw;
  unsigned long write_sector;
  unsigned int size;
};


#endif
