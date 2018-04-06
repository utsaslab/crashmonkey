#ifndef BASE_TEST_CASE_H
#define BASE_TEST_CASE_H

#include "../results/DataTestResult.h"

#include <string>

namespace fs_testing {
namespace tests {

class BaseTestCase {
 public:
  virtual ~BaseTestCase() {};
  virtual int setup() = 0;
  virtual int run(int checkpoint) = 0;
  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) = 0;
  virtual int init_values(std::string mount_dir, long filesys_size);

 protected:
  std::string mnt_dir_;
  long filesys_size_;
};

typedef BaseTestCase *test_create_t();
typedef void test_destroy_t(BaseTestCase *instance);

}  // namespace tests
}  // namespace fs_testing

#endif
