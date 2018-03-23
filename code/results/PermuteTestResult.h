#ifndef TESTS_PERMUTE_TEST_RESULT_H
#define TESTS_PERMUTE_TEST_RESULT_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs_testing {

class PermuteTestResult {
 public:
  std::ostream& PrintCrashStateSize(std::ostream& os) const;
  std::ostream& PrintCrashState(std::ostream& os) const;

  unsigned int last_checkpoint;
  std::vector<unsigned int> crash_state;
  std::vector<std::pair<unsigned int, unsigned int>> sector_crash_state;

 private:
  std::ostream& PrintFullBioCrashState(std::ostream &os) const;
  std::ostream& PrintSectorCrashState(std::ostream &os) const;
};

}  // namespace fs_testing

#endif  // TESTS_PERMUTE_TEST_RESULT_H

