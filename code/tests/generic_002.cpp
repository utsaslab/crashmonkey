/*
Reproducing xfstest generic/002

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
#define NUM_LINKS 5


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic002: public BaseTestCase {
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

    sync();
    close(fd_foo);
    return 0;
  }

  virtual int run() override {
  
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    // Create new set of links
    for (int i = 0; i < NUM_LINKS; i++){
      string foo_link = foo_link_path + std::to_string(i);
      if(link(foo_path.c_str(), foo_link.c_str()) < 0){
        return -2;
      }

      // fsync foo
      int res = fsync(fd_foo);
      if (res < 0){
        return -3;
      }

      // Make a user checkpoint here
      if (Checkpoint() < 0){
        return -4;
      }
    }

    // Remove the set of added links
    for (int i = 0; i < NUM_LINKS; i++){
      string foo_link = foo_link_path + std::to_string(i);
      if(remove(foo_link.c_str()) < 0) {
        return -5;
      }

      // fsync foo
      int res = fsync(fd_foo);
      if (res < 0){
        return -6;
      }

      // Make a user checkpoint here
      if (Checkpoint() < 0){
        return -7;
      }
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

    // Since crash could have happened between a fsync and Checkpoint, last_checkpoint only tells the minimum number of operations that have
    // happened. In reality, one more fsync could have happened, which might alter the link count, delta is used to keep track of that. For example,
    // during linking phase, if crash occured between fsync and checkpoint, number of links will be 1 more than what checkpoint says while during remove
    // phase, the same will be 1 less than what the checkpoint says.
    int delta = 0;
    if (last_checkpoint == 2 * NUM_LINKS) { // crash after all fsyncs completed
    	delta = 0;
    } else if (last_checkpoint < NUM_LINKS) { // crash during link phase
    	delta = 1;
    } else { // crash during remove phase
    	delta = -1;
    }

    int num_expected_links;
    if (last_checkpoint <= NUM_LINKS) {
    	num_expected_links = last_checkpoint;
    } else {
    	num_expected_links = 2 * NUM_LINKS - last_checkpoint;
    }

    struct stat st;
    stat(foo_path.c_str(), &st);
    int actual_count = st.st_nlink;
    actual_count--; // subtracting one for the file itself

    // If actual count doesn't match with expected number of links
    if (actual_count != num_expected_links && actual_count != num_expected_links + delta) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Number of links not matching with expected count";
    }

/*    // Remove all files within  test_dir_a
    system("rm -f /mnt/snapshot/test_dir_a/*");

    //Now try removing the directory itself.
    const int rem = rmdir(TEST_MNT "/" TEST_DIR_A);
    const int err = errno;


    // If rmdir failed because the dir was not empty, it's a bug
    if(rem < 0 && err == ENOTEMPTY){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      test_result->error_description = " : Cannot remove dir even if empty";
    }
*/
    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;    
    const string foo_link_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic002;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
