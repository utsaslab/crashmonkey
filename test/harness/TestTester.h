#ifndef TEST_TESTER_H
#define TEST_TESTER_H

#include "../../code/harness/Tester.h"

namespace fs_testing {
namespace test {

class TestTester {
 public:
  TestTester();
  void set_tester_snapshot(char *sn, size_t size);

  Tester tester;
};

}  // namespace test
}  // namespace fs_testing

#endif
