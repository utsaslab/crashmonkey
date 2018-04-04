/*
Reproducing xfstest generic/037

1. Create file foo under mnt_dir_
2. In a loop, set the attribute to file foo with alternating values

After a crash at a random point, the value of the xattr should be one among
the two defined values.

Verify that replacing a xattr's value is an atomic operation.
This is motivated by an issue in btrfs where replacing a xattr's value
wasn't an atomic operation, it consisted of removing the old value and
then inserting the new value in a btree. This made readers (getxattr
and listxattrs) not getting neither the old nor the new value during
a short time window.

The btrfs issue was fixed by the following linux kernel patch:

   Btrfs: make xattr replace operations atomic

https://github.com/kdave/xfstests/blob/master/tests/generic/037
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
#define ATTR_KEY "user.foo"
#define ATTR_VALUE1 "val1"
#define ATTR_VALUE2 "value2"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic037: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    //Create file foo in mnt_dir_/
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }
    close(fd_foo);

    // Set the attribute at least once so that if it crashes at the beginning of run()
    // method, the check_test should pass
    string val = string(ATTR_VALUE1);
    int res = setxattr(foo_path.c_str(), ATTR_KEY, val.c_str(), val.size(), 0);
    if (res < 0) {
      return -1;
    }

    sync();

    return 0;
  }

  virtual int run() override {

	init_paths();

    // set attr value to file foo in a loop
	int num_iterations = 10000;
	int num_checkpoints = 3; // use 3 checkpoints overall

	for (int i = 0; i < num_iterations; i++) {
		string val;
		if (i % 2 == 0) {
			val = string(ATTR_VALUE1);
		} else {
			val = string(ATTR_VALUE2);
		}

	    int res = setxattr(foo_path.c_str(), ATTR_KEY, val.c_str(), val.size(), 0);
	    if (res < 0) {
	      return -1;
	    }

	    if (i % (num_iterations/num_checkpoints) == 0) {
	        if (Checkpoint() < 0){
	          return -3;
	        }
	    }
	}

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	// In any case, the attribute value should be either of VAL1 or VAL2
	init_paths();

	char val[1024];
	int num_bytes = getxattr(foo_path.c_str(), ATTR_KEY, val, sizeof(val));
	// If foo's attribute is missing or is not matching with VAL1 or VAL2
	if(num_bytes < 0 || (strncmp(val, ATTR_VALUE1, num_bytes) != 0 && strncmp(val, ATTR_VALUE2, num_bytes) != 0)) {
		test_result->SetError(DataTestResult::kFileMetadataCorrupted);
		test_result->error_description = " : File foo's attribute missing or not matching with val1";
		return 0;
	}
    return 0;
  }

 private:
  string foo_path;

  void init_paths() {
	  foo_path = mnt_dir_ + "/" TEST_FILE_FOO;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic037;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
