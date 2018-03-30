/*
 * Variant of fstest generic/042 that uses unaligned fallocate
 * 1. Fill the file system with known data so that later files will have old,
 *    uncleared data in them (setup)
 * 2. Delete the file used to fill the file system with data (setup)
 * 3. Touch the file used in run() so we know it should always exist (setup)
 * 4. Write 65k of known data to file
 * 5. falloc a 4k chunk of file at offset 60k + 128
 * 6. Crash
 *
 *
 * For reordering replays that don't have all/most operations:
 * The file should be zero length
 *
 * For reordering replays that have all/most operations:
 * The file should be 65k with the known data written in (4)
 *
 * For in-order replays:
 * There are no Checkpoint()s, this should be the same as the above case
 */

#include "generic_042_base.h"

namespace fs_testing {
namespace tests {

class Generic042FallocateUnaligned: public Generic042Base {
 public:
   Generic042FallocateUnaligned() :
     Generic042Base((1024 * 65), ((1024 * 60) + 128), 4096, 0) {}

   int check_test(unsigned int last_checkpoint, DataTestResult *test_result)
       override {
     int res = CheckBase(test_result);
     if (res <= 0) {
       // Either something went wrong or the file size was 0.
       return res;
     }

     return CheckDataNoZeros(0, start_file_size_, test_result);
   }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic042FallocateUnaligned;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
