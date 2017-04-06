#include "TestTester.h"

namespace fs_testing {
namespace test {

TestTester::TestTester() : tester(Tester(false)) {}

void TestTester::set_tester_snapshot(char *sn, size_t size) {
  if (tester.device_clone) {
    delete[] tester.device_clone;
  }
  tester.device_clone = sn;
  tester.device_size = size;
}

}  // namespace test
}  // namespace fs_testing
