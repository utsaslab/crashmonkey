#ifndef HELLOW_IOCTL_H
#define HELLOW_IOCTL_H

#define HWM_LOG_OFF               0xff00
#define HWM_LOG_ON                0xff01
#define HWM_GET_LOG_META          0xff02
#define HWM_GET_LOG_DATA          0xff03
#define HWM_NEXT_ENT              0xff04
#define HWM_CLR_LOG               0xff05
#define HWM_CHECKPOINT            0xff06

#define COW_BRD_SNAPSHOT          0xff06
#define COW_BRD_UNSNAPSHOT        0xff07
#define COW_BRD_RESTORE_SNAPSHOT  0xff08
#define COW_BRD_WIPE              0xff09

// Defines that are separate from the kernel because these values aren't stable.
// Based on 4.4 kernel flags.
//
// TODO(ashmrtn): These should be narrowed down to just the flags that we
// actually care about.
enum flag_shifts {
  // Common flags.
  REQ_WRITE_,
  REQ_FAILFAST_DEV_,
  REQ_FAILFAST_TRANSPORT_,
  REQ_FAILFAST_DRIVER_,

  REQ_SYNC_,
  REQ_META_,
  REQ_PRIO_,
  REQ_DISCARD_,

  REQ_SECURE_,
  REQ_WRITE_SAME_,
  REQ_NOIDLE_,
  REQ_INTEGRITY_,

  REQ_FUA_,
  REQ_FLUSH_,
  // Bio only flags.
  REQ_READAHEAD_,
  REQ_THROTTLED_,

  // Request only flags.
  REQ_SORTED_,
  REQ_SOFTBARRIER_,
  REQ_NOMERGE_,
  REQ_STARTED_,

  REQ_DONTPREP_,
  REQ_QUEUED_,
  REQ_ELVPRIV_,
  REQ_FAILED_,

  REQ_QUIET_,
  REQ_PREEMPT_,
  REQ_ALLOCED_,
  REQ_COPY_USER_,

  REQ_FLUSH_SEQ_,
  REQ_IO_STAT_,
  REQ_MIXED_MERGE_,
  REQ_PM_,

  REQ_HASHED_,
  REQ_MQ_INFLIGHT_,
  REQ_NO_TIMEOUT_,
  REQ_NR_BITS_,
};

#define HWM_WRITE_FLAG (1ULL << REQ_WRITE_)
#define HWM_FAILFAST_DEV_FLAG (1ULL << REQ_FAILFAST_DEV_)
#define HWM_FAILFAST_TRANSPORT_FLAG (1ULL << REQ_FAILFAST_TRANSPORT_)
#define HWM_FAILFAST_DRIVER_FLAG (1ULL << REQ_FAILFAST_DRIVER_)

#define HWM_SYNC_FLAG (1ULL << REQ_SYNC_)
#define HWM_META_FLAG (1ULL << REQ_META_)
#define HWM_PRIO_FLAG (1ULL << REQ_PRIO_)
#define HWM_DISCARD_FLAG (1ULL << REQ_DISCARD_)

#define HWM_SECURE_FLAG (1ULL << REQ_SECURE_)
#define HWM_WRITE_SAME_FLAG (1ULL << REQ_WRITE_SAME)
#define HWM_NOIDLE_FLAG (1ULL << REQ_NOIDLE_)
#define HWM_INTEGRITY_FLAG (1ULL << REQ_INTEGRITY_)

#define HWM_FUA_FLAG (1ULL << REQ_FUA_)
#define HWM_FLUSH_FLAG (1ULL << REQ_FLUSH_)
#define HWM_READAHEAD_FLAG (1ULL << REQ_READAHEAD_)
#define HWM_THROTTLED_FLAG (1ULL << REQ_THROTTLED_)

#define HWM_SORTED_FLAG (1ULL << REQ_SORTED_)
#define HWM_SOFTBARRIER_FLAG (1ULL << REQ_SOFTBARRIER_)
#define HWM_NOMERGE_FLAG (1ULL << REQ_NOMERGE_)
#define HWM_STARTED_FLAG (1ULL << REQ_STARTED_)

#define HWM_DONTPREP_FLAG (1ULL << REQ_DONTPREP_)
#define HWM_QUEUED_FLAG (1ULL << REQ_QUEUED_)
#define HWM_ELVPRIV_FLAG (1ULL << REQ_ELVPRIV_)
#define HWM_FAILED_FLAG (1ULL << REQ_FAILED_)

#define HWM_QUIET_FLAG (1ULL << REQ_QUIET_)
#define HWM_PREEMPT_FLAG (1ULL << REQ_PREEMPT_)
#define HWM_ALLOCED_FLAG (1ULL << REQ_ALLOCED_)
#define HWM_COPY_USER_FLAG (1ULL << REQ_COPY_USER_)

#define HWM_FLUSH_SEQ_FLAG (1ULL << REQ_FLUSH_SEQ_)
#define HWM_IO_STAT_FLAG (1ULL << REQ_IO_STAT_)
#define HWM_MIXED_MERGE_FLAG (1ULL << REQ_MIXED_MERGE_)
#define HWM_PM_FLAG (1ULL << REQ_PM_)

#define HWM_HASHED_FLAG (1ULL << REQ_HASHED_)
#define HWM_MQ_INFLIGHT_FLAG (1ULL << REQ_MQ_INFLIGHT_)
#define HWM_NO_TIMEOUT_FLAG (1ULL << REQ_NO_TIMEOUT_)

#define HWM_CHECKPOINT_FLAG       (1ULL << 63)

// For ease of transferring data to user-land.
struct disk_write_op_meta {
  unsigned long long bi_flags;
  unsigned long long bi_rw;
  unsigned long write_sector;
  unsigned int size;
  unsigned long long time_ns;
};

#endif
