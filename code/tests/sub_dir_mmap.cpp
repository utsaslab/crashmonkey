#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <string>

#include "BaseTestCase.h"

using std::calloc;
using std::string;

#define TEST_FILE "test_file"
#define TEST_DIR "test_dir"
#define TEST_MNT "/mnt/snapshot"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))
#define TEST_TEXT_SIZE (1024)
#define NUM_TEST_FILES (1)

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

using std::memcmp;

class echo_sub_dir_big : public BaseTestCase {
 public:
  virtual int setup() override {
    int res = mkdir(TEST_MNT "/" TEST_DIR, 0777);
    if (res < 0) {
      return -1;
    }
    const int dir = open(TEST_MNT "/" TEST_DIR, O_RDONLY);
    if (dir < 0) {
      return -1;
    }
    res = fsync(dir);
    if (res < 0) {
      return -1;
    }
    close(dir);

    // Get text for the actual test.
    int rand_fd = open("/dev/urandom", O_RDONLY);
    if (rand_fd < 0) {
      return -1;
    }

    unsigned int bytes_read = 0;
    do {
      int res = read(rand_fd, (void*) ((unsigned long) text + bytes_read),
          TEST_TEXT_SIZE - bytes_read);
      if (res < 0) {
        close(rand_fd);
        return -1;
      }
      bytes_read += res;
    } while (bytes_read < TEST_TEXT_SIZE);
    close(rand_fd);

    return 0;
  }

  virtual int run() override {
    for (unsigned int i = 0; i < NUM_TEST_FILES; ++i) {
      const int old_umask = umask(0000);
      string file_name = string(TEST_MNT "/" TEST_DIR "/" TEST_FILE
          + std::to_string(i));
      const int fd = open(file_name.c_str(), O_RDWR | O_CREAT,
          TEST_FILE_PERMS);
      if (fd == -1) {
        umask(old_umask);
        return -1;
      }
      umask(old_umask);

      if (ftruncate(fd, TEST_TEXT_SIZE) < 0) {
        return -1;
      }

      void* file_data = mmap(NULL, TEST_TEXT_SIZE, PROT_WRITE, MAP_SHARED, fd,
          0);
      if (file_data <= 0) {
        return -1;
      }
      memcpy(file_data, text, TEST_TEXT_SIZE);
      munmap(file_data, TEST_TEXT_SIZE);

      close(fd);
    }
    return 0;
  }

  virtual int check_test() override {
    int res2 = 0;
    for (unsigned int i = 0; i < NUM_TEST_FILES; ++i) {
      struct stat stats;
      const string file_name = string(TEST_MNT "/" TEST_DIR "/" TEST_FILE
          + std::to_string(i));
      int res = stat(file_name.c_str(), &stats);
      if (res < 0) {
        return -1;
      }
      if (!S_ISREG(stats.st_mode)) {
        return -1;
      }
      if (((S_IRWXU | S_IRWXG | S_IRWXO) & stats.st_mode) != TEST_FILE_PERMS) {
        return -1;
      }

      const int fd = open(file_name.c_str(), O_RDONLY);
      if (fd < 0) {
        return -1;
      }

      unsigned int bytes_read = 0;
      char* buf = (char*) calloc(TEST_TEXT_SIZE, sizeof(char));
      if (buf == NULL) {
        return -2;
      }
      do {
        res = read(fd, buf + bytes_read, TEST_TEXT_SIZE - bytes_read);
        if (res < 0) {
          free(buf);
          close(fd);
          return -1;
        } else if (res == 0) {
          break;
        }
        bytes_read += res;
      } while (bytes_read < TEST_TEXT_SIZE);
      close(fd);

      if (bytes_read != TEST_TEXT_SIZE) {
        res2 = -1;
      } else if (memcmp(text, buf, TEST_TEXT_SIZE) != 0) {
        res2 = -1;
      }

      free(buf);
    }
    return res2;
  }

 private:
   char text[TEST_TEXT_SIZE];
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::echo_sub_dir_big;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
