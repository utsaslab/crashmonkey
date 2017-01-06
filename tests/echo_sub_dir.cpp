#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "../test_case.h"

#define TEXT "This is some string of characters that is going to be at least" \
  " 256 characters long. It is going to be some very random sentences that " \
  "probaby won't make sense in any other context or any context at all. It " \
  "really doesn't matter though because all I want to do is see if this test " \
  "is working properly or not so there.\n"
#define TEST_FILE "test_file"
#define TEST_DIR "test_dir"
#define TEST_MNT "/mnt/snapshot"

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {

using std::strlen;

class echo_sub_dir : public test_case {
 public:
  virtual int setup() override {
    int res = mkdir(TEST_MNT "/" TEST_DIR, 0777);
    if (res < 0) {
      return -1;
    }
    int dir = open(TEST_MNT "/" TEST_DIR, O_RDONLY);
    if (res < 0) {
      return -1;
    }
    res = fsync(dir);
    if (res < 0) {
      return -1;
    }
    close(dir);
    return 0;
  }

  virtual int run() override {
    int fd = open(TEST_MNT "/" TEST_DIR "/" TEST_FILE, O_RDWR | O_CREAT,
        S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1) {
      return -1;
    }

    unsigned int str_len = strlen(TEXT) * sizeof(char);
    unsigned int written = 0;
    while (written != str_len) {
      written = write(fd, TEXT + written, str_len - written);
      if (written == -1) {
        goto out;
      }
    }
    close(fd);
    return 0;

   out:
    close(fd);
    return -1;
  }
};

}  // namespace fs_testing

extern "C" fs_testing::test_case* get_instance() {
  return new fs_testing::echo_sub_dir;
}

extern "C" void delete_instance(fs_testing::test_case* tc) {
  delete tc;
}
