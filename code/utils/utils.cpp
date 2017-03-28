// Very messy hack to get the defines for different bio operations. Should be
// changed to something more palatable if possible.
#include <linux/blk_types.h>

#include <cstring>

#include <fstream>
#include <istream>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

#include "utils.h"

namespace fs_testing {
namespace utils {
using std::cout;
using std::fstream;
using std::istream;
using std::memcpy;
using std::mt19937;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::uniform_int_distribution;
using std::vector;

bool disk_write::is_async_write() {
  return (
      !((metadata.bi_rw & REQ_SYNC) ||
        (metadata.bi_rw & REQ_FUA) ||
        (metadata.bi_rw & REQ_FLUSH) ||
        (metadata.bi_rw & REQ_FLUSH_SEQ) ||
        (metadata.bi_rw & REQ_SOFTBARRIER)) &&
        metadata.bi_rw & REQ_WRITE);
}

bool disk_write::is_barrier_write() {
  return (
      ((metadata.bi_rw & REQ_FUA) ||
        (metadata.bi_rw & REQ_FLUSH) ||
        (metadata.bi_rw & REQ_FLUSH_SEQ) ||
        (metadata.bi_rw & REQ_SOFTBARRIER)) &&
        metadata.bi_rw & REQ_WRITE);
}

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

void disk_write::operator=(const disk_write& other) {
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

static disk_write deserialize(istream& is) {
  disk_write_op_meta meta;
  is >> meta.bi_flags
    >> meta.bi_rw
    >> meta.write_sector
    >> meta.size;

  char *data = new char[meta.size];
  is >> data;
  return disk_write(meta, (void *) data);
}

/*
 * Output all data for a single object on a single line.
 */
fstream& operator<<(fstream& fs, const disk_write& dw) {
  fs << dw.metadata.bi_flags << " "
    << dw.metadata.bi_rw << " "
    << dw.metadata.write_sector << " "
    << dw.metadata.size << " "
    << dw.data;
  return fs;
}

ostream& operator<<(ostream& os, const disk_write& dw) {
  os << std::hex << std::showbase << dw.metadata.bi_rw << std::noshowbase
    << std::dec;
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

}  // namespace utils
}  // namespace fs_testing
