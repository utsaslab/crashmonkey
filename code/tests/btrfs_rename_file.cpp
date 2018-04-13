/*

Reproducing new btrfs bug reported : create a regular file, rename it, and 
create a hard link with the old name of the special file, a crash shouldn't 
make the FS unmountable

1. Create directory A under TEST_MNT
2. Create dregular file foo in dir A
3. Create log tree by creating a dummy file and fsync it in dir a
4. Rename the regular file foo to bar
5. Create a link 'foo' to file bar
6. To persist this in the log tree, delete dummy file and fsync it 
//(We want steps 4 and 5 in same tx)


If a power fail occurs now, the file system must be mountable

Does not fail on any FS
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
#define TEST_FILE_DUMMY "dummy"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class BtrfsRenameFile: public BaseTestCase {
 public:
  virtual int setup() override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dummy_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_DUMMY;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    // Create test directory A.
    int res = mkdir(dira_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Create a FIFO file(named pipe) foo in dir a
    const int fd_foo = open(foo_path.c_str(), O_RDWR| O_CREAT, TEST_FILE_PERMS);
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

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    bar_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_BAR;
    dummy_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_DUMMY;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    //Create the log tree by creating a random file in dir a, and fsyncing it
    const int fd_dummy = open(dummy_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_dummy < 0) {
      return -1;
    }

    int res = fsync(fd_dummy);
    if (res < 0){
      close(fd_dummy);
      return -2;
    }

    //Now rename the regular file from foo to bar
    if(rename(foo_path.c_str() , bar_path.c_str()) < 0){
      close(fd_dummy);
      return -3;
    }

    //Create a link to bar. Call this foo again
    if (link(bar_path.c_str(), foo_path.c_str()) < 0){
      close(fd_dummy);
      return -4;
    }

    //persist the log tree. We'll delete the dummy file and fsync it to trigger this
    if( remove(dummy_path.c_str()) < 0) {
      close(fd_dummy);
      return -5;
    }

    //fsync file bar
    res = fsync(fd_dummy);
    if (res < 0){
      close(fd_dummy);
      return -6;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -4;
    }
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    close(fd_dummy);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    return 0;
  }

   private:
    string foo_path;    
    string bar_path;
    string dummy_path;
    string dira_path;
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::BtrfsRenameFile;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
