#ifndef TEST_CASE_H
#define TEST_CASE_H

namespace fs_testing {
namespace tests {

class BaseTestCase {
 public:
  virtual ~BaseTestCase() {};
  virtual int setup() = 0;
  virtual int run() = 0;
  virtual int check_test() = 0;
};

typedef BaseTestCase *test_create_t();
typedef void test_destroy_t(BaseTestCase *instance);

}  // namespace tests
}  // namespace fs_testing

#endif
