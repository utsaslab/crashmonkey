// Very messy hack to get the defines for different bio operations. Should be
// changed to something more palatable if possible.

#include <cassert>
#include <cstring>

#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "utils.h"
#include "utils_c.h"

namespace fs_testing {
namespace utils {
using std::endl;
using std::ifstream;
using std::ios;
using std::memcpy;
using std::mt19937;
using std::ofstream;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::uniform_int_distribution;
using std::vector;

bool disk_write::is_async_write() {
  return c_is_async_write(&metadata);
}

bool disk_write::is_barrier_write() {
  return c_is_barrier_write(&metadata);
}

bool disk_write::is_meta() {
  return c_is_meta(&metadata);
}

disk_write::disk_write(const struct disk_write_op_meta& m,
    const char *d) {
  metadata = m;
  if (metadata.size > 0 && d != NULL) {
    data = shared_ptr<char>(new char[metadata.size],
        [](char* c) {delete[] c;});
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
    } else if (a.data.get() == NULL && b.data.get() == NULL) {
      return true;
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

void disk_write::serialize(std::ofstream& fs, const disk_write& dw) {
  ios prev_format(NULL);
  prev_format.copyfmt(fs);
  fs << std::hex;
  fs << dw.metadata.bi_flags << " "
    << dw.metadata.bi_rw << " "
    << dw.metadata.write_sector << " "
    << dw.metadata.size << " ";
  char *data = (char *) dw.data.get();
  for (unsigned int i = 0; i < dw.metadata.size; ++i) {
    // TODO(ashmrtn): Change to put?
    fs << *(data + i);
  }
  fs << endl;
  fs.copyfmt(prev_format);
}

// TODO(ashmrtn): Greatly refactor this so that it is much more flexible and
// complete. Think about removing whitespace between fields and just checking
// for newlines? But then what happens if your data is all newlines?
disk_write disk_write::deserialize(ifstream& is) {
  disk_write_op_meta meta;
  ios prev_format(NULL);
  prev_format.copyfmt(is);
  is >> std::skipws;
  is >> std::hex;
  is >> meta.bi_flags
    >> meta.bi_rw
    >> meta.write_sector
    >> meta.size;

  char nl;
  // Eat single space between size of data and data.
  is.get(nl);
  assert(nl == ' ');
  char *data = new char[meta.size];
  is.read(data, meta.size);
  // Make sure that the meta.size field matches the actual data size in the log.
  is.get(nl);
  assert(nl == '\n');
  is.copyfmt(prev_format);
  disk_write res(meta, data);
  delete[] data;
  return res;
}

/*
 * Output all data for a single object on a single line.
 */
ofstream& operator<<(ofstream& fs, const disk_write& dw) {
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
  return c_has_write_flag(&metadata);
}

shared_ptr<char> disk_write::set_data(const char *d) {
  if (metadata.size > 0 && d != NULL) {
    data = shared_ptr<char>(new char[metadata.size]);
    memcpy(data.get(), d, metadata.size);
  }
  return data;
}

shared_ptr<char> disk_write::get_data() {
  return data;
}

}  // namespace utils
}  // namespace fs_testing
