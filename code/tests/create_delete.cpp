#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include "../user_tools/api/actions.h"
#include "BaseTestCase.h"

using std::calloc;
using std::string;

using fs_testing::user_tools::api::Checkpoint;

#define TEST_FILE "test_file"
#define TEST_DIR "test_dir"
#define TEST_MNT "/mnt/snapshot"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))
#define TEST_TEXT_SIZE (1024 * 1024)
#define NUM_TEST_FILES (3)

namespace fs_testing {
namespace tests {

using std::memcmp;

int filesPresent(int i, std::vector<int> files_present){
  for (unsigned int j = 0; j < i; ++j) {
    if(files_present[j] == 0)
      return -1;
  }

  for(unsigned int j = i; j < NUM_TEST_FILES; ++j){
    if(files_present[j] < 0)
      return -1;   
  }
  return 0;
}

class create_delete : public BaseTestCase {
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
      fsync(fd);

      unsigned int written = 0;
      do {
        int res = write(fd, (void*) ((unsigned long) text + written),
            TEST_TEXT_SIZE - written);
        if (res == -1) {
          close(fd);
          return -1;
        }
        written += res;
      } while (written != TEST_TEXT_SIZE);
      fsync(fd);
      Checkpoint();
      close(fd);
    }

    for (unsigned int i = 0; i < NUM_TEST_FILES; ++i) {
      string file_to_remove = string(TEST_MNT "/" TEST_DIR "/" TEST_FILE
          + std::to_string(i));
      if(remove(file_to_remove.c_str()) < 0){
        return -1;
      }
      Checkpoint();
      sleep(6);
    }

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {
    std::vector<int> files_present;
    for (unsigned int i = 0; i < NUM_TEST_FILES; ++i) {
      const string file_name = string(TEST_MNT "/" TEST_DIR "/" TEST_FILE
          + std::to_string(i));
      struct stat stats;
      int res = stat(file_name.c_str(), &stats);
      files_present.push_back(res);
      if (res < 0) {
        continue;
      }
      if (!S_ISREG(stats.st_mode)) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        return 0;
      }
      if (((S_IRWXU | S_IRWXG | S_IRWXO) & stats.st_mode) != TEST_FILE_PERMS) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        return 0;
      }

      const int fd = open(file_name.c_str(), O_RDONLY);
      if (fd < 0) {
        test_result->SetError(DataTestResult::kOther);
        return 0;
      }

      unsigned int bytes_read = 0;
      char* buf = (char*) calloc(TEST_TEXT_SIZE, sizeof(char));
      if (buf == NULL) {
        test_result->SetError(DataTestResult::kOther);
      }
      do {
        res = read(fd, buf + bytes_read, TEST_TEXT_SIZE - bytes_read);
        if (res < 0) {
          free(buf);
          close(fd);
          test_result->SetError(DataTestResult::kOther);
          return 0;
        } else if (res == 0) {
          break;
        }
        bytes_read += res;
      } while (bytes_read < TEST_TEXT_SIZE);
      close(fd);

      if(last_checkpoint >= i+1 && i>0){
        if (bytes_read != TEST_TEXT_SIZE) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);
        } else if (memcmp(text, buf, TEST_TEXT_SIZE) != 0) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);
        }
      }

      free(buf);
    }


    for (int k=0;k < NUM_TEST_FILES; k++){
      string present = (files_present[k] == 0)? "present" : "not present";
      std::cout << "File "<< k << " " << present << std::endl;
    }
    std::cout << "\n\n" << std::endl;

    for (unsigned int i = 1; i <= NUM_TEST_FILES; ++i) {
      if(last_checkpoint == i+NUM_TEST_FILES-1 && filesPresent(i, files_present) < 0){
        test_result->SetError(DataTestResult::kOldFilePersisted);
      }
    }


    return 0;
  }

 private:
   char text[TEST_TEXT_SIZE];
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::create_delete;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
