#ifndef HARNESS_SINGLE_TEST_INFO_H
#define HARNESS_SINGLE_TEST_INFO_H

#include "../tests/DataTestResult.h"
#include "FileSystemTestResult.h"

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
struct SingleTestInfo {
  // TODO(ashmrtn): Create class for crash state information.
  fs_testing::tests::DataTestResult data_test;
  fs_testing::FileSystemTestResult fs_test;
};

}  // namespace fs_testing

#endif  // HARNESS_SINGLE_TEST_INFO_H
