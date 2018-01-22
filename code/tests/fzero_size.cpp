#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

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

    //1. Write 0-428K
    if (WriteData(fd, 0, 438272) < 0) {
      close(fd);
      return -2;
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

    fsync(fd_reg);
    if (Checkpoint() < 0){
      return -2;
    }

    //To ensure checkpoint 1 is in a seperate epoch, force a flush
    syncfs(fd_reg);

    //Falloc keep size 428K - 432K
    if (fallocate(fd_reg, FALLOC_FL_KEEP_SIZE, 438272,
        4096) < 0) {
      close(fd_reg);
      return -3;
    }
    sleep(6);


    if (Checkpoint() < 0){
      return -4;
    }

    //Falloc zero range 0-430K
    if (fallocate(fd_reg, FALLOC_FL_ZERO_RANGE, 0,
        440320) < 0) {
      std::cout << "Falloc error zero range" << std::endl;
      close(fd_reg);
      return -5;
    }

    
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
    //std::cout << "FS block size = "<< stats.st_blksize << std::endl;
    //std::cout << "Inode num = "<< stats.st_ino << std::endl;

    //After fdatasync call, if you crash at any point, and on recovery
    //if blocks != 32, EOF blocks are missing.
    if (last_checkpoint == 2 && stats.st_size != 440320){
    	test_result->SetError(DataTestResult::kFileMetadataCorrupted);
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
