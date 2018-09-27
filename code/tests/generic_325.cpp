/*
Reproducing xfstest generic/325

1. Create file foo and write some contents 0-256K into it and sync it
2. mmap write 0-4K to file foo
3. mmap write 252K - 256K to file foo
4. msync both the regions

After a crash, contents of file foo must be persisted.

https://patchwork.kernel.org/patch/4824621/
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


class Generic325: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();


    const int fd_Afoo = open(foo_backup_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_Afoo < 0) {
      return -1;
    }

    // Write some contents to the backup file (for verifying md5sum in check_test)
    if (WriteData(fd_Afoo, 0, 262144) < 0) {
    	return -2;
    }

    sync();


    char *filep_Afoo = (char *) mmap(NULL, 262144, PROT_WRITE|PROT_READ, MAP_SHARED, fd_Afoo, 0);
    if (filep_Afoo == MAP_FAILED) {
	return -1;
    }

    int offset_Afoo = 0;
    int to_write_Afoo = 4096 ;
    const char *text_Afoo  = "12345678123456781234567812345678";

    while (offset_Afoo < 4096){
	if (to_write_Afoo < 32){
		memcpy(filep_Afoo + offset_Afoo, text_Afoo, to_write_Afoo);
		offset_Afoo += to_write_Afoo;
	}
	else {
		memcpy(filep_Afoo + offset_Afoo ,text_Afoo, 32);
		offset_Afoo += 32; 
	} 
     }

     if ( msync ( filep_Afoo, 65536 , MS_SYNC) < 0){
	munmap( filep_Afoo, 65536); 
	return -1;
     }


  offset_Afoo = 0;
   while (offset_Afoo < 4096){
        if (to_write_Afoo < 32){
                memcpy(filep_Afoo +258048+ offset_Afoo, text_Afoo, to_write_Afoo);
                offset_Afoo += to_write_Afoo;
        }
        else {
                memcpy(filep_Afoo+258048+ offset_Afoo,text_Afoo, 32);
                offset_Afoo += 32;
        }
     }

     if ( msync ( filep_Afoo+196608, 65536 , MS_SYNC) < 0){
        munmap( filep_Afoo+65536, 196608);
        return -1;
     }
     munmap( filep_Afoo, 262144);

    sync();
    close(fd_Afoo);



    return 0;
  }

  virtual int run(int checkpoint) override {

	int local_checkpoint = 0;
	init_paths();

    const int fd_Afoo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_Afoo < 0) {
      return -1;
    }
    if (WriteData(fd_Afoo, 0, 262144) < 0) {
        return -2;
    }

   sync();

   char *filep_Afoo = (char *) mmap(NULL, 262144, PROT_WRITE|PROT_READ, MAP_SHARED, fd_Afoo, 0);
    if (filep_Afoo == MAP_FAILED) {
        return -1;
    }

    int offset_Afoo = 0;
    int to_write_Afoo = 4096 ;
    const char *text_Afoo  = "12345678123456781234567812345678";

    while (offset_Afoo < 4096){
        if (to_write_Afoo < 32){
                memcpy(filep_Afoo + offset_Afoo, text_Afoo, to_write_Afoo);
                offset_Afoo += to_write_Afoo;
        }
        else {
                memcpy(filep_Afoo + offset_Afoo ,text_Afoo, 32);
                offset_Afoo += 32;
        }
     }


  offset_Afoo = 0;
   while (offset_Afoo < 4096){
        if (to_write_Afoo < 32){
                memcpy(filep_Afoo +258048+ offset_Afoo, text_Afoo, to_write_Afoo);
                offset_Afoo += to_write_Afoo;
        }
        else {
                memcpy(filep_Afoo+258048+ offset_Afoo,text_Afoo, 32);
                offset_Afoo += 32;
        }
     }

     if ( msync ( filep_Afoo, 65536 , MS_SYNC) < 0){
        munmap( filep_Afoo, 65536);
        return -1;
     }
    

     if ( msync ( filep_Afoo+196608, 65536 , MS_SYNC) < 0){
        munmap( filep_Afoo+65536, 196608);
        return -1;
     }
     munmap( filep_Afoo, 262144); 

     if (Checkpoint() < 0)
        return -1;

    local_checkpoint += 1;
    if (local_checkpoint == checkpoint){
	return 1;
    }

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
        		test_result->error_description = " : md5sum of bar does not match with expected md5sum of foo backup";
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
  return new fs_testing::tests::Generic325;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
