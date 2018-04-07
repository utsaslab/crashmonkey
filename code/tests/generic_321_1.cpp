/*
Reproducing xfstest generic/321 directory_test()

1. Create directory A and fsync it

After a crash, the directory should still be present.

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

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_DIR_A "test_dir_a"


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
    return 0;
  }

  virtual int run() override {

	init_paths();

	// Create test directory A.
	int res = mkdir(dir_path.c_str(), 0777);
	if (res < 0) {
	  return -1;
	}

	// fsync the directory
	int dir = open(dir_path.c_str(), O_RDONLY, O_DIRECTORY);
	if (dir < 0) {
		return -4;
	}

	if (fsync(dir) < 0) {
		return -5;
	}

	if (Checkpoint() < 0){
      return -5;
    }

	close(dir);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    struct stat stats;
    const int stats_res = stat(dir_path.c_str(), &stats);
    const int errno_stats = errno;

    if (stats_res < 0 && errno_stats == ENOENT && last_checkpoint == 1) {
      test_result->SetError(DataTestResult::kFileMissing);
      test_result->error_description = " : " + dir_path + " is missing";
      return 0;
    }

    // If it is not a directory
    if (!S_ISDIR(stats.st_mode) && last_checkpoint == 1) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : " + dir_path + " is not a directory";
        return 0;
    }

    return 0;
  }

 private:
  string dir_path;

  void init_paths() {
	  dir_path = mnt_dir_ + "/" TEST_DIR_A;
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
