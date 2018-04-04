/*
Reproducing xfstest generic/035 overwriting directory

1. Create two dirs B and C under dir A
2. Rename (overwrite) dir B to dir C (nlink of old dir C should be 0)
3. Remove dir C

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
#define TEST_DIR_B "test_dir_b"
#define TEST_DIR_C "test_dir_c"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic321_2: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

	// Create test directory A.
	int res = mkdir(dir_path.c_str(), 0777);
	if (res < 0) {
	  return -1;
	}

	// Create directories B and C inside A
	res = mkdir(dir1_path.c_str(), 0777);
	if (res < 0) {
	  return -1;
	}

	res = mkdir(dir2_path.c_str(), 0777);
	if (res < 0) {
	  return -1;
	}

	sync();

    return 0;
  }

  virtual int run() override {

	init_paths();

	int dir2 = open(dir2_path.c_str(), O_RDONLY, O_DIRECTORY);
	if (dir2 < 0) {
		return -3;
	}

	if (rename(dir1_path.c_str(), dir2_path.c_str()) < 0) {
		return -4;
	}

	struct stat stats;
    const int stats_res = fstat(dir2, &stats);

    if (stats_res < 0) {
    	return -5;
    }

    // Since the original directory was overwritten, nlink should be 0
    if (stats.st_nlink != 0) {
        return -6;
    }
    close(dir2);

	// fsync dir2
	dir2 = open(dir2_path.c_str(), O_RDONLY, O_DIRECTORY);
	if (dir2 < 0) {
		return -7;
	}
	fsync(dir2);

	if (Checkpoint() < 0){
      return -8;
    }

	close(dir2);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    if (rmdir(dir2_path.c_str()) < 0) {
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
  string dir1_path;
  string dir2_path;

  void init_paths() {
	  dir_path = mnt_dir_ + "/" TEST_DIR_A;
	  dir1_path = dir_path + "/" TEST_DIR_B;
	  dir2_path = dir_path + "/" TEST_DIR_C;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic321_2;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
