/*
Reproducing fstest generic/342

1. Create dir A in TEST_MNT
2. Create file foo in dir A and write 0-16K into it
3. Sync everything
4. Rename file foo to bar in the same dir A
5. Write 4K into file foo now
6. fsync (foo)


If we power fail and mount the filesystem, we should see file foo with 4K data and file bar with
16K data.

Fails on f2fs (kernel 4.15) https://patchwork.kernel.org/patch/10135085/

The file bar, which is the renamed of foo is lost, while foo is rewritten to have the new contents.


*/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"
#define FOO_OLD_SIZE 16384
#define FOO_NEW_SIZE 4096


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic342: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test dirA
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }

    // Create foo in dirA
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -2;
    }

    //Write data into this file.
    if (WriteData(fd_foo, 0, FOO_OLD_SIZE) < 0) {
      close(fd_foo);
      return -3;
    }

    //Sync everything
    sync();

    close(fd_foo);

    return 0;
  }

  virtual int run(int checkpoint) override {

    int local_checkpoint = 0;
    //Rename foo to bar
    if (rename(foo_path.c_str(), bar_path.c_str()) < 0) {
      return -1;
    }

    // Open file foo
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -2;
    }

    //Write 4K data into file foo.
    if (WriteData(fd_foo, 0, FOO_NEW_SIZE) < 0) {
      close(fd_foo);
      return -3;
    }

    //fsync file foo
    int res = fsync(fd_foo);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
    }
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    //Close open files  
    close(fd_foo);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    /*std::cout << "Dir A :" << std::endl;
    system("ls /mnt/snapshot/test_dir_a");*/


    struct stat stats_old;
    struct stat stats_new;
    const int stat_foo = stat(foo_path.c_str(), &stats_old);
    const int errno_foo = errno;
    const int stat_bar = stat(bar_path.c_str(), &stats_new);
    const int errno_bar = errno;

    // Neither stat found the file, it's gone...
    if (stat_foo < 0 && errno_foo == ENOENT &&
        stat_bar < 0 && errno_bar == ENOENT) {
      test_result->SetError(DataTestResult::kFileMissing); 
      test_result->error_description = " : " + foo_path + " and " + bar_path+  " missing";
      }

    // We renamed foo-> bar and created a new file foo. So old file foo's contents should be 
    // present in file bar. Else we have lost data present in old file foo during
    // rename.

    //We have lost file bar(contents of old foo)
    if(last_checkpoint ==1 && stat_bar < 0 && errno_bar == ENOENT && stats_old.st_size != FOO_OLD_SIZE){
      test_result->SetError(DataTestResult::kFileMissing); 
      test_result->error_description = " : " + foo_path + " has new data " + bar_path + " missing";
    }

    //if bar is present, verify bar is the old foo 
    if(stat_bar == 0 && (stats_old.st_size != FOO_NEW_SIZE || stats_new.st_size != FOO_OLD_SIZE)){
      test_result->SetError(DataTestResult::kFileDataCorrupted); 
      test_result->error_description = " : " + foo_path + " and " +bar_path + " has incorrect data";
    }

 

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;
    const string bar_path = TEST_MNT "/" TEST_DIR_A  "/" TEST_FILE_BAR;
};
}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic342;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
