/*
Reproducing fstest generic/376

1. mkdir test_dir
2. Create file foo in test_dir
3. fsync(test_dir)
4. fsync(foo)
5. rename test_dir/foo -> test_dir/bar
6. touch test_dir/foo
7. fsync(test_dir/bar)

If we rename a file, without changing its parent directory, create
a new file that has the old name of the file we renamed and do an fsync
against the file we renamed, after crash both foo and bar must exist. 

This is tested to fail on btrfs (kernel 4.4) (https://patchwork.kernel.org/patch/9297215/)
File foo is lost.
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
#define TEST_FILE_FOO "foo"
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR "test_dir"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic376: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test directory.
    int res = mkdir(TEST_MNT "/" TEST_DIR, 0777);
    if (res < 0) {
      return -1;
    }

    const int dir = open(TEST_MNT "/" TEST_DIR, O_RDONLY);
    if (dir < 0) {
      return -2;
    }

    //Create file foo in TEST_DIR
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    //Sync the new directory
    res = fsync(dir);
    if (res < 0) {
      return -4;
    }
    close(dir);

    //Sync the file created
    res = fsync(fd_foo);
    if (res < 0) {
      return -5;
    }
    close(fd_foo);

    //Just sync to force all changes to disk before snapshot
    sync();

    return 0;
  }

  virtual int run(int checkpoint) override {

    int local_checkpoint = 0;
    //Rename foo to bar in TEST_DIR
    if (rename(foo_path.c_str(), bar_path.c_str()) < 0) {
      return -1;
    }

    //Open file_bar
    const int fd_bar = open(bar_path.c_str(), O_RDWR);
    if (fd_bar < 0) {
      return -2;
    }

    //Now create file_foo again in TEST_DIR
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    //fsync only file_bar
    int res = fsync(fd_bar);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
    }
    //Close open files  
    close(fd_bar);
    close(fd_foo);
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    struct dirent *dir_entry;
    bool foo_present = false;
    bool bar_present = false;

    DIR *dir = opendir(TEST_MNT "/" TEST_DIR);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        //if (dir_entry->d_type == DT_REG){
          if (strcmp(dir_entry->d_name, "foo") == 0){
            foo_present = true;
            //std::cout << "File foo present" << std::endl;
          }
          else if (strcmp(dir_entry->d_name, "bar") == 0){
            bar_present = true;
            //std::cout << "File bar present" << std::endl;
          }
          else ;
        //}
      }
    }

    closedir(dir);

    // If the last seen checkpoint is 1, i.e after fsync(file_bar)
    // then both the files foo and bar must be present in test_dir
    if (last_checkpoint == 1 && (!foo_present || !bar_present)){
    	test_result->SetError(DataTestResult::kFileMissing);
      if(!foo_present && !bar_present)
        test_result->error_description = " : " + foo_path + " and " + bar_path + " missing";      
      else if(!foo_present)
        test_result->error_description = " : " + foo_path + " missing";
      else 
        test_result->error_description = " : " + bar_path + " missing";
    	return 0;
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR "/" TEST_FILE_FOO;
    const string bar_path = TEST_MNT "/" TEST_DIR "/" TEST_FILE_BAR;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic376;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
