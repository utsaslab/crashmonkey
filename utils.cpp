// Very messy hack to get the defines for different bio operations. Should be
// changed to something more palatable if possible.
#include <linux/blk_types.h>

#include <cstring>

#include <iostream>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "utils.h"

namespace fs_testing {

namespace {

bool is_async_write(const disk_write& write) {
  return (
      !((write.metadata.bi_rw & REQ_SYNC) ||
        (write.metadata.bi_rw & REQ_FUA) ||
        (write.metadata.bi_rw & REQ_FLUSH) ||
        (write.metadata.bi_rw & REQ_FLUSH_SEQ) ||
        (write.metadata.bi_rw & REQ_SOFTBARRIER)) &&
      write.metadata.bi_rw & REQ_WRITE);
}

}  // namespace

using std::cout;
using std::memcpy;
using std::mt19937;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::uniform_int_distribution;
using std::vector;

disk_write::disk_write(const struct disk_write_op_meta& m,
    const void* d) {
  metadata = m;
  if (metadata.size > 0 && d != NULL) {
    data = shared_ptr<void>(new char[metadata.size]);
    memcpy(data.get(), d, metadata.size);
  }
}

disk_write::disk_write(const disk_write& other) {
  metadata = other.metadata;
  data = other.data;
}

disk_write& disk_write::operator=(const disk_write& other) {
  metadata = other.metadata;
  data = other.data;
}

bool operator==(const disk_write& a, const disk_write& b) {
  if (tie(a.metadata.bi_flags, a.metadata.bi_rw, a.metadata.write_sector,
        a.metadata.size) ==
      tie(b.metadata.bi_flags, b.metadata.bi_rw, b.metadata.write_sector,
        b.metadata.size)) {
    if ((a.data.get() == NULL && b.data.get() != NULL) ||
        (a.data.get() != NULL && b.data.get() == NULL)) {
      return false;
    }
    if (memcmp(a.data.get(), b.data.get(), a.metadata.size) == 0) {
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

bool disk_write::has_write_flag() {
  return metadata.bi_rw & REQ_WRITE;
}

shared_ptr<void> disk_write::set_data(const void* d) {
  if (metadata.size > 0 && d != NULL) {
    data = shared_ptr<void>(new char[metadata.size]);
    memcpy(data.get(), d, metadata.size);
  }
  return data;
}

shared_ptr<void> disk_write::get_data() {
  return data;
}

permuter::permuter(const vector<disk_write>* data) : original(*data) {
  for (int i = 0; i < original.size(); ++i) {
    if (is_async_write(original.at(i))) {
      struct permute_info info;
      info.write = original.at(i);
      info.shift = 0;
      ordering.push_back(info);
    }
  }
  current = original;
  // TODO(ashmrtn): Make a flag to make it random or not.
  rand = mt19937(42);
  //done = unorderd_set<vector<disk_write>>();
}

permuter& permuter::operator=(const permuter& other) {
  original = other.original;
  ordering = other.ordering;
  current = original;
  //done = unorderd_set<vector<disk_write>>();
}

bool permuter::permute(vector<disk_write>* res) {
  disk_write unordered;
  bool last_permutation = true;
  // Checks to see if we have gone through all the permutations possible. If we
  // have then return the first permutation that we got.
  for (int idx = ordering.size() - 1, cur_idx = current.size() - 1; idx >= 0;
      --idx, --cur_idx) {
    if (ordering.at(idx).write != current.at(cur_idx)) {
      last_permutation = false;
      unordered = ordering.at(idx).write;
      //cout << "unordered element set to: " << unordered << " at index: " << idx
      //  << std::endl;
      break;
    }
  }
  if (last_permutation) {
    return false;
  }
  int move_idx = 0;
  for (int i = 0; i < ordering.size(); ++i) {
    if (ordering.at(i).write == unordered) {
      move_idx = i;
      ordering.at(move_idx++).shift += 1;
      break;
    }
  }

  current = original;
  for (int i = move_idx; i < ordering.size(); ++i) {
    ordering.at(i).shift = 0;
  }
  for (permute_info info : ordering) {
    if (info.shift == 0) {
      continue;
    }

    int pos = 0;
    for (int i = 0; i < current.size(); ++i) {
      if (current.at(i) == info.write) {
        pos = i;
        break;
      }
    }

    for (int i = 0, idx = pos; i < info.shift; ++i, ++idx) {
      current.at(idx) = current.at(idx + 1);
    }
    current.at(pos + info.shift) = info.write;
  }
  *res = current;
  return true;
}

bool permuter::permute_nth(std::vector<disk_write>* res, int n) {
  for (int i = 0; i < n - 1; ++i) {
    if (!permute(res)) {
      return false;
    }
  }
  return true;
}

void permuter::permute_random(std::vector<disk_write>* res) {
  const int size = original.size();
  *res = original;
  for (permute_info info : ordering) {
    // Find where in the vector the current movable write is.
    int pos = 0;
    for (int i = 0; i < current.size(); ++i) {
      if (current.at(i) == info.write) {
        pos = i;
        break;
      }
    }
    uniform_int_distribution<int> uid(0, size - pos - 1);
    const int shift = uid(rand);

    // Move everything over and then place the write we're moving.
    for (int i = 0, idx = pos; i < shift; ++i, ++idx) {
      res->at(idx) = res->at(idx + 1);
    }
    res->at(pos + shift) = info.write;
  }
}

}  // namespace fs_testing
