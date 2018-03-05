/*
Reproducing new btrfs bug reported

1. Create directory A under TEST_MNT
2. Create file foo in dir A
3. fsync foo

If a power fail occurs after populating the fsync log tree, 
create of a new file should succeed after recovery. 


Fails on btrfs kernel 4.16. Create fails with EEXIST error because
the fs tree is not searced for highest object id after replay, thereby returning
the object id in use by file foo. So create of file bar fails.
https://patchwork.kernel.org/patch/10184679/

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
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class BtrfsInodeEEXIST: public BaseTestCase {
 public:
  virtual int setup() override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    // Create test directory A.
    int res = mkdir(dira_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Sync everything
    sync();

    return 0;
  }

  virtual int run() override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }
  
    // By doing a fsync we create a fsync log that'll be replayed during recovery
    // A sync() or sleep(30) won't help reproduce the bug
    if(fsync(fd_foo) < 0 ){
      close(fd_foo);
      return -2;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -3;
    }

    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    //Let's try creating a new file bar after log recovery. It should succeed.
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    const int errno_bar = errno;
    if (fd_bar < 0 && errno_bar == EEXIST ) {
      // std::cout << "Can't create file bar " << std::endl;
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);          
      test_result->error_description = " : Cannot create new file bar : EEXIST error";
      return -2;
    }

    close(fd_bar);

    return 0;
  }

   private:
    string foo_path;   
    string bar_path;
    string dira_path;
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::BtrfsInodeEEXIST;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
