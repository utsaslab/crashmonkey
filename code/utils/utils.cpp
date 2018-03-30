#include <endian.h>

#include <cassert>
#include <cstdint>
#include <cstring>

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "utils.h"

namespace fs_testing {
namespace utils {
using std::endl;
using std::ifstream;
using std::ios;
using std::ios_base;
using std::memcpy;
using std::mt19937;
using std::ofstream;
using std::ostream;
using std::pair;
using std::shared_ptr;
using std::tie;
using std::uniform_int_distribution;
using std::vector;

namespace {

const unsigned int kSerializeBufSize = 4096;

static const char* const flag_names[] = {
  "write", "fail fast dev", "fail fast transport", "fail fast driver", "sync",
  "meta", "prio", "discard", "secure", "write same", "no idle", "fua",
  "flush", "read ahead", "throttled", "sorted", "soft barrier", "no merge",
  "started", "don't prep", "queued", "elv priv", "failed", "quiet", "preempt",
  "alloced", "copy user", "flush seq", "io stat", "mixed merge", "pm",
  "hashed", "mq_inflight", "no timeout", "op write zeroes", "nr bits"
};

static char const checkpoint_name[] = "checkpoint";

}

bool disk_write::is_async_write() {
  return !((metadata.bi_rw & HWM_SYNC_FLAG) ||
            (metadata.bi_rw & HWM_FUA_FLAG) ||
            (metadata.bi_rw & HWM_FLUSH_FLAG) ||
            (metadata.bi_rw & HWM_FLUSH_SEQ_FLAG) ||
            (metadata.bi_rw & HWM_SOFTBARRIER_FLAG)) &&
            (metadata.bi_rw & HWM_WRITE_FLAG);
}

bool disk_write::is_barrier() {
  return !!(metadata.bi_rw & HWM_FUA_FLAG) ||
          (metadata.bi_rw & HWM_FLUSH_FLAG) ||
          (metadata.bi_rw & HWM_FLUSH_SEQ_FLAG) ||
          (metadata.bi_rw & HWM_SOFTBARRIER_FLAG);
}

bool disk_write::is_meta() {
  return !!(metadata.bi_rw & HWM_META_FLAG);
}

bool disk_write::is_checkpoint() {
  return !!(metadata.bi_rw & HWM_CHECKPOINT_FLAG);
}

disk_write::disk_write() {
  // Apparently contained structs aren't set to 0 on default initialization
  // unless their constructors are defined too. But, we have a struct that is
  // shared between C and C++ so we can't define an constructor easily.
  metadata.bi_flags = 0;
  metadata.bi_rw = 0;
  metadata.write_sector = 0;
  metadata.size = 0;
  metadata.time_ns = 0;
  data.reset();
}

disk_write::disk_write(const struct disk_write_op_meta& m,
    const char *d) {
  metadata = m;
  if (metadata.size > 0 && d != NULL) {
    data.reset(new char[metadata.size], [](char* c) {delete[] c;});
    memcpy(data.get(), d, metadata.size);
  }
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

// Assumes binary file stream provided.
void disk_write::serialize(std::ofstream& fs, const disk_write& dw) {
  char buffer[kSerializeBufSize];
  memset(buffer, 0, kSerializeBufSize);
  // Working all in big endian...

  // Write out the metadata for this log entry.
  unsigned int buf_offset = 0;
  const uint64_t write_flags = htobe64(dw.metadata.bi_flags);
  const uint64_t write_rw = htobe64(dw.metadata.bi_rw);
  const uint64_t write_write_sector = htobe64(dw.metadata.write_sector);
  const uint64_t write_size = htobe64(dw.metadata.size);
  const uint64_t time_ns = htobe64(dw.metadata.time_ns);
  memcpy(buffer + buf_offset, &write_flags, sizeof(const uint64_t));
  buf_offset += sizeof(const uint64_t);
  memcpy(buffer + buf_offset, &write_rw, sizeof(const uint64_t));
  buf_offset += sizeof(const uint64_t);
  memcpy(buffer + buf_offset, &write_write_sector, sizeof(const uint64_t));
  buf_offset += sizeof(const uint64_t);
  memcpy(buffer + buf_offset, &write_size, sizeof(const uint64_t));
  buf_offset += sizeof(const uint64_t);
  memcpy(buffer + buf_offset, &time_ns, sizeof(const uint64_t));
  buf_offset += sizeof(const uint64_t);

  // Write out the 4K buffer containing metadata for this log entry
  fs.write(buffer, kSerializeBufSize);
  if (!fs.good()) {
    std::cerr << "some error writing to file" << std::endl;
    return;
  }

  // Write out the actual data for this log entry. Data could be larger than
  // buf_size so loop through this.
  const char *data = (char *) dw.data.get();
  for (unsigned int i = 0; i < dw.metadata.size; i += kSerializeBufSize) {
    const unsigned int copy_amount =
      ((i + kSerializeBufSize) > dw.metadata.size)
        ? (dw.metadata.size - i)
        : kSerializeBufSize;
    // Not strictly needed, but it makes it easier.
    memset(buffer, 0, kSerializeBufSize);
    memcpy(buffer, data + i, copy_amount);
    fs.write(buffer, kSerializeBufSize);
    if (!fs.good()) {
      std::cerr << "some error writing to file" << std::endl;
      return;
    }
  }
}

// Assumes binary file stream provided.
disk_write disk_write::deserialize(ifstream& is) {
  char buffer[kSerializeBufSize];
  memset(buffer, 0, kSerializeBufSize);

  // TODO(ashmrtn): Make this read only the size of the required data. This
  // means we should probably slightly restructure some of the structs we use so
  // we can read from the buffer into struct fields.
  is.read(buffer, kSerializeBufSize);
  // check if read was successful
  assert(is);

  unsigned int buf_offset = 0;
  uint64_t write_flags, write_rw, write_write_sector, write_size, time_ns;
  memcpy(&write_flags, buffer + buf_offset, sizeof(uint64_t));
  buf_offset += sizeof(uint64_t);
  memcpy(&write_rw, buffer + buf_offset, sizeof(uint64_t));
  buf_offset += sizeof(uint64_t);
  memcpy(&write_write_sector, buffer + buf_offset, sizeof(uint64_t));
  buf_offset += sizeof(uint64_t);
  memcpy(&write_size, buffer + buf_offset, sizeof(uint64_t));
  buf_offset += sizeof(uint64_t);
  memcpy(&time_ns, buffer + buf_offset, sizeof(uint64_t));
  buf_offset += sizeof(uint64_t);

  disk_write_op_meta meta;

  meta.bi_flags = be64toh(write_flags);
  meta.bi_rw = be64toh(write_rw);
  meta.write_sector = be64toh(write_write_sector);
  meta.size = be64toh(write_size);
  meta.time_ns = be64toh(time_ns);

  char *data = new char[meta.size];
  for (unsigned int i = 0; i < meta.size; i += kSerializeBufSize) {
    const unsigned int read_amount =
      ((i + kSerializeBufSize) > meta.size)
        ? (meta.size - i)
        : kSerializeBufSize;
    is.read(buffer, kSerializeBufSize);
    // check if read was successful
    assert(is);
    memcpy(data + i, buffer, read_amount);
  }

  disk_write res(meta, data);
  delete[] data;
  return res;
}

std::string disk_write::flags_to_string(long long flags) {
  std::string res;

  if (flags & HWM_CHECKPOINT_FLAG) {
    res += checkpoint_name;
  }

  for (unsigned int i = REQ_WRITE_; i < REQ_NR_BITS_; i++) {
    if (flags & (1ULL << i)) {
      res = res + flag_names[i] + ", ";
    }
  }

  return res;
}

ostream& operator<<(ostream& os, const disk_write& dw) {
  os << std::dec << std::setw(18) << std::fixed <<
    ((double) dw.metadata.time_ns) / 100000000 <<
    " " << std::setw(18) << std::hex << std::showbase <<
      dw.metadata.write_sector <<
    " " << std::setw(18) << dw.metadata.size << std::endl <<
    '\t' << "flags " << std::setw(18) << dw.metadata.bi_rw << std::noshowbase
      << std::dec << ": " << disk_write::flags_to_string(dw.metadata.bi_rw) <<
      endl;
  return os;
}

bool disk_write::has_write_flag() {
  return !!(metadata.bi_rw & HWM_WRITE_FLAG);
}

bool disk_write::has_flush_flag() {
  return !!(metadata.bi_rw & HWM_FLUSH_FLAG);
}

bool disk_write::has_flush_seq_flag() {
  return !!(metadata.bi_rw & HWM_FLUSH_SEQ_FLAG);
}

bool disk_write::has_FUA_flag() {
  return !!(metadata.bi_rw & HWM_FUA_FLAG);
}

void disk_write::set_flush_flag() {
  metadata.bi_rw = (metadata.bi_rw | HWM_FLUSH_FLAG);
}

void disk_write::set_flush_seq_flag() {
 metadata.bi_rw = (metadata.bi_rw | HWM_FLUSH_SEQ_FLAG);
}

void disk_write::clear_flush_flag() {
  metadata.bi_rw = (metadata.bi_rw & ~(HWM_FLUSH_FLAG));
}

void disk_write::clear_flush_seq_flag() {
  metadata.bi_rw = (metadata.bi_rw & ~(HWM_FLUSH_SEQ_FLAG));
}

shared_ptr<char> disk_write::set_data(const char *d) {
  if (metadata.size > 0 && d != NULL) {
    data.reset(new char[metadata.size], [](char* c) {delete[] c;});
    memcpy(data.get(), d, metadata.size);
  }
  return data;
}

shared_ptr<char> disk_write::get_data() {
  return data;
}

void disk_write::clear_data() {
  data.reset();
}


DiskWriteData::DiskWriteData() :
      full_bio(false), bio_index(0), bio_sector_index(0), disk_offset(0),
      size(0), data_offset_(0) {
  data_base_.reset();
}

DiskWriteData::DiskWriteData(bool full_bio, unsigned int bio_index,
    unsigned int bio_sector_index ,unsigned int disk_offset,
    unsigned int size, std::shared_ptr<char> data_base,
    unsigned int data_offset) :
      full_bio(full_bio), bio_index(bio_index),
      bio_sector_index(bio_sector_index), disk_offset(disk_offset),
      size(size), data_offset_(data_offset) {
  data_base_ = data_base;
}

void * DiskWriteData::GetData() {
  return (void*) (data_base_.get() + data_offset_);
}

}  // namespace utils
}  // namespace fs_testing
