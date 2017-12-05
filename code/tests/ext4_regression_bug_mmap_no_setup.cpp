#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

namespace {
  static unsigned int kSleepSecs = 30;

  static constexpr char kTestFile[] = "/mnt/snapshot/test_file";
}

class Ext4RegressionBugMmapNoSetup: public BaseTestCase {
 public:
  virtual int setup() override {
    std::cout << "running with a delay of " << kSleepSecs << std::endl;
    const int fd = open(kTestFile, O_WRONLY | O_TRUNC | O_CREAT,
        TEST_FILE_PERMS);
    if (fd < 0) {
      return -1;
    }

    syncfs(fd);
    close(fd);

    return 0;
  }

  virtual int run() override {
    const int fd_reg = open(kTestFile, O_RDWR);
    if (fd_reg < 0) {
      return -1;
    }

    if (WriteData(fd_reg, 0x136dd, 0xdc69) < 0) {
      close(fd_reg);
      return -3;
    }

    if (fallocate(fd_reg, 0, 0xb531, 0xb5ad) < 0) {
      close(fd_reg);
      return -4;
    }

    if (fallocate(fd_reg, FALLOC_FL_COLLAPSE_RANGE, 0x1c000, 0x4000) < 0) {
      close(fd_reg);
      return -4;
    }
    sleep(kSleepSecs);

    if (WriteData(fd_reg, 0x3e5ec, 0xdc69) < 0) {
      close(fd_reg);
      return -3;
    }

    if (fallocate(fd_reg, FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE, 0x20fac,
        0x274f) < 0) {
      close(fd_reg);
      return -4;
    }

    if (WriteDataMmap(fd_reg, 0x216ad, 0x274f) < 0) {
      close(fd_reg);
      return -5;
    }

    close(fd_reg);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_res) override {
    return 0;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Ext4RegressionBugMmapNoSetup;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
