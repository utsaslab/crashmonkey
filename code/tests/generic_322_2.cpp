/*
Reproducing xfstest generic/322

1. Create file foo and write some contents into it and do fsync
2. Rename foo to bar and do fsync

After a crash at random point, bar should be present and should have the same contents
written to foo.

https://github.com/kdave/xfstests/blob/master/tests/generic/322
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


class Generic322_2: public BaseTestCase {
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

    // Write some contents to the file
    const char *buf = file_contents.c_str();
    if (pwrite(fd_foo, buf, strlen(buf), 0) < 0) {
    	return -2;
    }

    // Sync the file
    if (fsync(fd_foo) < 0) {
    	return -1;
    }

    // write more contents
    const char *buf2 = file_contents2.c_str();
    if (pwrite(fd_foo, buf2, strlen(buf2), 0) < 0) {
    	return -2;
    }

    // sync again
    sync();
    close(fd_foo);
    return 0;
  }

  virtual int run() override {

	// Rename the foo to bar
	if (rename(foo_path.c_str(), bar_path.c_str()) != 0) {
		return -2;
	}
  
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -3;
    }
    if (fsync(fd_bar) < 0) {
    	return -4;
    }

    if (Checkpoint() < 0){
      return -5;
    }

    //Close open files  
    close(fd_bar);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
	if (fd_bar < 0 && last_checkpoint >= 1) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Unable to locate bar file after crash recovery";
	}

	// Read contents of the file and check if contents are same
	char buf[100];
	if (pread(fd_bar, buf, sizeof(buf), 0) < 0) {
		return -6;
	}
	if (strncmp(buf, file_contents2.c_str(), file_contents2.length()) != 0 && last_checkpoint >= 1) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Contents of bar does not match with expected contents";
	}

	close(fd_bar);
    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;
    const string bar_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_BAR;
    const string file_contents = "some random file contents...";
    const string file_contents2 = "some random file contents again...";
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic322_2;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
