/*
Reproducing fstest generic/343

1. Create dir X and Y in TEST_MNT 
2. Create a file foo in X
3. Create dir Z in dir Y.
4. Create another file foo2 in dir Y
5. Persist all changes so far - sync()
6. Create a link for X/foo in X/bar
7. Move dir Z from Y to X 
8. Move file foo2 from Y to X
9. fsync(X/foo)

If  we crash now and recover, directory Z and file foo2 must be present only in dir X

This is tested to fail on btrfs (kernel 4.4) (https://www.spinics.net/lists/linux-btrfs/msg53907.html
https://patchwork.kernel.org/patch/8766401/)

In btrfs, the file foo2 and dir Z are present in both X and Y.
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
#define TEST_FILE_FOO_2 "foo_2"
#define TEST_FILE_FOO_LINK "foo_link"
#define TEST_FILE_BAR "bar"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_X "test_dir_x"
#define TEST_DIR_Y "test_dir_y"
#define TEST_DIR_Z "test_dir_z"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic343: public BaseTestCase {
 public:
  virtual int setup() override {

    // Create test directory X.
    int res = mkdir(TEST_MNT "/" TEST_DIR_X, 0777);
    if (res < 0) {
      return -1;
    }

    // Create test directory Y.
    res = mkdir(TEST_MNT "/" TEST_DIR_Y, 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_X (X has foo)
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    // Create test directory Z in Y. (Y has Z)
    res = mkdir(TEST_MNT "/" TEST_DIR_Y "/" TEST_DIR_Z, 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo_2 in TEST_DIR_Y (Y has foo_2, Z)
    const int fd_foo_2 = open(foo_2_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    //Sync everything
    sync();

    close(fd_foo);
    close(fd_foo_2);

    return 0;
  }

  virtual int run(int checkpoint) override {

  	int local_checkpoint = 0;
    //X has foo, Y has foo_2 and Z

  	//Now add a link to foo at directory x
  	// X has foo and bar, Y has foo_2 and Z
  	if (link(foo_path.c_str(), bar_path.c_str()) < 0){
      return -1;
    }
    
    //move directory z from y to x
    //X has foo, bar and Z, Y has foo_2
    if (rename(TEST_MNT "/" TEST_DIR_Y "/" TEST_DIR_Z, 
    	TEST_MNT "/" TEST_DIR_X "/" TEST_DIR_Z) < 0) {
      return -2;
    }

    //move file foo2 from dir y to x
    //X has foo, bar, foo_2 and Z, Y is empty
    if (rename(foo_2_path.c_str(), foo_2_path_moved.c_str()) < 0) {
      return -3;
    }    

    //open foo
    const int fd_foo = open(foo_path.c_str(), O_RDWR);
    if (fd_foo < 0) {
      return -4;
    }

    //fsync only file_foo
    int res = fsync(fd_foo);
    if (res < 0){
      return -5;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point. 
    //We expect that after a log replay we see the new link for foo's inode(bar) 
    //and that z and foo_2 are only located at directory x.
    if (Checkpoint() < 0){
      return -5;
    }
    local_checkpoint += 1;
    if (local_checkpoint == checkpoint) {
      return 1;
    }

    //Close open files  
    close(fd_foo);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    /*std::cout << "Dir X :" << std::endl;
    system("ls /mnt/snapshot/test_dir_x");
    std::cout << "Dir Y :" << std::endl;    
    system("ls /mnt/snapshot/test_dir_y");*/

    //x must have z, foo, foo_2, bar

    struct stat stats_old;
    struct stat stats_new;
    const int stat_old_res = stat(foo_2_path.c_str(), &stats_old);
    const int errno_old = errno;
    const int stat_new_res = stat(foo_2_path_moved.c_str(), &stats_new);
    const int errno_new = errno;

    // Neither stat found the file, it's gone...
    if (stat_old_res < 0 && errno_old == ENOENT &&
        stat_new_res < 0 && errno_new == ENOENT) {
      test_result->SetError(DataTestResult::kFileMissing);
      test_result->error_description = " : " + foo_2_path + ", " + foo_2_path_moved + " missing";
      return 0;
    }

    //foo_2 is present at both directories
    if (stat_old_res >= 0 && stat_new_res >= 0) {
      test_result->SetError(DataTestResult::kOldFilePersisted);
      test_result->error_description = " : " + foo_2_path + " still present";
      return -1;
    }

    struct dirent *dir_entry;
    bool foo_present_in_X = false;
    bool bar_present_in_X = false;
    bool foo_2_present_in_X = false;
    bool z_present_in_X  = false;
    bool empty_B = true;
    bool foo_2_present_in_Y = false;
    bool z_present_in_Y = false;

    DIR *dir = opendir(TEST_MNT "/" TEST_DIR_X);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        //if (dir_entry->d_type == DT_REG || DT_DIR){
          if (strcmp(dir_entry->d_name, "foo") == 0){
          	foo_present_in_X = true;
          }
          else if (strcmp(dir_entry->d_name, "foo_2") == 0){
            foo_2_present_in_X = true;
          }
          else if (strcmp(dir_entry->d_name, "bar") == 0){
            bar_present_in_X = true;
          }
          else if (strcmp(dir_entry->d_name, "test_dir_z") == 0){
            z_present_in_X = true;
          }
          else ;
        //}
      }
    }

    closedir(dir);

    dir = opendir(TEST_MNT "/" TEST_DIR_Y);

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
        //if (dir_entry->d_type == DT_REG || DT_DIR){
        	if (strcmp(dir_entry->d_name, "foo_2") == 0){
          		foo_2_present_in_Y = true;
          	}
          	else if (strcmp(dir_entry->d_name, "test_dir_z") == 0){
            	z_present_in_Y = true;
         	}
         	else ;
        //}
      }
    }

    closedir(dir);

    // If the last seen checkpoint is 1, i.e after fsync(file_foo)
    // then all files must be in X and Y be empty
    bool X_valid = foo_present_in_X && foo_2_present_in_X && bar_present_in_X
    				&& z_present_in_X;
    bool Y_valid = !foo_2_present_in_Y && !z_present_in_Y;				
    if (last_checkpoint == 1 && !X_valid ){
    	test_result->SetError(DataTestResult::kFileMissing);
        test_result->error_description = " : " + string(TEST_DIR_X) + " has missing files";      
    	return 0;
    }

    if (last_checkpoint == 1 && !Y_valid) {
    	test_result->SetError(DataTestResult::kOldFilePersisted);    	
        test_result->error_description = " : " + string(TEST_DIR_Y) + " has old files";
      	return 0;
    }

    return 0;
  }

   private:
    const string foo_path = TEST_MNT "/" TEST_DIR_X "/" TEST_FILE_FOO;
    const string foo_2_path = TEST_MNT "/" TEST_DIR_Y "/" TEST_FILE_FOO_2;      
    const string bar_path = TEST_MNT "/" TEST_DIR_X "/" TEST_FILE_BAR;
    const string foo_2_path_moved = TEST_MNT "/" TEST_DIR_X "/" TEST_FILE_FOO_2;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic343;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
