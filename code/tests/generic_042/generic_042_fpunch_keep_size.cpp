/*
 * Reproducing fstest generic/042
 * 1. Fill the file system with known data so that later files will have old,
 *    uncleared data in them (setup)
 * 2. Delete the file used to fill the file system with data (setup)
 * 3. Touch the file used in run() so we know it should always exist (setup)
 * 4. Write 64k of known data to file
 * 5. punch a 4k hole in file at offset 60k
 * 6. Crash
 *
 *
 * For reordering replays that don't have all/most operations:
 * The file should be zero length
 *
 * For reordering replays that have all/most operations:
 * The file should be 64k with the known data written in (4) for the first 60k,
 * the final 4k should be zero due to the punch operation
 *
 * For in-order replays:
 * There are no Checkpoint()s, this should be the same as the above case
 */

#include <fcntl.h>

#include "generic_042_base.h"

namespace fs_testing {
namespace tests {

class Generic042FpunchKeepSize: public Generic042Base {
 public:
   Generic042FpunchKeepSize() :
     Generic042Base((1024 * 64), (1024 * 60), 4096,
         FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE) {}

   int check_test(unsigned int last_checkpoint, DataTestResult *test_result)
       override {
     int res = CheckBase(test_result);
     if (res <= 0) {
       // Either something went wrong or the file size was 0.
       return res;
     }

     res = CheckDataNoZeros(0, falloc_offset_, test_result);
     if (res < 0) {
       return res;
     }

     res = CheckDataWithZeros(falloc_offset_, falloc_len_, test_result);
     if (res < 0) {
       return res;
     }

     const unsigned int offset = falloc_offset_ + falloc_len_;
     return CheckDataNoZeros(offset, start_file_size_ - offset, test_result);
   }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic042FpunchKeepSize;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
