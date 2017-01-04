#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "../test_case.h"

#define TEXT "hello great big world out there\n"
#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"

namespace fs_testing {

using std::cerr;
using std::strlen;

class echo_root_dir : public test_case {
 public:
  virtual int setup() override {
    return 0;
  }

  virtual int run() override {
    int fd = open(TEST_MNT "/" TEST_FILE, O_RDWR | O_CREAT);
    if (fd == -1) {
      cerr << "Error opening test file\n";
      return -1;
    }

    unsigned int str_len = strlen(TEXT) * sizeof(char);
    unsigned int written = 0;
    while (written != str_len) {
      written = write(fd, TEXT + written, str_len - written);
      if (written == -1) {
        cerr << "Error while writing to test file\n";
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
  return new fs_testing::echo_root_dir;
}

extern "C" void delete_instance(fs_testing::test_case* tc) {
  delete tc;
}
