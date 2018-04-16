/*
Reproducing xfstest generic/177

1. Create file foo and write some contents 0-128K into it and fsync it
2. punch_hole 96K-128K to file foo
3. punch_hole 64K - 192K to file foo
4. punch_hole 32K - 128K to file foo
5. fsync file foo

After a crash, contents of file foo must be persisted and correctly preserve holes.

https://patchwork.kernel.org/patch/7536021/

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
#include <sys/mman.h>
#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_FILE_FOO_BACKUP "foo_backup"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic177: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();


    const int fd_Afoo = open(foo_backup_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_Afoo < 0) {
      return -1;
    }

    // Write some contents to the backup file (for verifying md5sum in check_test)
    if (WriteData(fd_Afoo, 0, 131072) < 0) {
    	return -2;
    }

    sync();
 
    if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 98304, 32768) < 0)
	return -3;

     if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 65536, 131072) < 0)
        return -4;

     if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 32786, 98304) < 0)
        return -5; 
    
     sync();
     close(fd_Afoo);

    return 0;
  }

  virtual int run() override {

	init_paths();

    const int fd_Afoo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_Afoo < 0) {
      return -1;
    }
     
    if (WriteData(fd_Afoo, 0, 131072) < 0) {
        return -2; 
    }   

    fsync(fd_Afoo);
 
    if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 98304, 32768) < 0)
        return -3; 

     if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 65536, 131072) < 0)
        return -4;

     if (fallocate(fd_Afoo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 32786, 98304) < 0)
        return -5;
    
     fsync(fd_Afoo);

     if (Checkpoint() < 0)
        return -1;

     close(fd_Afoo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

        if (last_checkpoint >= 1){
		string expected_md5sum = get_md5sum(foo_backup_path);
		string actual_md5sum = get_md5sum(foo_path);

		if (expected_md5sum.compare(actual_md5sum) != 0 ) {
        		test_result->SetError(DataTestResult::kFileDataCorrupted);
        		test_result->error_description = " : md5sum of foo does not match with expected md5sum of foo backup";
		}
	}

    return 0;
  }

   private:
    string foo_path;
    string foo_backup_path;
    
    void init_paths() {
        foo_path = mnt_dir_ +  "/" TEST_FILE_FOO;
        foo_backup_path = mnt_dir_ +  "/" TEST_FILE_FOO_BACKUP;
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
  return new fs_testing::tests::Generic177;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
