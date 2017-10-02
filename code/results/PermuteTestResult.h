#ifndef TESTS_PERMUTE_TEST_RESULT_H
#define TESTS_PERMUTE_TEST_RESULT_H

#include <iostream>
#include <string>
#include <vector>

namespace fs_testing {

class PermuteTestResult {
 public:
  std::ostream& PrintCrashState(std::ostream& os) const;

  unsigned int last_checkpoint;
  std::vector<unsigned int> crash_state;
};

}  // namespace fs_testing

#endif  // TESTS_PERMUTE_TEST_RESULT_H

