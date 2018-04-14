/*
Reproducing xfstest generic/056

1. Create file foo and write some data and fsync the file
2. Create a hard link to the file
3. Create another file bar and fsync it

After a crash, the file foo should persist the contents that was fsynced.

https://patchwork.kernel.org/patch/5822681/
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
#define TEST_FILE_FOO_BACKUP "foo_backup"
#define TEST_FILE_BAR "bar"
#define TEST_FILE_FOO_LINK "foo_link_"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic056: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    // Create test directory A.
	string dir_path = mnt_dir_ + "/" TEST_DIR_A;
    int res = mkdir(dir_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }
    const int fd_foo_backup = open(foo_backup_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo_backup < 0) {
      return -1;
    }

    if (WriteData(fd_foo_backup, 0, 4096) < 0) {
    	return -2;
    }

    sync();
    close(fd_foo);
    close(fd_foo_backup);

    return 0;
  }

  virtual int run() override {

	init_paths();
  
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    // Write some contents to the file and the backup file
    if (WriteData(fd_foo, 0, 4096) < 0) {
    	return -2;
    }

    if (fsync(fd_foo) < 0) {
	return -3;
    }

    //After this checkpoint, foo should have the same data as foo_backup.	
    if (Checkpoint() < 0){
      return -4;
    }	

    // Create a new hard link
    if(link(foo_path.c_str(), foo_link_path.c_str()) < 0){
      return -5;
    }

    const int fd_bar = open(bar_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_bar < 0) {
      return -1;
    }

    if (fsync(fd_bar) < 0) {
    	return -3;
    }

    // Make a user checkpoint here
    if (Checkpoint() < 0){
      return -4;
    }

    //Close open files  
    close(fd_foo);
    close(fd_bar);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
        test_result->SetError(DataTestResult::kFileMissing);
        test_result->error_description = " : foo file missing after crash";
        return 0;
    }

    close(fd_foo);

    // If checkpoint >= 1, md5sum of foo should match with md5sum of foo_backup
     if (last_checkpoint >= 1){
	string expected_md5sum = get_md5sum(foo_backup_path);
	string actual_md5sum = get_md5sum(foo_path);
	if (expected_md5sum.compare(actual_md5sum) != 0) {
        test_result->SetError(DataTestResult::kFileDataCorrupted);
        test_result->error_description = " : md5sum of foo not maching with foo backup";
		return 0;
	}
      }

    return 0;
  }

   private:
    string foo_path;
    string foo_backup_path;
    string foo_link_path;
    string bar_path;
    
    void init_paths() {
        foo_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO;
        foo_backup_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO_BACKUP;
        foo_link_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
        bar_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_BAR;
    }

    string get_md5sum(string file_name) {
        FILE *fp;
        string command = "md5sum " + file_name;

        fp = popen(command.c_str(), "r");
        char md5[100];
        fscanf(fp,"%s", md5);
        fclose(fp);

        string md5_str(md5);
        return md5_str;
    }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic056;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
