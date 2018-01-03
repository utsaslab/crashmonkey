#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

namespace {
  static constexpr char kTestFile[] = "/mnt/snapshot/test_file";
}

class EOFBlocksLoss: public BaseTestCase {
 public:
  virtual int setup() override {
    const int fd = open(kTestFile, O_WRONLY | O_TRUNC | O_CREAT,
        TEST_FILE_PERMS);
    if (fd < 0) {
      return -1;
    }

    if (WriteData(fd, 8192, 8192) < 0) {
      close(fd);
      return -3;
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

    if (fallocate(fd_reg, FALLOC_FL_KEEP_SIZE, 4202496,
        8192) < 0) {
      close(fd_reg);
      return -4;
    }
    
    fdatasync(fd_reg);
    
    close(fd_reg);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    struct stat stats;
    int res = stat(TEST_MNT "/" TEST_FILE, &stats);
    if (res < 0) {
      test_result->SetError(DataTestResult::kFileMissing);
      return 0;
    }

    std::cout << "Stat of the file :" << std::endl;
    std::cout << "Size = " << stats.st_size << std::endl;
    std::cout << "Block count = " << stats.st_blocks << std::endl;
    std::cout << "FS block size = "<< stats.st_blksize << std::endl;
    std::cout << "Inode num = "<< stats.st_ino << std::endl;

    //After fdatasync call, if you crash at any point, and on recovery
    //if blocks != 32, EOF blocks are missing.
    if (last_checkpoint == 1 && stats.st_blocks != 32){
    	test_result->SetError(DataTestResult::kIncorrectBlockCount);
    	return 0;
    }


    return 0;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::EOFBlocksLoss;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
