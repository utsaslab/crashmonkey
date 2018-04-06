#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>

#include <iostream>

#include "BaseTestCase.h"

#define TEXT "hello great big world out there\n"
#define TEST_TEXT_SIZE (sizeof TEXT)
#define TEST_FILE "test_file"
#define TEST_DIR "test_dir"
#define TEST_MNT "/mnt/snapshot"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

using std::calloc;
using std::strlen;

class echo_sub_dir_no_sync : public BaseTestCase {
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
    return 0;
  }

  virtual int run(int checkpoint) override {
    const int old_umask = umask(0000);
    const int fd = open(TEST_MNT "/" TEST_DIR "/" TEST_FILE, O_RDWR | O_CREAT,
        TEST_FILE_PERMS);
    if (fd == -1) {
      umask(old_umask);
      return -1;
    }
    umask(old_umask);

    const unsigned int str_len = strlen(TEXT);
    unsigned int written = 0;
    while (written != str_len) {
      written = write(fd, TEXT + written, str_len - written);
      if (written < 0) {
        goto out;
      }
    }
    close(fd);
    return 1;

   out:
    close(fd);
    return -1;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {
    struct stat stats;
    int res = stat(TEST_MNT "/" TEST_DIR "/" TEST_FILE, &stats);
    if (res < 0) {
      test_result->SetError(DataTestResult::kFileMissing);
      return 0;
    }
    if (!S_ISREG(stats.st_mode)) {
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      return 0;
    }
    if (((S_IRWXU | S_IRWXG | S_IRWXO) & stats.st_mode) != TEST_FILE_PERMS) {
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      return 0;
    }
    const int fd = open(TEST_MNT "/" TEST_DIR "/" TEST_FILE, O_RDONLY);
    if (fd < 0) {
      test_result->SetError(DataTestResult::kOther);
      return 0;
    }
    int size = strlen(TEXT);
    int bytes_read = 0;
    char* buf = (char*) calloc(size, sizeof(char));
    if (buf == NULL) {
      test_result->SetError(DataTestResult::kOther);
      return 0;
    }
    do {
      res = read(fd, buf + bytes_read, size - bytes_read);
      if (res < 0) {
        free(buf);
        close(fd);
        test_result->SetError(DataTestResult::kOther);
        return 0;
      } else if (res == 0) {
        break;
      }
      bytes_read += res;
    } while (bytes_read < size);
    close(fd);

    if (bytes_read != size) {
      test_result->SetError(DataTestResult::kFileDataCorrupted);
    } else if (memcmp(TEXT, buf, TEST_TEXT_SIZE) != 0) {
      test_result->SetError(DataTestResult::kFileDataCorrupted);
    }

    free(buf);
    return 0;
  }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::echo_sub_dir_no_sync;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
