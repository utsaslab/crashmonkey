#ifndef HARNESS_SINGLE_TEST_INFO_H
#define HARNESS_SINGLE_TEST_INFO_H

#include "DataTestResult.h"
#include "FileSystemTestResult.h"
#include "PermuteTestResult.h"

namespace fs_testing {

// TODO(ashmrtn): Watch memory consumption with this. If we have 1 million tests
// then we will have 1 million class instantiations which could be a lot of
// memory. We are already potentially using a lot of memory for the RAM block
// device and disk wrapper logging and we want this to be a useable test
// harness.

// Container for all things related to the crash state being tested including
// 1. bio prefix used to generate the state
// 2. permutation of bios in final epoch of state
// 3. results of user data consistency tests
// 4. results of file system consistency tests and repair(s)
class SingleTestInfo {
 public:
  enum ResultType {
    kPassed,
    kFsckFixed,
    kFsckRequired,
    kFailed
  };

  SingleTestInfo();
  void PrintResults(std::ostream& os) const;
  SingleTestInfo::ResultType GetTestResult() const;

  unsigned int test_num;
  fs_testing::tests::DataTestResult data_test;
  fs_testing::FileSystemTestResult fs_test;
  fs_testing::PermuteTestResult permute_data;
};

std::ostream& operator<<(std::ostream& os, SingleTestInfo::ResultType rt);

}  // namespace fs_testing

#endif  // HARNESS_SINGLE_TEST_INFO_H
