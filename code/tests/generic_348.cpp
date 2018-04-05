/*
Reproducing fstest generic/348

1. Create dir A in TEST_MNT and sync it
2. Create file foo in TEST_MNT
3. Create a symbolic link for file foo1 in A/bar1
4. fsync the directory : fsync(TEST_MNT/A)

If we power fail and mount the filesystem again, symlink bar1 should exist and its
content must match what we specified when we created it(i.e a readlink(bar1) 
should point to foo1 and must not be empty or point to something else).

This is tested to fail on btrfs (kernel 4.4) (https://patchwork.kernel.org/patch/8938991/)
(https://patchwork.kernel.org/patch/9158353/) (Patched in 4.5)

In btrfs, the file bar1 exists, but is empty.
*/


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO1 "foo1"
#define TEST_FILE_FOO2 "foo2"
#define TEST_FILE_BAR1 "bar1"
#define TEST_FILE_BAR2 "bar2"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"
#define TEST_DIR_B "test_dir_b"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic348: public BaseTestCase {
 public:

  virtual int setup() override {

    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }

    //Sync everything
    sync();
    return 0;
  }

  virtual int run(int checkpoint) override {
    int local_checkpoint = 0;

    // Create a symlink for foo1 in TEST_DIR_A/bar1 
    // Case 1 : Parent directory is durably persisted
    if (symlink(foo1_path.c_str(), bar1_path.c_str()) < 0){
      return -1;
    }

    const int dir_a = open(TEST_MNT "/" TEST_DIR_A, O_RDONLY);
    if (dir_a < 0) {
      return -2;
    }

    //Sync the parent directory of the symlink - TEST_DIR_A
    if (fsync(dir_a) < 0){
      return -3;
    }

    int res = mkdir(TEST_MNT "/" TEST_DIR_B, 0777);
    if (res < 0) {
      return -4;
    }

    //Create a symlink for foo2 in TEST_DIR_B/bar2
    //Parent directory is new. Not persisted yet
    if (symlink(foo2_path.c_str(), bar2_path.c_str()) < 0){
      return -5;
    }

    const int dir_b = open(TEST_MNT "/" TEST_DIR_B, O_RDONLY);
    if (dir_b < 0) {
      return -6;
    }

    //fsync the parent directory
    if (fsync(dir_b) < 0){
      return -7;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    // If we readlink now, we should see foo1 and foo2 path in bar1 and bar2 respectively
    if (Checkpoint() < 0){
      return -8;
    }
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    //close
    close(dir_a);
    close(dir_b);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    std::string path1 = ReadLink(bar1_path);
    std::string path2 =  ReadLink(bar2_path);
    bool link1_empty = false;
    bool link2_empty = false;

    /*if(path1.empty())
      std::cout <<"Bar 1 : Link empty" << std::endl;
    else
      std::cout <<"Bar 1 : " << path1 << std::endl;    

    if(path2.empty())
      std::cout <<"Bar 2 : Link empty" << std::endl;
    else      
      std::cout <<"Bar 2 : " << path2 << std::endl;*/

    // After both parent directories are synced, if we read contents of
    // the symlink bar1 and bar2, it should point to foo1 and foo2 resp
    if (last_checkpoint == 1 && (path1.empty() || path2.empty())){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      if(path1.empty() && path2.empty())
        test_result->error_description = " : " + bar1_path + ", " + bar2_path + " link empty"; 
      else if (path1.empty())
        test_result->error_description = " : " + bar1_path + " link empty";
      else
        test_result->error_description = " : " + bar1_path + " link empty";
    }

    return 0;
  }

  std::string ReadLink(std::string const& path) {
    char buf[PATH_MAX];
    char* error = "readlink error";  

    /* buf not null terminated. So zero the buffer. */
    memset(buf, 0, sizeof(buf));

    /* Use sizeof(buf)-1  as we need to null terminate */
    if (readlink(path.c_str(), buf, sizeof(buf)-1) < 0) {
      memcpy(buf, error, strlen(error));
      return buf;
    }

    return string(buf);
  }

   private:
    const string foo1_path = TEST_MNT "/" TEST_FILE_FOO1;
    const string foo2_path = TEST_MNT "/" TEST_FILE_FOO2;    
    const string bar1_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_BAR1;
    const string bar2_path = TEST_MNT "/" TEST_DIR_B "/" TEST_FILE_BAR2;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic348;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
