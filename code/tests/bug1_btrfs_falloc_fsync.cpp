/*

NEW BUG #1 (Acknowledged, but not yet patched)

1. Create file foo
2. Write data (offset 0 -16K) into the file foo
3. fsync(foo) -> fsync-1
4. sync()
5. Fallocate a file with keep size, that extends the file (16K-20K)
6. fsync(foo) -> fsync-2


If a power fail occurs now, and remount the filesystem, 
the contents of file foo must match the state after fsync-2

This is tested to fail on btrfs(latest kernel 4.16) - Acknowledged here :
https://www.spinics.net/lists/linux-btrfs/msg75108.html

File foo contents match the state after fsync-1. Blocks allocated by 
falloc are lost, even though there is a fsync after the operation.
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <errno.h>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_MNT "/mnt/snapshot"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class BtrfsFsyncFalloc: public BaseTestCase {
 public:
  virtual int setup() override {

    //Create file foo 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //Write 16KB of data to the file
    if(WriteData(fd_foo, 0, 16384) < 0){
      close(fd_foo);
      return -2;
    }

    //Fsync the file
    if(fsync(fd_foo) < 0){
      close(fd_foo);
      return -3;
    }


    //sync to perist everything so far
    sync();

    //close file foo
    close(fd_foo);

    return 0;
  }

  virtual int run() override {


    //Open file foo
    const int fd_foo = open(foo_path.c_str(), O_RDWR);
    if (fd_foo < 0) {
      return -1;
    }

    // system("stat /mnt/snapshot/foo");


    /*-------------------Variant 1 ---------------------------*/
    //unaligned offset and size less than  a page
    //Fallocate from off 17000 of length 3000
    if(fallocate(fd_foo, FALLOC_FL_KEEP_SIZE ,17000, 3000) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    int res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 1 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }

    //Expected output if checkpoint = 1 : Size = 16K, blocks == 40  (32 + 8)
    // system("stat /mnt/snapshot/foo");



    /*-------------------Variant 2 ---------------------------*/
    //aligned offset and size less than  a page
    //Fallocate from off 24K of length 3000
    if(fallocate(fd_foo, FALLOC_FL_KEEP_SIZE ,24576, 3000) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 2 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }
    //Expected output if checkpoint = 2: size = 16K, blocks == 48 (40 + 8)
    // system("stat /mnt/snapshot/foo");


    /*-------------------Variant 3 ---------------------------*/
    //Aligned offset and size > a page
    //Fallocate from off 32K of length 8192
    if(fallocate(fd_foo, FALLOC_FL_KEEP_SIZE , 32768, 8192) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 3 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }
    //Expected output if checkpoint = 3: size = 16K, blocks == 64 (48 + 16)
    // system("stat /mnt/snapshot/foo");


    /*-------------------Variant 4 ---------------------------*/
    //unaligned offset and size less than  a page
    //Fzero from off 42000 of length 3000
    if(fallocate(fd_foo, FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE ,42000, 3000) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 4 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }

    //Expected output if checkpoint = 4 : Size = 16K, blocks == 72  (64 + 8)
    // system("stat /mnt/snapshot/foo");



    /*-------------------Variant 5 ---------------------------*/
    //aligned offset and size less than  a page
    //Fzero from off 48K of length 3000
    if(fallocate(fd_foo, FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE ,49152, 3000) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 5 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }
    //Expected output if checkpoint = 5: size = 16K, blocks == 80 (72 + 8)
    // system("stat /mnt/snapshot/foo");


    /*-------------------Variant 6 ---------------------------*/
    //Aligned offset and size > a page
    //Fzero from off 52K of length 8192
    if(fallocate(fd_foo, FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE , 53248, 8192) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 6 beyond this point
    // Beyond this point, the effect of falloc must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }
    //Expected output if checkpoint = 6: size = 16K, blocks == 96 (80 + 16)
    // system("stat /mnt/snapshot/foo");






    //Close open files  
    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    // system("stat /mnt/snapshot/foo");


    struct stat stats;
    int res = stat(TEST_MNT "/" TEST_FILE_FOO, &stats);
    if (res < 0) {
      test_result->SetError(DataTestResult::kFileMissing);
      return 0;
    }

    //16K writes 32 blocks. If we have seen checkpoint 1, then falloc/fzero succeeded
    // and block count should have increased. If it hasn't, then we have lost these eof blocks

    if(last_checkpoint == 1 && stats.st_blocks < blocks_chk1){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Falloc -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have"
         + std::to_string(blocks_chk1) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    if(last_checkpoint == 2 && stats.st_blocks < blocks_chk2){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Falloc -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have "
         + std::to_string(blocks_chk2) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    if(last_checkpoint == 3 && stats.st_blocks < blocks_chk3){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Falloc -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have "
         + std::to_string(blocks_chk3) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    if(last_checkpoint == 4 && stats.st_blocks < blocks_chk4){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Fzero -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have "
         + std::to_string(blocks_chk4) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    if(last_checkpoint == 5 && stats.st_blocks < blocks_chk5){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Fzero -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have "
         + std::to_string(blocks_chk5) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    if(last_checkpoint == 6 && stats.st_blocks < blocks_chk6){
      test_result->SetError(DataTestResult::kIncorrectBlockCount);
      test_result->error_description =
        " : Fzero -k : Chk = " + std::to_string(last_checkpoint) + " : Expected file to have "
         + std::to_string(blocks_chk6) + " blocks but found " +
        std::to_string(stats.st_blocks);
      return 0;
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_FILE_FOO;
    //these are block counts on ext4. On btrfs, f2fs, xfs they are (40, 48, 64, 72, 80, 96)
    const int blocks_chk1 = 40;
    const int blocks_chk2 = 46;
    const int blocks_chk3 = 62;
    const int blocks_chk4 = 70;
    const int blocks_chk5 = 76;
    const int blocks_chk6 = 92;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::BtrfsFsyncFalloc;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
