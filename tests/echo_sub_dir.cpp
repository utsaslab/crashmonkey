#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "../test_case.h"

#define TEXT "hello great big world out there\n"
#define TEST_FILE "test_file"
#define TEST_DIR "test_dir"
#define TEST_MNT "/mnt/snapshot"

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {

using std::cout;
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
    cout << "Running run\n";
    int fd = open(TEST_MNT "/" TEST_DIR "/" TEST_FILE, O_RDWR | O_CREAT);
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
