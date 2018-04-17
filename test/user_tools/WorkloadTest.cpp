#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "../../code/user_tools/api/workload.h"

#include "gtest/gtest.h"




#include <iostream>

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
namespace test {

using std::string;

using fs_testing::user_tools::api::WriteData;

namespace {

// We want exactly 4k of data for this.
static const unsigned int kTestDataSize = 4096;
// 4K of data plus one terminating byte.
static constexpr char kTestDataBlock[kTestDataSize + 1] =
  REP(1, 2, 8, "abcdefghijklmnopqrstuvwxyz123456");

}  // namespace


/*
 * Test that a write aligned to a 4k boundary that is less than 4k will result
 * in the proper data written to the file.
 */
TEST(WorkloadTest, WriteLessThan4kAligned) {
  const string test_file = "test_file";
  const int write_size = 3 * 1024;
  const int fd = open(test_file.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
  unlink(test_file.c_str());
  ASSERT_GE(fd, 0);
  // So the file disappears after this test.
  ASSERT_GE(WriteData(fd, 0, write_size), 0);

  char data[write_size];
  unsigned int read_data = 0;
  while (read_data < write_size) {
    int res = read(fd, data + read_data, write_size - read_data);
    ASSERT_GE(res, 0);
    read_data += res;
  }
  char tmp;
  // Check that there's no more data in the file.
  EXPECT_EQ(read(fd, &tmp, 1), 0);

  close(fd);

  EXPECT_EQ(memcmp(data, kTestDataBlock, write_size), 0);
}

/*
 * Test that a write not aligned to a 4k boundary that is less than 4k will
 * result in the proper data written to the file.
 */
TEST(WorkloadTest, WriteLessThan4kUnaligned) {
  const string test_file = "test_file";
  const unsigned int write_size = 3 * 1024;
  const unsigned int offset = 1024;
  const int fd = open(test_file.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
  unlink(test_file.c_str());
  ASSERT_GE(fd, 0);
  // So the file disappears after this test.
  ASSERT_GE(WriteData(fd, offset, write_size), 0);

  // We read all the data here.
  const unsigned int file_size = 4096;
  char data[file_size];
  unsigned int read_data = 0;
  while (read_data < file_size) {
    int res = read(fd, data + read_data, file_size - read_data);
    ASSERT_GE(res, 0);
    read_data += res;
  }
  char tmp;
  // Check that there's no more data in the file.
  EXPECT_EQ(read(fd, &tmp, 1), 0);

  for (unsigned int i = 0; i < offset; ++i) {
    EXPECT_EQ(data[i], 0);
  }

  close(fd);

  EXPECT_EQ(memcmp(data + offset, kTestDataBlock, write_size), 0);
}

/*
 * Test that a write not aligned to a 4k boundary that is less than 4k will
 * result in the proper data written to the file.
 */
TEST(WorkloadTest, WriteLessThan4kUnaligned2) {
  const string test_file = "test_file";
  const unsigned int write_size = 3 * 1024;
  const unsigned int offset = 3;
  const int fd = open(test_file.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
  unlink(test_file.c_str());
  ASSERT_GE(fd, 0);
  // So the file disappears after this test.
  ASSERT_GE(WriteData(fd, offset, write_size), 0);

  // We read all the data here.
  const unsigned int file_size = offset + write_size;
  char data[file_size];
  unsigned int read_data = 0;
  while (read_data < file_size) {
    int res = read(fd, data + read_data, file_size - read_data);
    ASSERT_GE(res, 0);
    read_data += res;
  }
  char tmp;
  // Check that there's no more data in the file.
  EXPECT_EQ(read(fd, &tmp, 1), 0);

  for (unsigned int i = 0; i < offset; ++i) {
    EXPECT_EQ(data[i], 0);
  }
  for (unsigned int i = offset + write_size; i < file_size; ++i) {
    EXPECT_EQ(data[i], 0);
  }

  close(fd);

  EXPECT_EQ(memcmp(data + offset, kTestDataBlock + offset, write_size), 0);
}

}  // namespace test
}  // namespace fs_testing
