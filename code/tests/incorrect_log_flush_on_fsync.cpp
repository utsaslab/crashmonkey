#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <string>
#include <bits/stdc++.h>

#include "BaseTestCase.h"
#include "../user_tools/api/actions.h"

using std::calloc;
using std::string;
using std::to_string;
using std::rotate;

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::Checkpoint;

#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR "test_dir"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

#define TEST_TEXT u8\
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define NO_OF_WRITES 10

namespace fs_testing {
namespace tests {

using std::memcmp;

// Note that user checkpoints start from index 1
// Total number of user_checkpoints in this test_case : NO_OF_CHECKPOINTS+2
class IncorrectLogFlushOnFsync : public BaseTestCase {
 public:

  virtual int setup() override {
    return 0;
  }

  virtual int run() override {
    // For fsyncs later.
    const int root_dir = open(TEST_MNT, O_RDONLY);
    if (root_dir < 0) {
      return -1;
    }

    // Create test directory.
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
    res = fsync(root_dir);
    if (res < 0) {
      return -1;
    }
    // Checkpoint indexed 1
    res = Checkpoint();
    if (res < 0) {
      return -1;
    }
    close(dir);

    // Create original file with permission etc.
    const int file_umask = umask(0000);
    const int fd = open(file_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd == -1) {
      umask(file_umask);
      return -1;
    }
    umask(file_umask);

    // some writes to files
    unsigned int noOfWrites = NO_OF_WRITES;
    string test_text = TEST_TEXT;
    do {
      res = write(fd, (void*) ((char*) test_text.c_str()), strlen(test_text.c_str()));
      if (res == -1) {
        close(fd);
        return -1;
      }
      fsync(fd);
      fsync(root_dir);
      // Creates user checkpoints indexed from 2 to NO_OF_WRITES+1 
      res = Checkpoint();
      if (res < 0) {
        return -1;
      }
      noOfWrites--;
      std::rotate(test_text.begin(), test_text.begin()+1, test_text.end());
    } while (noOfWrites > 0); 
    res = write(fd, "#", 1);
    fsync(fd);
    fsync(root_dir);
    // Checkpoint indexed NO_OF_WRITES+2
    res = Checkpoint();
    if (res < 0) {
      return -1;
    }
    close(fd);
    return 0;
  }

  // User checkpoints are indexed from 1
  // Total number of user_checkpoints possible NO_OF_WRITES+2
  // check_test checks if data persisted till the 'last_checkpoint'
  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_res) override {

    test_res->ResetError();
    const int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
      test_res->SetError(DataTestResult::kOther);
      test_res->error_description = file_path + ": unable to open file";
      return -1;
    }

    string test_text = TEST_TEXT;
    char* buf = (char*) calloc(strlen(test_text.c_str()), sizeof(char));
    if (buf == NULL) {
      test_res->SetError(DataTestResult::kOther);
      close(fd);
      return -1;
    }

    // If last_checkpoint is 0 or 1 implies we never got to writing data to file
    if (last_checkpoint <= 1) {
      test_res->SetError(DataTestResult::kOther);
      free(buf);
      close(fd);
      return -1;
    }
    last_checkpoint -= 1;
    
    // Now we have last_checkpoint number of TEST_TEXT writes written to the file
    unsigned int noOfWrites = (last_checkpoint < NO_OF_WRITES)? last_checkpoint : NO_OF_WRITES;
    while (noOfWrites > 0) {
      const int res = read(fd, buf, strlen(test_text.c_str()));
      // check if read is successful
      if (res < 0) {
        free(buf);
        close(fd);
        test_res->SetError(DataTestResult::kOther);
        test_res->error_description = file_path + ": error reading file";
        return -1;
      } else if (res == 0) {
        free(buf);
        close(fd);
        return -1;
      }
      // check if the string read is of the same length as that of the test_text
      // and compare if its exactly the same as test_text
      if (res != strlen(test_text.c_str())) {
        test_res->SetError(DataTestResult::kFileDataCorrupted);
        test_res->error_description =
          file_path + ": tried to read " + to_string(strlen(test_text.c_str()))
          + " bytes but only read " + to_string(res);
      } else {
        for (unsigned int i = 0; i < strlen(test_text.c_str()); ++i) {
          if (buf[i] != test_text[i]) {
            test_res->SetError(DataTestResult::kFileDataCorrupted);
            test_res->error_description =
              file_path + ": read data incorrect at index " + to_string(i);
            free(buf);
            close(fd);
            return -1;
          }
        }
      }
      noOfWrites--;
      std::rotate(test_text.begin(), test_text.begin()+1, test_text.end());
    } 
    // if # was written to the file and a user checkpoint was called, last_checkpoint will be 
    // greator than the NO_OF_WRITES
    if (last_checkpoint > NO_OF_WRITES) {
      const int res = read(fd, buf, 1);
      if(buf[0] != '#') {
        test_res->SetError(DataTestResult::kFileDataCorrupted);
        test_res->error_description = file_path + ": Didnt find \"#\" at the end of file";
        free(buf);
        close(fd);
        return -1;
      }
    }
    free(buf);
    close(fd);
    return 0;
  }

 private:
    char text[strlen(TEST_TEXT)];
    const string file_path = TEST_MNT "/" TEST_DIR "/" TEST_FILE;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::IncorrectLogFlushOnFsync;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
