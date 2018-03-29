/*
Reproducing fstest generic/041

1. Create file foo
2. Create 3000 links for file foo in the same dir
    (link_0 to link_2999)
3. sync() everything so far
4. Remove a link -> link_0
5. Create a new link with a new name -> link_3000
6. Create a new link with the old name -> link_0
5. fsync(foo)

If a power fail occurs now, and remount the filesystem,
the dir A must be removable, after file foo and all links
are deleted.


This is tested to fail on btrfs(kernel 3.13) What happens here
is that when we create a large number of links, some
of which are extref and turn a regular ref into an 
extref, fsync the inode and then replay the fsync log 
we can endup with an fsync log that makes the replay code always fail with -EOVERFLOW when processing
the inode's references. So the FS won't mount unless
you explicitly use btrfs-zero-log to delete the fsync log.

https://patchwork.kernel.org/patch/5622341/
https://www.spinics.net/lists/linux-btrfs/msg41157.html
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
#define TEST_FILE_FOO_LINK "foo_link_"
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


class Generic041: public BaseTestCase {
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
      return -1;
    }

    //create 3000 hard links
    for (int i = 0; i < 3000; i++ ){
      string foo_link = foo_link_path + std::to_string(i);
      //std::cout << foo_link << std::endl;
      if(link(foo_path.c_str(), foo_link.c_str()) < 0){
        return -2;
      }
    }

    sync();
    close(fd_foo);
    return 0;
  }

  virtual int run(int checkpoint) override {
  
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //Remove one link : foo_link_0
    string foo_link_old = foo_link_path + "0";
    if(remove(foo_link_old.c_str()) < 0)
      return -1;

    //create a new link for file foo
    string foo_link_new = foo_link_path + "3000";
    if (link(foo_path.c_str(), foo_link_new.c_str()) < 0){
      return -2;
    }

    //next create another link with old name - deleted link
    if (link(foo_path.c_str(), foo_link_old.c_str()) < 0){
      return -3;
    }

    //fsync foo
    int res = fsync(fd_foo);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
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

    //Remove all files within  test_dir_a
    system("rm -f /mnt/snapshot/test_dir_a/*");

    //Now try removing the directory itself.
    const int rem = rmdir(TEST_MNT "/" TEST_DIR_A);
    const int err = errno;


    //If rmdir failed because the dir was not empty, it's a bug
    if(rem < 0 && err == ENOTEMPTY){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      test_result->error_description = " : Cannot remove dir even if empty";    
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;    
    const string foo_link_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic041;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
