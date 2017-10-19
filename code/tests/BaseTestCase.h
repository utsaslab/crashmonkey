#ifndef TEST_CASE_H
#define TEST_CASE_H

#include "../results/DataTestResult.h"
#include <string>
namespace fs_testing {
namespace tests {

class BaseTestCase {
 public:
  virtual ~BaseTestCase() {};
  virtual int setup() = 0;
  virtual int run() = 0;
  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) = 0;
  virtual int pass(std::string mountDir, std::string filesysSize) {}
};

typedef BaseTestCase *test_create_t();
typedef void test_destroy_t(BaseTestCase *instance);
}  // namespace tests
}  // namespace fs_testing

#endif
