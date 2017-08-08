#ifndef HARNESS_SINGLE_TEST_INFO_H
#define HARNESS_SINGLE_TEST_INFO_H

#include "../tests/DataTestError.h"

namespace fs_testing {

// Container for all things related to the crash state being tested including
// 1. bio prefix used to generate the state
// 2. permutation of bios in final epoch of state
// 3. results of user data consistency tests
// 4. results of file system consistency tests and repair(s)
struct SingleTestInfo {
  // TODO(ashmrtn): Create class for crash state information.
  // TODO(ashmrtn): Create class for file system consistency test results.
  DataTestResults* data_test;
}

#endif  // HARNESS_SINGLE_TEST_INFO_H
