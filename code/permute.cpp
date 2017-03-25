# include <cstring>

#include <iostream>
#include <utility>
#include <vector>

#include "hellow_ioctl.h"
#include "utils.h"

using std::cout;
using std::ostream;
using std::memcmp;
using std::memset;
using std::pair;
using std::vector;

using fs_testing::disk_write;
using fs_testing::permuter;

/*
* Request flags.  For use in the cmd_flags field of struct request, and in
* bi_rw of struct bio.  Note that some flags are only valid in either one.
*/
enum rq_flag_bits {
/* common flags */
__REQ_WRITE,            /* not set, read. set, write */
__REQ_FAILFAST_DEV,     /* no driver retries of device errors */
__REQ_FAILFAST_TRANSPORT, /* no driver retries of transport errors */
__REQ_FAILFAST_DRIVER,  /* no driver retries of driver errors */

__REQ_SYNC,             /* request is sync (sync write or read) */
__REQ_META,             /* metadata io request */
__REQ_PRIO,             /* boost priority in cfq */
__REQ_DISCARD,          /* request to discard sectors */
__REQ_SECURE,           /* secure discard (used with __REQ_DISCARD) */
__REQ_WRITE_SAME,       /* write same block many times */

__REQ_NOIDLE,           /* don't anticipate more IO after this one */
__REQ_FUA,              /* forced unit access */
__REQ_FLUSH,            /* request for cache flush */

/* bio only flags */
__REQ_RAHEAD,           /* read ahead, can fail anytime */
__REQ_THROTTLED,        /* This bio has already been subjected to
                         * throttling rules. Don't do it again. */

/* request only flags */
__REQ_SORTED,           /* elevator knows about this request */
__REQ_SOFTBARRIER,      /* may not be passed by ioscheduler */
__REQ_NOMERGE,          /* don't touch this for merging */
__REQ_STARTED,          /* drive already may have started this one */
__REQ_DONTPREP,         /* don't call prep for this one */
__REQ_QUEUED,           /* uses queueing */
__REQ_ELVPRIV,          /* elevator private data attached */
__REQ_FAILED,           /* set if the request failed */
__REQ_QUIET,            /* don't worry about errors */
__REQ_PREEMPT,          /* set for "ide_preempt" requests */
__REQ_ALLOCED,          /* request came from our alloc pool */
__REQ_COPY_USER,        /* contains copies of user pages */
__REQ_FLUSH_SEQ,        /* request for flush sequence */
__REQ_IO_STAT,          /* account I/O stat */
__REQ_MIXED_MERGE,      /* merge of different types, fail separately */
__REQ_KERNEL,           /* direct IO to kernel pages */
__REQ_PM,               /* runtime pm request */
__REQ_END,              /* last of chain of requests */
__REQ_NR_BITS,          /* stops here */
};

#define REQ_WRITE               (1ULL << __REQ_WRITE)
#define REQ_SYNC                (1ULL << __REQ_SYNC)
#define REQ_META                (1ULL << __REQ_META)
#define REQ_PRIO                (1ULL << __REQ_PRIO)
#define REQ_DISCARD             (1ULL << __REQ_DISCARD)
#define REQ_NOIDLE              (1ULL << __REQ_NOIDLE)

#define REQ_SOFTBARRIER         (1ULL << __REQ_SOFTBARRIER)
#define REQ_FUA                 (1ULL << __REQ_FUA)
#define REQ_FLUSH               (1ULL << __REQ_FLUSH)
#define REQ_FLUSH_SEQ           (1ULL << __REQ_FLUSH_SEQ)
#define REQ_END                 (1ULL << __REQ_END)

