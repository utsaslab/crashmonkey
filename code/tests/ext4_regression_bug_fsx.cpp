#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <string>

#include "BaseTestCase.h"

using std::calloc;
using std::string;

using fs_testing::tests::DataTestResult;


// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

namespace {
  static constexpr char kFsCommandsName[] = "/tmp/cm_fs_ops.fsxops";
  static constexpr char kTestFile[] = "/mnt/snapshot/test_file";
  static constexpr char kTestCommands[] =
    "write 0x137dd 0xdc69 0x0\n"
    "fallocate 0xb531 0xb5ad 0x21446\n"
    "collapse_range 0x1c000 0x4000 0x21446\n"
    "write 0x3e5ec 0x1a14 0x21446\n"
    "zero_range 0x20fac 0x6d9c 0x40000 keep_size\n"
    "mapwrite 0x216ad 0x274f 0x40000";

}

class Ext4RegressionBugFsx: public BaseTestCase {
 public:
  virtual int setup() override {
    fs_commands_.open(kFsCommandsName);
    fs_commands_ << kTestCommands;
    fs_commands_.close();

    return 0;
  }

  virtual int run() override {
    string xfs_command("~/xfstests/ltp/fsx -d --replay-ops ");
    xfs_command += kFsCommandsName;
    xfs_command += " ";
    xfs_command += kTestFile;
    std::cout << xfs_command << std::endl;
    return system(xfs_command.c_str());

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_res) override {
    return 0;
  }

 private:
  std::ofstream fs_commands_;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Ext4RegressionBugFsx;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
