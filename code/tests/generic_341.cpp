/*
Reproducing fstest generic/341

1. Create dir A in TEST_MNT
2. Create dir X and Y in dir A
3. Create files foo and bar in dir X, write data and sync everything
4. Move dir X to dir Y
5. Create dir X again
6. fsync(dir X)

If we power fail and mount the filesystem, we should see an empty dir X
and dir Y with files foo and bar and their contents intact

This is tested to fail on btrfs (kernel 4.4) (https://patchwork.kernel.org/patch/8694291/)
(https://www.spinics.net/lists/linux-btrfs/msg53591.html).

In btrfs, the new directory Y and all of its contents - file foo and bar are lost.
The files are now neither present in X nor Y
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
#define TEST_DIR_X "test_dir_x"
#define TEST_DIR_Y "test_dir_y"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic341: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test dirA/dirX.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }
    res = mkdir(dir_x_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    // Create foo in dirA/dirX
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -2;
    }

    //Write data into this file.
    if (WriteData(fd_foo, 0, 8192) < 0) {
      close(fd_foo);
      return -3;
    }

    // Create bar in dirA/dirX
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -4;
    }

    //Write data into this file.
    if (WriteData(fd_bar, 0, 8192) < 0) {
      close(fd_bar);
      return -5;
    }

    //Sync everything
    sync();

    close(fd_foo);
    close(fd_bar);

    return 0;
  }

  virtual int run() override {

    //Rename dir x to dir y
    if (rename(dir_x_path.c_str(), dir_y_path.c_str()) < 0) {
      return -1;
    }

    // Create test dirA/dirX.
    int res = mkdir(dir_x_path.c_str(), 0777);
    if (res < 0) {
      std::cout << "DIr create fails X" << std::endl;
      return -2;
    }

    const int dir_x = open(dir_x_path.c_str(), O_RDONLY);
    if (dir_x < 0) {
      return -3;
    }

    //fsync the new directory
    res = fsync(dir_x);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
    }

    //Close open files  
    close(dir_x);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    std::cout << "Dir X :" << std::endl;
    system("ls /mnt/snapshot/test_dir_a/test_dir_x");
    std::cout << "Dir Y :" << std::endl;    
    system("ls /mnt/snapshot/test_dir_a/test_dir_y"); 

    struct stat stats_old;
    struct stat stats_new;
    const int stat_old_res_foo = stat(foo_path.c_str(), &stats_old);
    const int errno_old_foo = errno;
    const int stat_new_res_foo = stat(foo_path_moved.c_str(), &stats_new);
    const int errno_new_foo = errno;

    const int stat_old_res_bar = stat(bar_path.c_str(), &stats_old);
    const int errno_old_bar = errno;
    const int stat_new_res_bar = stat(foo_path_moved.c_str(), &stats_new);
    const int errno_new_bar = errno;

    bool foo_missing = false, bar_missing = false;
    bool foo_present_both = false, bar_present_both = false;

    // Neither stat found the file, it's gone...
    if (stat_old_res_foo < 0 && errno_old_foo == ENOENT &&
        stat_new_res_foo < 0 && errno_new_foo == ENOENT) {
      foo_missing = true;
    }
    if (stat_old_res_bar < 0 && errno_old_bar == ENOENT &&
        stat_new_res_bar < 0 && errno_new_bar == ENOENT) {
      bar_missing = true;
    }

    if(foo_missing || bar_missing){
      test_result->SetError(DataTestResult::kFileMissing); 
      if(foo_missing && bar_missing)
        test_result->error_description = " : " + foo_path + ", " + bar_path + " missing in both dir X and Y";
      else if(foo_missing){
        test_result->error_description = " : " + foo_path + " missing in dir X and Y";
      }
      else
        test_result->error_description = " : " + bar_path + " missing in dir X and Y";
    return 0;
    }

    //Files present at both directories
    if (stat_old_res_foo >= 0 && stat_new_res_foo >= 0) 
      foo_present_both = true;
    if (stat_old_res_bar >= 0 && stat_new_res_bar >= 0) 
      bar_present_both = true;
    
    if(foo_present_both || bar_present_both){
      test_result->SetError(DataTestResult::kOldFilePersisted); 
      if(foo_present_both && bar_present_both)
        test_result->error_description = " : " + foo_path + ", " + bar_path + " present in both dir X and Y";
      else if(foo_present_both)
        test_result->error_description = " : " + foo_path + " present in dir X and Y";
      else
        test_result->error_description = " : " + bar_path + " present in dir X and Y";
    return 0;
    }


    return 0;
  }

   private:
    const string dir_x_path = TEST_MNT "/" TEST_DIR_A "/" TEST_DIR_X;    
    const string dir_y_path = TEST_MNT "/" TEST_DIR_A "/" TEST_DIR_Y;
    const string foo_path = string(dir_x_path) + "/" + TEST_FILE_FOO;
    const string foo_path_moved = string(dir_y_path) + "/" + TEST_FILE_FOO;
    const string bar_path = string(dir_x_path) + "/" + TEST_FILE_BAR;
    const string bar_path_moved = string(dir_y_path) + "/" + TEST_FILE_BAR;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic341;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
