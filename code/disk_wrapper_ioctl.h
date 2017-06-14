#ifndef HELLOW_IOCTL_H
#define HELLOW_IOCTL_H

#define HWM_LOG_OFF               0xff00
#define HWM_LOG_ON                0xff01
#define HWM_GET_LOG_META          0xff02
#define HWM_GET_LOG_DATA          0xff03
#define HWM_NEXT_ENT              0xff04
#define HWM_CLR_LOG               0xff05

#define COW_BRD_SNAPSHOT          0xff06
#define COW_BRD_UNSNAPSHOT        0xff07
#define COW_BRD_RESTORE_SNAPSHOT  0xff08
#define COW_BRD_WIPE              0xff09

// For ease of transferring data to user-land.
struct disk_write_op_meta {
  unsigned long bi_flags;
  unsigned long bi_rw;
  unsigned long write_sector;
  unsigned int size;
};

#endif
