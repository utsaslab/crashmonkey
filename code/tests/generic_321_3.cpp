/*
Reproducing xfstest generic/321 replay_rename_test()

1. Create file foo under mnt_dir_
2. Create a directory A, and fsync file foo
3. Move the file foo to directory A
4. fsync directory A
5. setfattr -n user.foo -v blah foo_file
6. fsync file foo

After a crash, mnt_dir_ should contain only the directory A
and A should contain just the file foo.

https://github.com/kdave/xfstests/blob/master/tests/generic/321
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

// Header file was changed in 4.15 kernel.
#ifdef NEW_XATTR_INC
#include <sys/xattr.h>
#else
#include <attr/xattr.h>
#endif

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic321_3: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    //Create file foo in mnt_dir_/
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    // Create test directory A.
	int res = mkdir(dir_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    // Sync the file foo
    if (fsync(fd_foo) < 0) {
    	return -1;
    }

    close(fd_foo);
    sync();

    return 0;
  }

  virtual int run(int checkpoint) override {

	init_paths();

  int local_checkpoint = 0;

	// Move foo to directory A
	if (rename(foo_path.c_str(), foo_moved_path.c_str()) != 0) {
		return -2;
	}

	// fsync directory A
    // fsync the directory
    int dir = open(dir_path.c_str(), O_RDONLY, O_DIRECTORY);
    if (dir < 0) {
    	return -4;
    }

    if (fsync(dir) < 0) {
    	return -5;
    }
    close(dir);

    const int fd_foo_moved = open(foo_moved_path.c_str(), O_RDONLY, TEST_FILE_PERMS);
    if (fd_foo_moved < 0) {
      return -1;
    }

    // set attr to file foo
    int res = fsetxattr(fd_foo_moved, "user.foo", "val1", 4, 0);
    if (res < 0) {
      return -1;
    }

    // fsync file foo
    if (fsync(fd_foo_moved) < 0) {
    	return -1;
    }
    close(fd_foo_moved);

    if (Checkpoint() < 0){
      return -5;
    }

    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    struct dirent *dir_entry;
    bool foo_present_in_mnt = false;
    bool foo_present_in_A = false;
    bool A_present_in_mnt = false;

    // ls of mnt_dir_
    string dir_name = mnt_dir_;
    DIR *dir = opendir(dir_name.c_str());

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
		  if (strcmp(dir_entry->d_name, "foo") == 0){
			foo_present_in_mnt = true;
		  }
		  if (strcmp(dir_entry->d_name, TEST_DIR_A) == 0){
			A_present_in_mnt = true;
		  }
      }
    }

    closedir(dir);

    // ls of dir A
    dir_name = dir_path;
    dir = opendir(dir_name.c_str());

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
		  if (strcmp(dir_entry->d_name, "foo") == 0){
			foo_present_in_A = true;
		  }
      }
    }

    closedir(dir);

    if (foo_present_in_A && foo_present_in_mnt) {
		test_result->SetError(DataTestResult::kOldFilePersisted);
		test_result->error_description = " : Both old and new files present";
		return 0;
    }

    if (!foo_present_in_A && !foo_present_in_mnt) {
		test_result->SetError(DataTestResult::kFileMissing);
		test_result->error_description = " : Both old and new files are missing";
		return 0;
    }

    if (last_checkpoint == 1) {
    	if (!A_present_in_mnt) {
    		test_result->SetError(DataTestResult::kFileMissing);
    		test_result->error_description = " : Directory A missing under mnt_dir_";
    		return 0;
    	} else if (foo_present_in_mnt) {
    		test_result->SetError(DataTestResult::kOldFilePersisted);
    		test_result->error_description = " : File foo still present in mnt_dir_ after checkpoint";
    		return 0;
    	}

        char val[1024];
        int num_bytes = getxattr(foo_moved_path.c_str(), "user.foo", val, sizeof(val));
        // If foo's attribute is missing or is not matching with "val1"
        if(num_bytes < 0 || strncmp(val, "val1", num_bytes) != 0){
    		test_result->SetError(DataTestResult::kFileMetadataCorrupted);
    		test_result->error_description = " : File foo's attribute missing or not matching with val1";
    		return 0;
        }
    }
    return 0;
  }

 private:
  string foo_path;
  string foo_moved_path;
  string dir_path;

  void init_paths() {
	  foo_path = mnt_dir_ + "/" TEST_FILE_FOO;
	  foo_moved_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO;
	  dir_path = mnt_dir_ + "/" TEST_DIR_A;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic321_3;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
