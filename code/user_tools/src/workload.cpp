#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstring>

#include "../api/workload.h"


// Super ugly defines to do compile-time string concatonation X times...
#define REP0(x)
#define REP1(x)     x
#define REP2(x)     REP1(x) x
#define REP3(x)     REP2(x) x
#define REP4(x)     REP3(x) x
#define REP5(x)     REP4(x) x
#define REP6(x)     REP5(x) x
#define REP7(x)     REP6(x) x
#define REP8(x)     REP7(x) x
#define REP9(x)     REP8(x) x
#define REP10(x)    REP9(x) x

#define REP(hundreds, tens, ones, x) \
  REP##hundreds(REP10(REP10(x))) \
  REP##tens(REP10(x)) \
  REP##ones(x)

namespace fs_testing {
namespace user_tools {
namespace api {

namespace {

// We want exactly 4k of data for this.
static const unsigned int kTestDataSize = 4096;
// 4K of data plus one terminating byte.
static constexpr char kTestDataBlock[kTestDataSize + 1] =
  REP(1, 2, 8, "abcdefghijklmnopqrstuvwxyz123456");

}  // namespace

int WriteData(int fd, unsigned int offset, unsigned int size) {
  // Offset into a data block to start working at.
  const unsigned int rounded_offset =
    (offset + (kTestDataSize - 1)) & (~(kTestDataSize - 1));
  // Round down size to 4k for number of full pages to write.
  
  const unsigned int aligned_size = (size >= kTestDataSize) ?
    (size - (rounded_offset - offset)) & ~(kTestDataSize - 1) :
    0;
  unsigned int num_written = 0;

  // The start of the write range is not aligned with our data blocks.
  // Therefore, we should write out part of a data block for this segment,
  // with the first character in the data block aligning with the data block
  // boundary.
  if (rounded_offset != offset) {
    // We should never write more than kTestDataSize of unaligned data at the
    // start.
    const unsigned int to_write = (size < rounded_offset - offset) ?
      size : rounded_offset - offset;
    while (num_written < to_write){
      const unsigned int mod_offset =
        (num_written + offset) & (kTestDataSize - 1);
      assert(mod_offset < kTestDataSize);

      int res = pwrite(fd, kTestDataBlock + mod_offset, to_write - num_written,
          offset + num_written);
      if (res < 0) {
        return res;
      }
      num_written += res;
    }
  }

  // Write out the required number of full pages for this request. The first
  // byte will be aligned with kTestDataSize.
  unsigned int aligned_written = 0;
  while (aligned_written < aligned_size) {
    const unsigned int mod_offset = (aligned_written & (kTestDataSize - 1));
    // Write up to a full page of data at a time.
    int res = pwrite(fd, kTestDataBlock + mod_offset,
        kTestDataSize - mod_offset, offset + num_written);
    if (res < 0) {
      return res;
    }
    num_written += res;
    aligned_written += res;
  } 

  if (num_written == size) {
    return 0;
  }

  // Write out the last partial page of data. The first byte will be aligned
  // with kTestDataSize.
  unsigned int end_written = 0;
  while (num_written < size) {
    assert(end_written < kTestDataSize);
    const unsigned int mod_offset = (end_written & (kTestDataSize - 1));
    int res = pwrite(fd, kTestDataBlock + mod_offset,
        size - num_written, offset + num_written);
    if (res < 0) {
      return res;
    }
    num_written += res;
    end_written += res;
  } 

  return 0;
}

int WriteDataMmap(int fd, unsigned int offset, unsigned int size) {
  const unsigned int map_size = size + (offset & ((1 << 12) - 1));
  char *filep = (char *) mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED,
      fd, offset & ~((1 << 12) - 1));
  if (filep == MAP_FAILED) {
    return -1;
  }

  // Offset into a data block to start working at.
  const unsigned int rounded_offset_diff =
    ((offset + (kTestDataSize - 1)) & (~(kTestDataSize - 1))) - offset;

  // The start of the write range is not aligned with our data blocks.
  // Therefore, we should write out part of a data block for this segment,
  // with the first character in the data block aligning with the data block
  // boundary.
  if (rounded_offset_diff != 0) {
    const char *file_start_offset = filep + (offset & ((1 << 12) - 1));
    const unsigned int test_data_offset = offset & (kTestDataSize - 1);
    memcpy((char *) file_start_offset, kTestDataBlock + test_data_offset,
        kTestDataSize - test_data_offset);
  }

  // Write out the rest of the data for this request. All writes from here out
  // will have their first byte aligned with kTestDataSize.
  unsigned int num_written = 0;
  while (num_written != size - rounded_offset_diff) {
    const unsigned int to_write =
      (num_written + kTestDataSize > size - rounded_offset_diff)
      ? size - rounded_offset_diff - num_written
      : kTestDataSize;
    memcpy(filep + rounded_offset_diff + num_written, kTestDataBlock,
        to_write);
    num_written += to_write;
  }

  if (msync(filep, map_size, MS_SYNC) < 0) {
    munmap(filep, map_size);
    return -1;
  }

  munmap(filep, map_size);
  return 0;
}

} // fs_testing
} // user_tools
} // api
