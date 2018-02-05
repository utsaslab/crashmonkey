/*
Reproducing fstest generic/066

1. Create file foo in  TEST_MNT 
2. Add three xattr to the file foo user.xattr1, user.xattr2, user.xattr3
3. Sync everything
4. Delete the second  xattr - user.xattr2 from foo
5. Fsync file foo

If  we crash now and recover, only xattr 1 and 3 must be present
Tested to fail on btrfs (kernel 3.13)
(https://patchwork.kernel.org/patch/5872261/
https://www.spinics.net/lists/linux-btrfs/msg42162.html)
*/


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <linux/xattr.h>
#include <sys/xattr.h>
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


class Generic066: public BaseTestCase {
 public:
  virtual int setup() override {


    //Create file foo
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //add xattrs to the file foo
    int res = fsetxattr(fd_foo, "user.xattr1", "val1", 4, 0);
    if (res < 0) {
      return -1;
    }

    res = fsetxattr(fd_foo, "user.xattr2", "val2", 4, 0);
    if (res < 0) {
      return -1;
    }

    res = fsetxattr(fd_foo, "user.xattr3", "val3", 4, 0);
    if (res < 0) {
      return -1;
    }

    //Sync everything
    sync();

    close(fd_foo);
    sleep(2);

    return 0;
  }

  virtual int run() override {

    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //remove user.xattr2
    int res = removexattr(foo_path.c_str(), "user.xattr2");
    if (res < 0) {
      return -1;
    }

    //system("setfattr -x user.xattr2 /mnt/snapshot/foo");


    //fsync  file_foo
    res = fsync(fd_foo);
    if (res < 0){
      return -6;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point. 
    if (Checkpoint() < 0){
      return -5;
    }

    //Close open files  
    close(fd_foo);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {
    
    //system("getfattr -d /mnt/snapshot/foo");
    char val[1024];

    //if xattr2 is not present, it is correct behavior
    if(getxattr(foo_path.c_str(), "user.xattr2", val, sizeof(val)) < 0){
      return 0;
    }

    // Check if xattr2 is present and value is what we set
    bool attr2_present = false;
    if(strcmp(val, "val2") == 0){
      attr2_present = true;
    }

    // If we crashed after the checkpoint (i.e fsync()), and
    // if xattr2 is still present, it is a bug.
    if(last_checkpoint == 1 && attr2_present){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted); 
      test_result->error_description = " : " + foo_path + " has deleted xattr";
      return 0; 
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_FILE_FOO;     
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic066;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
