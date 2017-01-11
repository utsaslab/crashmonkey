// Very messy hack to get the defines for different bio operations. Should be
// changed to something more palatable if possible.
#include <linux/blk_types.h>

#include <cstring>

#include <iostream>
#include <tuple>

#include "utils.h"

namespace fs_testing {

using std::cout;
using std::memcpy;
using std::ostream;
using std::tie;
using std::vector;

disk_write::disk_write() {
  data = NULL;
}

// TODO(ashmrtn): Make more efficient with something like shared poiners or
// another way to share data memory.
disk_write::disk_write(const struct disk_write_op_meta& m,
    //const void* d) : metadata(m) {
    const void* d) {
  metadata = m;
  if (metadata.size > 0 && d != NULL) {
    data = malloc(metadata.size);
    if (data != NULL) {
      memcpy(data, d, metadata.size);
    }
  } else {
    data = NULL;
  }
}

// TODO(ashmrtn): Make more efficient with something like shared poiners or
// another way to share data memory.
disk_write::disk_write(const disk_write& other) {
  metadata = other.metadata;
  if (metadata.size > 0 && other.data != NULL) {
    data = malloc(metadata.size);
    if (data != NULL) {
      memcpy(data, other.data, metadata.size);
    } else {
      data = NULL;
    }
  }
}

disk_write::~disk_write() {
  if (data != NULL) {
    free(data);
    data = NULL;
  }
}

disk_write& disk_write::operator=(const disk_write& other) {
  metadata = other.metadata;
  if (data != NULL) {
    free(data);
    data = NULL;
  }
  if (metadata.size > 0 && other.data != NULL) {
    data = malloc(other.metadata.size);
    if (data != NULL) {
      memcpy(data, other.data, metadata.size);
    }
  } else {
    data = NULL;
  }
}

bool operator==(const disk_write& a, const disk_write& b) {
  if (tie(a.metadata.bi_flags, a.metadata.bi_rw, a.metadata.write_sector,
        a.metadata.size) ==
      tie(b.metadata.bi_flags, b.metadata.bi_rw, b.metadata.write_sector,
        b.metadata.size)) {
    if ((a.data == NULL && b.data != NULL) ||
        (a.data != NULL && b.data == NULL)) {
      return false;
    }
    if (memcmp(a.data, b.data, a.metadata.size) == 0) {
      return true;
    }
  }
  return false;
}

bool operator!=(const disk_write& a, const disk_write& b) {
  return !(a == b);
}

ostream& operator<<(ostream& os, const disk_write& dw) {
  os << dw.metadata.bi_rw;
  return os;
}

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

}  // namespace fs_testing
