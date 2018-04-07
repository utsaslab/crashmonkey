/*
Reproducing xfstest generic/035 overwriting regular file

1. Create two files file1 and file2 under dir A
2. Rename (overwrite) file1 to file2 (nlink of old file2 should be 0)
3. Remove file file2

After a crash, the directory A should be removeable

https://github.com/kdave/xfstests/blob/master/tests/generic/035
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

#define TEST_DIR_A "test_dir_a"
#define FILE1_NAME "file1"
#define FILE2_NAME "file2"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic321_1: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

	// Create test directory A.
	int res = mkdir(dir_path.c_str(), 0777);
	if (res < 0) {
	  return -1;
	}

	// open file1 and file2
	int fd1 = open(file1_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
	if (fd1 < 0) {
		return -2;
	}
	int fd2 = open(file2_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
	if (fd2 < 0) {
		return -2;
	}

	sync();

	close(fd1);
	close(fd2);

    return 0;
  }

  virtual int run() override {

	init_paths();

	int fd2 = open(file2_path.c_str(), O_RDONLY, TEST_FILE_PERMS);
	if (fd2 < 0) {
		return -3;
	}

	if (rename(file1_path.c_str(), file2_path.c_str()) < 0) {
		return -4;
	}

	struct stat stats;
    const int stats_res = fstat(fd2, &stats);

    if (stats_res < 0) {
    	return -5;
    }

    // Since the original file was overwritten, nlink should be 0
    if (stats.st_nlink != 0) {
        return -6;
    }
    close(fd2);

	// fsync file2
	fd2 = open(file2_path.c_str(), O_RDONLY, TEST_FILE_PERMS);
	if (fd2 < 0) {
		return -7;
	}
	fsync(fd2);

	if (Checkpoint() < 0){
      return -8;
    }

	close(fd2);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    if (remove(file2_path.c_str()) < 0) {
    	return -9;
    }

    // If the checkpoint is 1, the directory should be removeable
    const int rem = rmdir(dir_path.c_str());
    const int err = errno;

    if(rem < 0 && err == ENOTEMPTY && last_checkpoint == 1){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      test_result->error_description = " : Cannot remove dir even if empty";
    }

    return 0;
  }

 private:
  string dir_path;
  string file1_path;
  string file2_path;

  void init_paths() {
	  dir_path = mnt_dir_ + "/" TEST_DIR_A;
	  file1_path = dir_path + "/" FILE1_NAME;
	  file2_path = dir_path + "/" FILE2_NAME;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic321_1;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
