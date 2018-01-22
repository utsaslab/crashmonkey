#ifndef HARNESS_TEST_SUITE_RESULT_H
#define HARNESS_TEST_SUITE_RESULT_H

#include <vector>

#include <iostream>

#include "SingleTestInfo.h"

namespace fs_testing {

class TestSuiteResult {
 public:
  void AddCompletedTest(const fs_testing::SingleTestInfo& done);
  unsigned int GetCompleted() const;
  void PrintResults(std::ostream& os, bool is_log) const;
  bool SingleTestResult(const SingleTestInfo& test_info);
  void RetrieveFailedTests(std::vector<int>& failed_tests);

 private:
  std::vector<fs_testing::SingleTestInfo> completed_;
};

}  // namespace fs_testing

#endif  // HARNESS_TEST_SUITE_RESULT_H
