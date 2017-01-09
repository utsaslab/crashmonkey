#ifndef TEST_CASE_H
#define TEST_CASE_H

namespace fs_testing {

class test_case {
 public:
  virtual ~test_case() {};
  virtual int setup() = 0;
  virtual int run() = 0;
  virtual int check_test() = 0;
};

typedef test_case* create_t();
typedef void destroy_t(test_case*);

}  // namespace fs_testing

#endif
