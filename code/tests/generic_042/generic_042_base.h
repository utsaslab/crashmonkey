#ifndef TESTS_GENERIC_042_BASE_H
#define TESTS_GENERIC_042_BASE_H

#include <cstdint>

#include <string>

#include "../BaseTestCase.h"

namespace fs_testing {
namespace tests {

class Generic042Base: public BaseTestCase {
 public:
  virtual int setup() override;
  virtual int run() override;
  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) = 0;

 protected:
  /*
   * The actual test cases that CrashMonkey runs must have a zero-arg
   * constructor since it is called from the ClassLoader. However, that
   * restriction doesn't apply to base classes of the test cases themselves.
   * Therefore, we can specialize a base test case by having the derived class
   * pass the base class constructor args like here.
   */
  Generic042Base(int mode);

  /*
   * Checks the size of the file.
   *
   * Returns zero if the file size is zero. Returns 1 if the file size is equal
   * to the amount of data written to the file in run(). Returns a value < 0 if
   * something went wrong.
   */
  int CheckBase(DataTestResult *test_result);

  /*
   * Checks that the first kFallocOffset bytes in the file are 0xff. Returns 0
   * if this is true, else returns a value < 0.
   */
  int CheckDataBase(DataTestResult *test_result);

  /*
   * Checks that the bytes starting at kFallocOffset and continuing for
   * kFallocSize bytes are 0xff. Returns 0 if this is the case, else returns a
   * value < 0.
   */
  int CheckDataNoZeros(DataTestResult *test_result);

  /*
   * Checks that the bytes starting at kFallocOffset and continuing for
   * kFallocSize bytes are 0x00. Returns 0 if this is the case, else returns a
   * value < 0.
   */
  int CheckDataWithZeros(DataTestResult *test_result);

 private:
  int falloc_mode_;

  /*
   * Read len bytes of file data starting at offset and 'and' and 'or' the
   * values of that data into bit_and and bit_or respectively. If something goes
   * wrong, return a value < 0 and update test_result. Otherwise, test_result is
   * unmodified and the function returns 0.
   */
  int ReadData(DataTestResult *test_result, int fd, unsigned int offset,
      unsigned int len, uint32_t *bit_and, uint32_t *bit_or);

  /*
   * Place the output of `hexdump -C <path>` in the output string. Returns 0 on
   * success and < 0 on failure.
   */
  int HexdumpFile(const std::string &path, std::string &output);
};

}  // namespace tests
}  // namespace fs_testing

#endif  // TESTS_GENERIC_042_BASE_H
