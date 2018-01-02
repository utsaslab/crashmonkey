#ifndef HARNESS_TEST_SUITE_RESULT_H
#define HARNESS_TEST_SUITE_RESULT_H

#include <vector>

#include <iostream>

#include "SingleTestInfo.h"

namespace fs_testing {

class TestSuiteResult {
 public:
  void TallyResult(fs_testing::SingleTestInfo& r);
  unsigned int GetCompleted() const;
  void PrintResults(std::ostream& os) const;

 private:
  unsigned int num_failed_ = 0;
  unsigned int num_passed_fixed_ = 0;
  unsigned int num_passed_ = 0;
  unsigned int total_tests_ = 0;

  unsigned int fsck_required_ = 0;
  unsigned int old_file_persisted_ = 0;
  unsigned int file_missing_ = 0;
  unsigned int file_data_corrupted_ = 0;
  unsigned int file_metadata_corrupted_ = 0;
  unsigned int incorrect_block_count_ = 0;
  unsigned int other_ = 0;

};

}  // namespace fs_testing

#endif  // HARNESS_TEST_SUITE_RESULT_H
