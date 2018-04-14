/*
Reproducing fstest generic/335

1. Create directories A/B and C under TEST_MNT.
2. Create file foo in dir A/B, and sync
3. Move file foo to dir C
4. Create another file bar in dir A
5. fsync dir A

If a power fail occurs now, and remount the filesystem, 
file bar should be present under A and file foo should be present under directory C.
 
https://patchwork.kernel.org/patch/8312681/
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


class Generic335: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    // Create test directory A.
	string dir_name = mnt_dir_ + "/" TEST_DIR_A;
    int res = mkdir(dir_name.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    // Create test directory B under A.
    dir_name = mnt_dir_ + "/" TEST_DIR_A "/" TEST_DIR_B;
    res = mkdir(dir_name.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    // Create test directory C.
    dir_name = mnt_dir_ + "/" TEST_DIR_C;
    res = mkdir(dir_name.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_A (A has foo)
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -3;
    }

    //Sync everything
    sync();

    close(fd_foo);

    return 0;
  }

  virtual int run() override {

	init_paths();

    //Move A/B/foo to C/foo (B is empty, C has foo, A is empty)
    if (rename(foo_path.c_str(), foo_path_moved.c_str()) < 0) {
      return -2;
    }

    //Create file bar in TEST_DIR_A (A has foo)
    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -3;
    }

    // fsync directory A
    string dir_name = mnt_dir_ + "/" TEST_DIR_A;
    int dir = open(dir_name.c_str(), O_RDONLY, O_DIRECTORY);
    if (dir < 0) {
    	return -4;
    }

    if (fsync(dir) < 0) {
    	return -5;
    }
    close(dir);

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -6;
    }

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    struct dirent *dir_entry;
    bool foo_present_in_A = false;
    bool foo_present_in_B = false;
    bool foo_present_in_C = false;
    bool bar_present_in_A = false;
    bool bar_present_in_B = false;
    bool bar_present_in_C = false;
//    bool empty_B = true;

    string dir_name = mnt_dir_ + "/" TEST_DIR_A "/";
    DIR *dir = opendir(dir_name.c_str());

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
		  if (strcmp(dir_entry->d_name, "foo") == 0){
			foo_present_in_A = true;
		  }
		  if (strcmp(dir_entry->d_name, "bar") == 0){
			bar_present_in_A = true;
		  }
      }
    }

    closedir(dir);

    dir_name = mnt_dir_ + "/" TEST_DIR_A "/" TEST_DIR_B "/";
    dir = opendir(dir_name.c_str());

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
		  if (strcmp(dir_entry->d_name, "foo") == 0){
			foo_present_in_B = true;
		  }
		  if (strcmp(dir_entry->d_name, "bar") == 0){
			bar_present_in_B = true;
		  }
      }
    }

    closedir(dir);

    dir_name = mnt_dir_ + "/" TEST_DIR_C + "/";
    dir = opendir(dir_name.c_str());

    if (dir) {
      //Get all files in this directory
      while ((dir_entry = readdir(dir)) != NULL) {
		  if (strcmp(dir_entry->d_name, "foo") == 0){
			foo_present_in_C = true;
		  }
		  if (strcmp(dir_entry->d_name, "bar") == 0){
			bar_present_in_C = true;
		  }
      }
    }

    closedir(dir);

    // foo is not present in B and C
    if (!foo_present_in_B && !foo_present_in_C) {
      test_result->SetError(DataTestResult::kFileMissing);
      test_result->error_description = " : " + foo_path + ", " + foo_path_moved + " missing";
      return 0;
    }

    // foo is present at both directories
    if (foo_present_in_B && foo_present_in_C) {
      test_result->SetError(DataTestResult::kOldFilePersisted);
      test_result->error_description = " : " + foo_path + " still present";
      return 0;
    }

    // If the last seen checkpoint is 1, i.e after fsync(A)
    // then both the file foo must be present in C and bar in A and B should be empty
    if (last_checkpoint == 1) {
    	if (!foo_present_in_C) {
        	test_result->SetError(DataTestResult::kFileMissing);
            test_result->error_description = " : foo not present in " + string(TEST_DIR_C);
    	} else if (!bar_present_in_A) {
        	test_result->SetError(DataTestResult::kFileMissing);
            test_result->error_description = " : bar not present in " + string(TEST_DIR_A);
    	} else if (foo_present_in_A || foo_present_in_B) {
        	test_result->SetError(DataTestResult::kOldFilePersisted);
            test_result->error_description = " : foo present in " + string(TEST_DIR_A) + " or " + string(TEST_DIR_B);
    	} else if (bar_present_in_B || bar_present_in_C) {
        	test_result->SetError(DataTestResult::kFileMetadataCorrupted);
            test_result->error_description = " : bar present in " + string(TEST_DIR_B) + " or " + string(TEST_DIR_C);
    	}
    }

    return 0;
  }

   private:
  	string foo_path;
  	string foo_path_moved;
  	string bar_path;

    void init_paths() {
        foo_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_DIR_B "/" TEST_FILE_FOO;
        foo_path_moved = mnt_dir_ + "/" TEST_DIR_C "/" TEST_FILE_FOO;
        bar_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_BAR;
    }

};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic335;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
