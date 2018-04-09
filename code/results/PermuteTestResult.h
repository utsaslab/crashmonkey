#ifndef TESTS_PERMUTE_TEST_RESULT_H
#define TESTS_PERMUTE_TEST_RESULT_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../utils/utils.h"

namespace fs_testing {

class PermuteTestResult {
 public:
  std::ostream& PrintCrashStateSize(std::ostream& os) const;
  std::ostream& PrintCrashState(std::ostream& os) const;

  unsigned int last_checkpoint;
  std::vector<fs_testing::utils::DiskWriteData> crash_state;

};

}  // namespace fs_testing

#endif  // TESTS_PERMUTE_TEST_RESULT_H

