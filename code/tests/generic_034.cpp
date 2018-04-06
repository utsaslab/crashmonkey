/*
Reproducing fstest generic/034

1. Create directory A under TEST_MNT.
2. Create file foo in dir A
3. Sync the changes
4. Create file bar in dir A
5. Fsync(A)
6. fsync(A/bar)

If a power fail occurs now, and remount the filesystem,
if we delete files foo and bar from dir A, then it must
be possible to remove dir A.

This is tested to fail on btrfs(kernel 3.13) 
Removing both foo and bar from dir A should set its
inode i_size to 0, however due to incorrect log replay,
the i_size remains non-zero even after deleting files,
and hence can't be deleted on btrfs.
(https://patchwork.kernel.org/patch/4864571/)
(http://linux-btrfs.vger.kernel.narkive.com/PoV66uvM/patch-
xfstests-generic-add-dir-fsync-test-motivated-by-a-btrfs-bug)
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


class Generic034: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }
  
    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -2;
    }

    //Sync everything
    sync();

    close(fd_foo);

    return 0;
  }

  virtual int run(int checkpoint) override {
    int local_checkpoint = 0;

    //touch TEST_DIR_A/bar 
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -1;
    }

    //open dir A
    const int dir_a = open(TEST_MNT "/" TEST_DIR_A, O_RDONLY);
    if (dir_a < 0) {
      return -2;
    }

    //fsync dir A
    int res = fsync(dir_a);
    if (res < 0){
      return -3;
    }
    //fsync  file bar
    res = fsync(fd_bar);
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
    close(fd_bar);
    close(dir_a);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    //std::cout << "Dir A :" << std::endl;
    //system("ls /mnt/snapshot/test_dir_a");

    struct dirent *dir_entry;
    bool foo_present_in_A = false;
    bool bar_present_in_A = false;

    DIR *dir = opendir(TEST_MNT "/" TEST_DIR_A);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
          if (strcmp(dir_entry->d_name, "foo") == 0){
            foo_present_in_A = true;
          }
          if (strcmp(dir_entry->d_name, "bar") == 0){
            bar_present_in_A = true;
          }
      }
    }

    closedir(dir);

    //remove file foo, if present
    if(foo_present_in_A){
      if(remove(foo_path.c_str())  < 0)
        return -1;
    }

    //remove file bar, if present
    if(bar_present_in_A){
      if(remove(bar_path.c_str())  < 0)
        return -2;
    }

    //readdir again. It should be empty now
    dir = opendir(TEST_MNT "/" TEST_DIR_A);

    if((dir_entry = readdir(dir)) != NULL &&
      (!strcmp(dir_entry->d_name, "foo") ||
      !strcmp(dir_entry->d_name, "bar"))){
      std::cout << "Dir not empty" << std::endl;
      closedir(dir);
      return -3;
    }

    else {
      //we know directory is empty. Try remove(dir)
      closedir(dir);
      // If dir cannot be removed because of ENOTEMPTY, 
      // its a bug, because we verified the directory
      // is empty 
      const int rem = rmdir(TEST_MNT "/" TEST_DIR_A);
      const int err = errno;

      if(rem < 0 && err == ENOTEMPTY){
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Cannot remove dir even if empty";    
      }
      return 0;
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
  return new fs_testing::tests::Generic034;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
