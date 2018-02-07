/*
Reproducing fstest generic/336

1. Create directories A, B and C under TEST_MNT.
2. Create file foo in dir A
3. Create a link for foo in B/foo_link
4. Create another file bar in dir B.
5. sync all changes so far
6. Remove the link for foo in dir B
7. Move file bar from dir B to C
8. fsync(A/foo)

If a power fail occurs now, and remount the filesystem, 
file bar exists and should be located only in directory C.

This is tested to fail on btrfs(kernel 4.4) (https://patchwork.kernel.org/patch/8312691/)
(https://patchwork.kernel.org/patch/8293181/).

File bar is lost in btrfs during rename - neither present in dir B nor C.
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
#define TEST_FILE_FOO_LINK "foo_link"
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"
#define TEST_DIR_B "test_dir_b"
#define TEST_DIR_C "test_dir_c"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic336: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test directory A.
    int res = mkdir(TEST_MNT "/" TEST_DIR_A, 0777);
    if (res < 0) {
      return -1;
    }

    // Create test directory B.
    res = mkdir(TEST_MNT "/" TEST_DIR_B, 0777);
    if (res < 0) {
      return -1;
    }

    // Create test directory C.
    res = mkdir(TEST_MNT "/" TEST_DIR_C, 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_A (A has foo)
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    //Link TEST_DIR_A/foo to TEST_DIR_B/foo_link (B has foo_link)
    if (link(foo_path.c_str(), foo_link_path.c_str()) < 0){
      return -4;
    }

    //touch TEST_DIR_B/bar (B has foo_link and bar)
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -5;
    }

    //Sync everything
    sync();

    close(fd_foo);
    close(fd_bar);

    return 0;
  }

  virtual int run() override {

    //Unlink TEST_DIR_B/foo_link (B has bar)
    if (unlink(foo_link_path.c_str()) < 0){
      return -1;
    }

    //Move TEST_DIR_B/bar to TEST_DIR_C/bar (B is empty, C has bar, A has foo)
    if (rename(bar_path.c_str(), bar_path_moved.c_str()) < 0) {
      return -2;
    }

    //Open TEST_DIR_A/foo whose link count was decremented
    const int fd_foo = open(foo_path.c_str(), O_RDWR);
    if (fd_foo < 0) {
      return -3;
    }

    //fsync only file_foo
    int res = fsync(fd_foo);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
    }

    //Close open files  
    close(fd_foo);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    /*std::cout << "Dir A :" << std::endl;
    system("ls /mnt/snapshot/test_dir_a");
    std::cout << "Dir B :" << std::endl;    
    system("ls /mnt/snapshot/test_dir_b");
    std::cout << "Dir C :" << std::endl;    
    system("ls /mnt/snapshot/test_dir_c");*/

    struct stat stats_old;
    struct stat stats_new;
    const int stat_old_res = stat(bar_path.c_str(), &stats_old);
    const int errno_old = errno;
    const int stat_new_res = stat(bar_path_moved.c_str(), &stats_new);
    const int errno_new = errno;

    // Neither stat found the file, it's gone...
    if (stat_old_res < 0 && errno_old == ENOENT &&
        stat_new_res < 0 && errno_new == ENOENT) {
      test_result->SetError(DataTestResult::kFileMissing);
      test_result->error_description = " : " + bar_path + ", " + bar_path_moved + " missing";
      return 0;
    }

    //Bar is present at both directories
    if (stat_old_res >= 0 && stat_new_res >= 0) {
      test_result->SetError(DataTestResult::kOldFilePersisted);
      test_result->error_description = " : " + bar_path + " still present";
      return -1;
    }

    struct dirent *dir_entry;
    bool foo_present_in_A = false;
    bool bar_present_in_C = false;
    bool empty_B = true;

    DIR *dir = opendir(TEST_MNT "/" TEST_DIR_A);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        //if (dir_entry->d_type == DT_REG){
          if (strcmp(dir_entry->d_name, "foo") == 0){
            foo_present_in_A = true;
          }
        //}
      }
    }

    closedir(dir);

    dir = opendir(TEST_MNT "/" TEST_DIR_B);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        if (dir_entry->d_type == DT_REG){
          //std::cout << dir_entry->d_name <<std::endl;
          if ((strcmp(dir_entry->d_name, "foo_link") == 0)||(strcmp(dir_entry->d_name, "bar") ==0)){
            empty_B = false;
          }
        }
      }
    }

    closedir(dir);

    dir = opendir(TEST_MNT "/" TEST_DIR_C);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        if (dir_entry->d_type == DT_REG){
          if (strcmp(dir_entry->d_name, "bar") == 0){
            bar_present_in_C = true;
          }
        }
      }
    }

    closedir(dir);

    // If the last seen checkpoint is 1, i.e after fsync(file_foo)
    // then both the files foo must be present in A, bar in C, B empty
    if (last_checkpoint == 1 && (!foo_present_in_A || !bar_present_in_C || !empty_B)){
    	test_result->SetError(DataTestResult::kFileMissing);
      if(!empty_B)
        test_result->error_description = " : " + string(TEST_DIR_B) + " not empty";      
      else if(!foo_present_in_A)
        test_result->error_description = " : " + foo_path + " missing";
      else if(!bar_present_in_C)
        test_result->error_description = " : " + bar_path + " missing";
    	else ;
      return 0;
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_A "/" TEST_FILE_FOO;
    const string foo_link_path = TEST_MNT "/" TEST_DIR_B "/" TEST_FILE_FOO_LINK;    
    const string bar_path = TEST_MNT "/" TEST_DIR_B "/" TEST_FILE_BAR;
    const string bar_path_moved = TEST_MNT "/" TEST_DIR_C "/" TEST_FILE_BAR;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic336;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
