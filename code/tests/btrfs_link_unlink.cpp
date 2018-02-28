/*
Reproducing new btrfs bug reported

1. Create directory A under TEST_MNT
2. Create file foo in dir A
3. Create a link bar to file foo in the same dir A
4. Sync the changes
5. Unlink bar
6. Create file bar again
7. fsync(bar)

If a power fail occurs now, the file system must be mountable


Fails on btrfs kernel 4.16. The filesystem becomes unmountable

https://marc.info/?l=linux-btrfs&m=151983337811947&w=2
https://marc.info/?l=linux-btrfs&m=151983349112005&w=2
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


class BtrfsLinkUnlink: public BaseTestCase {
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

    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -2;
    }

    //create a link bar for file foo
    if (link(foo_path.c_str(), bar_path.c_str()) < 0){
      return -3;
    }

    //Sync everything
    sync();
    close(fd_foo);

    return 0;
  }

  virtual int run() override {

    //initialize paths
        //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;
  
    //unlink file bar
    if(unlink(bar_path.c_str()) < 0){
      return -1;
    }

    //create bar again
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -2;
    }

    //fsync file bar
    int res = fsync(fd_bar);
    if (res < 0){
      return -3;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -4;
    }

    //Close open files  
    close(fd_bar);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

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
  return new fs_testing::tests::BtrfsLinkUnlink;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
