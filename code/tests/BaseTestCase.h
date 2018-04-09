#ifndef BASE_TEST_CASE_H
#define BASE_TEST_CASE_H

#include <string>

#include "../results/DataTestResult.h"
#include "../user_tools/api/wrapper.h"

namespace fs_testing {
namespace tests {

class BaseTestCase {
 public:
  virtual ~BaseTestCase() {};
  virtual int setup() = 0;
  int Run(const int change_fd);
  virtual int run() = 0;
  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) = 0;
  virtual int init_values(std::string mount_dir, long filesys_size);

 protected:
  std::string mnt_dir_;
  long filesys_size_;
  fs_testing::user_tools::api::CmFsOps *cm_;
};

typedef BaseTestCase *test_create_t();
typedef void test_destroy_t(BaseTestCase *instance);

}  // namespace tests
}  // namespace fs_testing

#endif
