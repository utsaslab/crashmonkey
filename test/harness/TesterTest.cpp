#include "../../code/harness/Tester.h"
#include "TestTester.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace fs_testing {
namespace test {

using std::ifstream;

TEST(Tester, LoadSnapshotFromDisk) {
  TestTester t;

  const unsigned int disk_size = 8192;
  char disk_sn[disk_size];

  char *temp_file = strdup("/tmp/log_snapshotXXXXXX");
  int temp_fd = mkstemp(temp_file);
  EXPECT_TRUE(temp_fd > 0);
  close(temp_fd);

  for (unsigned int i = 0; i < disk_size; ++i) {
    disk_sn[i] = 42;
  }

  t.set_tester_snapshot(disk_sn, disk_size);
  EXPECT_EQ(SUCCESS, t.tester.log_snapshot_save(temp_file));

  ifstream log_file(temp_file);
  char read_data[disk_size];
  log_file.read(read_data, disk_size);
  log_file.close();
  EXPECT_EQ(EOF, log_file.peek());
  EXPECT_TRUE(std::equal(std::begin(disk_sn), std::end(disk_sn),
      std::begin(read_data)));
}

}  // namespace test
}  // namespace fs_testing