/*
vector<disk_write> permute(const vector<disk_write>* data,
    const vector<disk_write>* original,
    const vector<disk_write>* ordering) {
  cout << "in permute\n";
  vector<disk_write> res = *data;
  bool last_permutation = true;
  // Checks to see if we have gone through all the permutations possible. If we
  // have then return the first permutation that we got.
  int move_idx = 0;
  int res_idx = res.size() - 1;
  for (int idx = ordering->size() - 1; idx >= 0; --idx) {
    if (ordering->at(idx) != res.at(res_idx)) {
      move_idx = res_idx;
      last_permutation = false;
      break;
    }
    --res_idx;
  }
  if (last_permutation) {
    return *original;
  }
  cout << "move index set to " << move_idx << " by first loop\n";
  cout << "checking last element of new ordering\n";

  // Move the asynchronus write furthest to the end over by one. If the
  // asynchronus write operations at the end are already in order then move the
  // next asynchronus write closest to the end over by one.
  if (res.back() == ordering->back()) {
    // Set all the writes except the one closest to the end that isn't already
    // ordered at the end to where they were in the original ordering.
    disk_write temp;
    for (int idx = move_idx; idx >= 0; --idx) {
      if (!(
            (res.at(idx).metadata.bi_rw & REQ_SYNC) ||
            (res.at(idx).metadata.bi_rw & REQ_FUA) ||
            (res.at(idx).metadata.bi_rw & REQ_FLUSH) ||
            (res.at(idx).metadata.bi_rw & REQ_FLUSH_SEQ) ||
            (res.at(idx).metadata.bi_rw & REQ_SOFTBARRIER)) &&
          res.at(idx).metadata.bi_rw & REQ_WRITE) {
        move_idx = idx;
        temp = res.at(idx);
        cout << "temp is " << temp << "\n";
        cout << "Move index determined to be " << move_idx << "\n";
        break;
      }
    }
    for (int idx = move_idx; idx < res.size(); ++idx) {
      res.at(idx) = original->at(idx);
    }
    res.at(move_idx) = res.at(move_idx + 1);
    res.at(move_idx + 1) = temp;
    return res;
  } else {
    cout << "Moving the last asynchronus element of the ordering over by one\n";
    // size() - 2 should be fine as the if statement above makes sure that the
    // last write is not the last asynchronus write submitted to the block
    // device.
    for (int idx = res.size() - 2; idx >= 0; --idx) {
      // This is the first asynchronus write so just shift it to the right by
      // one.
      if (!(
            (res.at(idx).metadata.bi_rw & REQ_SYNC) ||
            (res.at(idx).metadata.bi_rw & REQ_FUA) ||
            (res.at(idx).metadata.bi_rw & REQ_FLUSH) ||
            (res.at(idx).metadata.bi_rw & REQ_FLUSH_SEQ) ||
            (res.at(idx).metadata.bi_rw & REQ_SOFTBARRIER)) &&
          res.at(idx).metadata.bi_rw & REQ_WRITE) {
        cout << "index for moving is " << idx << "\n";
        disk_write temp = res.at(idx);
        res.at(idx) = res.at(idx + 1);
        res.at(idx + 1) = temp;
        return res;
      }
    }
  }
}
*/

int main(int argc, char** argv) {
  vector<disk_write> data;

  struct disk_write_op_meta meta;
  void* dw_data;

  meta.bi_rw = (REQ_WRITE);
  meta.size = 10;
  dw_data = malloc(10);
  memset(dw_data, 'a', 10);
  data.emplace_back(meta, dw_data);

  meta.bi_rw = (REQ_WRITE | REQ_SYNC);
  meta.size = 0;
  data.emplace_back(meta, (const void*) NULL);

  meta.bi_rw = (REQ_WRITE | REQ_PRIO);
  meta.size = 15;
  dw_data = malloc(15);
  memset(dw_data, 'b', 15);
  data.emplace_back(meta, dw_data);

  meta.bi_rw = (REQ_WRITE | REQ_SYNC | REQ_FUA);
  meta.size = 0;
  data.emplace_back(meta, (const void*) NULL);

  meta.bi_rw = (REQ_WRITE | REQ_META);
  meta.size = 20;
  dw_data = malloc(20);
  memset(dw_data, 'c', 20);
  data.emplace_back(meta, dw_data);

  meta.bi_rw = (REQ_WRITE | REQ_FLUSH | REQ_PRIO);
  meta.size = 0;
  data.emplace_back(meta, (const void*) NULL);

  vector<disk_write> other;

  cout << "original is:\n";
  for (auto it = data.begin(); it != data.end(); ++it) {
    cout << "\t" << *it << "\n";
  }

  permuter p = permuter(&data);

  cout << "Starting permutations\n";
  int i = 0;
  do {
    ++i;
    other = p.permute();
    cout << "permutation " << i << ":\n";
    for (auto it = other.begin(); it != other.end(); ++it) {
      cout << "\t" << *it << "\n";
    }
  } while (other != data);

  cout << "Done with permutations\n";
  return 0;
}
