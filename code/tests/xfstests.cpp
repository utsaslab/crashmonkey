#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <string>

#include "BaseTestCase.h"
#include "../user_tools/api/actions.h"

using std::calloc;
using std::string;
using std::to_string;

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::Checkpoint;

#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR "test_dir"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {

using std::memcmp;

class Xfstests : public BaseTestCase {
 public:
  virtual int setup() override {
    return 0;
  }

  virtual int run() override {
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_res) override {
    return 0;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Xfstests;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
