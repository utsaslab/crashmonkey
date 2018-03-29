/*
Reproducing xfstest generic/002

1. Create file foo
2. Create 10 links for file foo in the same dir
    (link_0 to link_9) (sync and checkpoint after each create)
3. Remove the 10 links created (sync and checkpoint after each remove)

After a crash at random point, the file should have the correct number of links
corresponding to the checkpoint number.

https://github.com/kdave/xfstests/blob/master/tests/generic/002
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
#define TEST_DIR_A "test_dir_a"
#define NUM_LINKS 10


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

	init_paths();

    // Create test directory A.
	string dir_path = mnt_dir_ + "/" TEST_DIR_A;
    int res = mkdir(dir_path.c_str(), 0777);
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

	init_paths();
  
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

	init_paths();

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

    return 0;
  }

   private:
    string foo_path;
    string foo_link_path;
    
    void init_paths() {
        foo_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO;
        foo_link_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
    }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic002;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
