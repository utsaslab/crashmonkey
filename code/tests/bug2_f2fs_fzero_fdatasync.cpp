/* 

NEW_BUG #2 (Acknowledged and patched)

If we do a falloc_zero_range with keep_size and then crash, the recovered file size should be
the value before crash.

Eg, consider the workload:
1. create foo
2. Write (8K - 16K) -> foo size = 16K now
3. fsync()
4. falloc zero_range , keep_size (4202496 - 4210688) -> foo_size must be 16K
5. fdatasync()
Crash 

In f2fs, if we recover after a crash, we see the file size to be 4210688 and not 16K

https://sourceforge.net/p/linux-f2fs/mailman/message/36236482/ 
https://patchwork.kernel.org/patch/10240977/ 

*/


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>

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

class F2fsFzero: public BaseTestCase {
 public:
  virtual int setup() override {
    const int fd = open(kTestFile, O_WRONLY | O_TRUNC | O_CREAT,
        TEST_FILE_PERMS);
    if (fd < 0) {
      return -1;
    }

    // Write 8K data to the file
    if (WriteData(fd, 8192, 8192) < 0) {
      close(fd);
      return -2;
    }

    //Persist the write
    syncfs(fd);
    close(fd);

    return 0;
  }

  virtual int run() override {
    const int fd_reg = open(kTestFile, O_RDWR);
    if (fd_reg < 0) {
      return -1;
    }

    system("stat /mnt/snapshot/test_file");

    // Fallocate (zero-range, keep_size) at offset 4MB+8K, length =8K
    // This should increase block count, but not the size
    if (fallocate(fd_reg, FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE, 4202496,
        8192) < 0) {
      close(fd_reg);
      return -2;
    }

    //Now do a fdatasync. This should atleast flush size and block count
    if (fdatasync(fd_reg) < 0){
      close(fd_reg);
      return -3;
    }

    //Beyond this checkpoint, block count must be >16, but size = 16K
    if (Checkpoint() < 0){
      return -4;
    }

    system("stat /mnt/snapshot/test_file");
    
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

    /*
    std::cout << "Size expected = " << size << std::endl;
    std::cout << "Stat of the file :" << std::endl;
    std::cout << "Size = " << stats.st_size << std::endl;
    std::cout << "Block count = " << stats.st_blocks << std::endl;
    std::cout << "FS block size = "<< stats.st_blksize << std::endl;
    std::cout << "Inode num = "<< stats.st_ino << std::endl;
    */

    //After fdatasync call, if you crash at any point, and on recovery
    //if file_size != 16K (we used keep_size option), or if block count isn't
    // incremented, then its a bug
    if (last_checkpoint == 1 && (stats.st_size != size || stats.st_blocks == blocks)) {
      if(stats.st_blocks == blocks) {
          test_result->SetError(DataTestResult::kIncorrectBlockCount);
          test_result->error_description = "Expected file to have 32 blocks but found " +
        std::to_string(stats.st_blocks);
      }
      else {
          test_result->SetError(DataTestResult::kFileMetadataCorrupted);
          test_result->error_description = ": Expected file size to be " + std::to_string(size) + " but found " +
        std::to_string(stats.st_size);
      }
    	return 0;
    }


    return 0;
  }

private:
  int size = 16384;
  int blocks = 16;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::F2fsFzero;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
