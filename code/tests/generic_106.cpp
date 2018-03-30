/*
Reproducing xfstest generic/106

1. Create file foo, create a link foo_link and sync
2. Unlink foo_link, drop caches and fsync foo

After a crash, after the file foo is removed, the directory should be removeable.

https://github.com/kdave/xfstests/blob/master/tests/generic/106
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


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic106: public BaseTestCase {
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
    // Create a new hard link
    if(link(foo_path.c_str(), foo_link_path.c_str()) < 0){
      return -2;
    }

    sync();
    close(fd_foo);

    return 0;
  }

  virtual int run() override {

	init_paths();
  
    // Unlink foo_link
    if(unlink(foo_link_path.c_str()) < 0){
      return -3;
    }

    // drop caches
    if (system("echo 2 > /proc/sys/vm/drop_caches") < 0) {
    	return -4;
    }

	const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -5;
    }

    if (fsync(fd_foo) < 0) {
    	return -6;
    }

    // Make a user checkpoint here
    if (Checkpoint() < 0){
      return -7;
    }

    //Close open files  
    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

	string dir = mnt_dir_ + "/" TEST_DIR_A;
	string rm_cmd = "rm -f " + dir + "/*";

	// Remove files within the directory
	system(rm_cmd.c_str());

	// Directory should be removeable
	if (rmdir(dir.c_str()) < 0) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Unable to remove directory after deleting files";
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
  return new fs_testing::tests::Generic106;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
