#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>

#include "../test_case.h"

using std::calloc;

#define TEXT "hello great big world out there\n"
#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"
#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {

using std::cerr;
using std::strlen;

class echo_root_dir : public test_case {
 public:
  virtual int setup() override {
    return 0;
  }

  virtual int run() override {
    const int old_umask = umask(0000);
    const int fd = open(TEST_MNT "/" TEST_FILE, O_RDWR | O_CREAT,
        TEST_FILE_PERMS);
    if (fd == -1) {
      umask(old_umask);
      cerr << "Error opening test file\n";
      return -1;
    }
    umask(old_umask);

    const unsigned int str_len = strlen(TEXT);
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

  virtual int check_test() override {
    struct stat stats;
    int res = stat(TEST_MNT "/" TEST_FILE, &stats);
    if (res < 0) {
      return -1;
    }
    if (!S_ISREG(stats.st_mode)) {
      return -1;
    }
    if (((S_IRWXU | S_IRWXG | S_IRWXO) & stats.st_mode) != TEST_FILE_PERMS) {
      return -1;
    }
    const int fd = open(TEST_MNT "/" TEST_DIR "/" TEST_FILE, O_RDONLY);
    if (fd < 0) {
      return -1;
    }
    int size = strlen(TEXT);
    int bytes_read = 0;
    char* buf = (char*) calloc(size, sizeof(char));
    if (buf == NULL) {
      return -2;
    }
    do {
      res = read(fd, buf + bytes_read, size - bytes_read);
      if (res < 0) {
        free(buf);
        close(fd);
        return -1;
      }
      bytes_read += res;
    } while (bytes_read < size);
    free(buf);
    close(fd);
    return 0;
  }
};

}  // namespace fs_testing

extern "C" fs_testing::test_case* get_instance() {
  return new fs_testing::echo_root_dir;
}

extern "C" void delete_instance(fs_testing::test_case* tc) {
  delete tc;
}
