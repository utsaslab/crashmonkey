// Hack to allow us to determine different bio flags based on kernel code. This
// mus now be compiled with kernel headers.
#include <linux/blk_types.h>

#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <ios>
#include <iostream>
#include <vector>

#include "../../code/utils/utils.h"
#include "gtest/gtest.h"

namespace fs_testing {
namespace test {
using std::endl;;
using std::ifstream;
using std::ofstream;
using std::vector;

using fs_testing::utils::disk_write;

TEST(DiskWrite, Serialize_Deserialize) {
  disk_write test_write;

  test_write.metadata.write_sector = 50;
  test_write.metadata.size = 8192;
  test_write.metadata.bi_flags = REQ_WRITE;
  test_write.metadata.bi_rw = REQ_WRITE;
  test_write.metadata.bi_flags |= REQ_SYNC;
  test_write.metadata.bi_rw |= REQ_SYNC;
  char data[test_write.metadata.size];
  for (unsigned int i = 0; i < test_write.metadata.size; ++i) {
    data[i] = 0x20;
  }
  test_write.set_data(data);

  // Get a temp file to write and read to/from.
  char *temp_file = strdup("/tmp/disk_write_serializeXXXXXX");
  int temp_fd = mkstemp(temp_file);
  EXPECT_TRUE(temp_fd > 0);

  ofstream output(temp_file);
  close(temp_fd);

  output << std::hex;
  disk_write::serialize(output, test_write);
  output << std::dec;
  output << endl;
  output.close();

  ifstream input(temp_file);
  free(temp_file);
  input >> std::hex;
  disk_write read = disk_write::deserialize(input);
  input >> std::dec;
  input.close();

  EXPECT_EQ(test_write.metadata.write_sector, read.metadata.write_sector);
  EXPECT_EQ(test_write.metadata.size, read.metadata.size);
  EXPECT_EQ(test_write.metadata.bi_flags, read.metadata.bi_flags);
  EXPECT_EQ(test_write.metadata.bi_rw, read.metadata.bi_rw);
  EXPECT_EQ(0,
      memcmp(test_write.get_data().get(), read.get_data().get(),
          test_write.metadata.size));
}

TEST(DiskWrite, Serialize_Deserialize_Epoch) {
  vector<disk_write> epoch;
  disk_write test_write;

  test_write.metadata.write_sector = 50;
  test_write.metadata.size = 8192;
  test_write.metadata.bi_flags = REQ_WRITE;
  test_write.metadata.bi_rw = REQ_WRITE;
  test_write.metadata.bi_flags |= REQ_SYNC;
  test_write.metadata.bi_rw |= REQ_SYNC;
  char data[test_write.metadata.size];
  for (unsigned int i = 0; i < test_write.metadata.size; ++i) {
    data[i] = 0x20;
  }
  test_write.set_data(data);
  epoch.push_back(test_write);

  test_write.metadata.write_sector = 50;
  test_write.metadata.size = 16384;
  test_write.metadata.bi_flags = REQ_WRITE;
  test_write.metadata.bi_rw = REQ_WRITE;
  test_write.metadata.bi_flags |= REQ_SYNC;
  test_write.metadata.bi_rw |= REQ_SYNC;
  char data2[test_write.metadata.size];
  for (unsigned int i = 0; i < test_write.metadata.size; ++i) {
    data2[i] = 0x0A;
  }
  test_write.set_data(data2);
  epoch.push_back(test_write);

  // Get a temp file to write and read to/from.
  char *temp_file = strdup("/tmp/disk_write_serializeXXXXXX");
  int temp_fd = mkstemp(temp_file);
  EXPECT_TRUE(temp_fd > 0);

  ofstream output(temp_file);
  close(temp_fd);

  output << std::hex;
  for (const auto& write : epoch) {
    disk_write::serialize(output, write);
  }
  output << std::dec;
  output.close();

  vector<disk_write> read;
  ifstream input(temp_file);
  free(temp_file);

  input >> std::hex;

  while (input.peek() != EOF) {
    read.push_back(disk_write::deserialize(input));
    std::cout << "next character is 0x" << std::hex << input.peek() << std::dec << std::endl;
  }

  input >> std::dec;
  input.close();

  for (unsigned int i = 0; i < epoch.size(); ++i) {
    EXPECT_EQ(epoch.at(i).metadata.write_sector,
        read.at(i).metadata.write_sector);
    EXPECT_EQ(epoch.at(i).metadata.size, read.at(i).metadata.size);
    EXPECT_EQ(epoch.at(i).metadata.bi_flags, read.at(i).metadata.bi_flags);
    EXPECT_EQ(epoch.at(i).metadata.bi_rw, read.at(i).metadata.bi_rw);
    EXPECT_EQ(0,
        memcmp(epoch.at(i).get_data().get(), read.at(i).get_data().get(),
            epoch.at(i).metadata.size));
  }
}

}  // namespace test
}  // namespace fs_testing
