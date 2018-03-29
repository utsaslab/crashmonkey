/*
Reproducing fstest generic/059

1. Create file foo
2. Write data (offset 0 -16K) into the file foo
3. fsync(foo) -> fsync-1
4. sync()
5. Punch a hole in the file that affects only 1 page
6. fsync(foo) -> fsync-2


If a power fail occurs now, and remount the filesystem, 
the contents of file foo must match the state after fsync-2

This is tested to fail on btrfs(kernel 3.13) 
https://patchwork.kernel.org/patch/5830831/
https://www.spinics.net/lists/linux-btrfs/msg42047.html

File foo contents match the state after fsync-1. Changes due
to punch-hole are lost, even though there is a fsync after the operation.
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <errno.h>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_MNT "/mnt/snapshot"
#define TEST_TEXT_SIZE 16384

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic059: public BaseTestCase {
 public:
  virtual int setup() override {

    //Create file foo 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //Write 16KB of data to the file
    if(WriteData(fd_foo, 0, 16384) < 0){
      close(fd_foo);
      return -2;
    }

    //Fsync the file
    if(fsync(fd_foo) < 0){
      close(fd_foo);
      return -3;
    }

    //For later comparison, copy contents of foo into text.
    unsigned int bytes_read = 0;
    do {
      int res = read(fd_foo, (void*) ((unsigned long) text + bytes_read),
          TEST_TEXT_SIZE - bytes_read);
      if (res < 0) {
        close(fd_foo);
        return -1;
      }
      bytes_read += res;
    } while (bytes_read < TEST_TEXT_SIZE);

    //sync to perist everything so far
    sync();

    //close file foo
    close(fd_foo);
    sleep(2);

    return 0;
  }

  virtual int run(int checkpoint) override {
    //Open file foo
    const int fd_foo = open(foo_path.c_str(), O_RDWR);
    if (fd_foo < 0) {
      return -1;
    }

    //Punch hole from offset 8000 , for a length of 4K
    if(fallocate(fd_foo, FALLOC_FL_PUNCH_HOLE | 
      FALLOC_FL_KEEP_SIZE ,8000, 4096) < 0){
      close(fd_foo);
      return -2;
    }

    //fsync file_foo
    int res = fsync(fd_foo);
    if (res < 0){
      close(fd_foo);
      return -3;
    }

    // Make a user checkpoint here. Checkpoint must be 1 beyond this point
    // Beyond this point, the effect of punch_hole must be visible
    if (Checkpoint() < 0){
      close(fd_foo);
      return -4;
    }

    //Close open files  
    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    //If checkpoint < 1, we are not concerned.
    if(last_checkpoint >= 1){

      FILE* f = fopen(foo_path.c_str(), "rw");

      int res = 0; 
      unsigned int bytes_read = 0;
      char* buf = (char*) calloc(TEST_TEXT_SIZE, sizeof(char));
      if (buf == NULL) {
        test_result->SetError(DataTestResult::kOther);
      }


      bytes_read = fread(buf, 1, TEST_TEXT_SIZE, f);

      //std::cout << "Buf size = " << strlen(buf) << std::endl;
      //std::cout << "Bytes read = " << bytes_read << std::endl;
      if (bytes_read != TEST_TEXT_SIZE) {
        //error reading the file
        std::cout << "Error reading file" << std::endl;
        test_result->SetError(DataTestResult::kOther);
      }

      else {

        // After punch_hole, the number of characters in the 
        // file should be less than original
        if (strlen(buf) == TEST_TEXT_SIZE) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);          
          test_result->error_description = " : punch_hole not persisted even after fsync";
        }

        // If buf length is not the same as orginal, verify the 
        // contents of the file to check if this is the state after fsync
        // or not.
        else if (memcmp(text, buf, TEST_TEXT_SIZE) == 0) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);
          test_result->error_description = " : punch_hole not persisted even after fsync";
        }
      }

      fclose(f);
      free(buf);
  
    }
    return 0;
  }

   private:
    char text[TEST_TEXT_SIZE];
    const string foo_path = TEST_MNT "/" TEST_FILE_FOO;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic059;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
