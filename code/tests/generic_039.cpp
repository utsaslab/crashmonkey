/*
Reproducing fstest generic/039

1. Create directory A under TEST_MNT
2. Create dir B under dir A
3. Create file foo in dir B
4. Create a link bar to file foo in the same dir B
5. Sync the changes
6. Remove the link bar to file foo
5. fsync(foo)

If a power fail occurs now, and remount the filesystem,
the dir B must be removable, after file foo is deleted.


This is tested to fail on btrfs(kernel 3.13) 
On replaying the log tree, the i_size due to the removed link
bar was not decremented in dir B, and left dangling directory
index references. This makes B unremovable. This is also detected
by btrfs check - returns error.

https://patchwork.kernel.org/patch/5853491/
https://patchwork.kernel.org/patch/5576901/
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
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic039: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }
    sync();
    return 0;
  }

  virtual int run(int checkpoint) override {
    int local_checkpoint = 0;
  
    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //create a link bar for file foo
    if (link(foo_path.c_str(), bar_path.c_str()) < 0){
      return -2;
    }

    //Sync everything
    sync();

    //remove file bar
    if(remove(bar_path.c_str()) < 0){
      return -1;
    }

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

    char mode[] = "0777";
    int i_mode = strtol(mode,0,8);
    chmod(foo_path.c_str(), i_mode);
    chmod(bar_path.c_str(), i_mode);

    //Remove all files within  test_dir_a
    system("rm -f /mnt/snapshot/test_dir_a/*");

    //Now try removing the directory itself.
    const int rem = rmdir(TEST_MNT "/" TEST_DIR_A);
    const int err = errno;

    //Do we need a FS check to detect inconsistency?
    bool need_check = false;

    //If rmdir failed because the dir was not empty, it's a bug
    if(rem < 0 && err == ENOTEMPTY){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      test_result->error_description = " : Cannot remove dir even if empty";    
      need_check = true;
    }

    //We do a FS check :
    // We know this is a btrfs specific bug, so we use btrfs check
    if(need_check){
        system("umount /mnt/snapshot");
        string check_string;
        char tmp[128];
        string command("btrfs check /dev/cow_ram_snapshot1_0 2>&1");
        FILE *pipe = popen(command.c_str(), "r");
        while (!feof(pipe)) {
        char *r = fgets(tmp, 128, pipe);
        // NULL can be returned on error.
          if (r != NULL) {
            check_string += tmp;
           }
        }

        int ret = pclose(pipe);
        if(ret != 0){
          test_result->error_description += " \nbtrfs check result : \n " + check_string;
        }
      }
    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;    
    const string bar_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_BAR;
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic039;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
