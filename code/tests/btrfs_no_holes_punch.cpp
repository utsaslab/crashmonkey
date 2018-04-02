/*

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
#include <stdlib.h>

#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_FILE_DUMMY "dummy"
#define TEST_DIR_A "test_dir_a"
#define TEST_TEXT_SIZE 435945472


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class BtrfsRenameFifo: public BaseTestCase {
 public:
  virtual int setup() override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    dummy_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_DUMMY;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    // Create test directory A.
    int res = mkdir(dira_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Create a file foo for direct write
    const int fd_foo = open(foo_path.c_str(),  O_RDWR| O_CREAT | O_DIRECT | O_SYNC, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

        //Create file foo_compare
    const int fd_foo_compare = open(dummy_path.c_str(),  O_RDWR | O_CREAT | O_DIRECT | O_SYNC, TEST_FILE_PERMS);
    if (fd_foo_compare < 0) {
      return -1;
    }

    int offset = 0;

    //Align memory boundaries for direct write
    void* data;
    if (posix_memalign(&data, 512, 262144) < 0) {
      return -2;
    }
    memset(data, 'a', 262144);
    for(int i=0; i<=831; i++){
        offset = i*2*256*1024;

    	if(pwrite(fd_foo, data, 262144, offset) < 0) {
		std::cout << strerror(errno) << std::endl;
      		close(fd_foo);
      		return -3;
    	}

    	/*if(pwrite(fd_foo_compare, data, 262144, offset) < 0) {
      		close(fd_foo_compare);
      		return -4;
    	}*/
    }

/*	std::cout << "Write completed" << std::endl;
    res = fallocate(fd_foo_compare, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, 216658016, 262144);
    if (res < 0)
      return -1;
*/
    //Sync everything
    sync();

	system("md5sum /mnt/snapshot/test_dir_a/foo");
	system("btrfs-debug-tree /dev/cow_ram0 >> out.txt");
    //system("stat /mnt/snapshot/test_dir_a/dummy");
    close(fd_foo);
    close(fd_foo_compare);
    return 0;
  }

  virtual int run() override {

    //initialize paths
    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    dummy_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_DUMMY;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;

    //Create the log tree by creating a random file in dir a, and fsyncing it
    const int fd_foo = open(foo_path.c_str(), O_RDWR, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    int off = 265814016 +128*1024 - 4000;	
    int res = fallocate(fd_foo, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE, off, 262144);
    if (res < 0)
      return -2;

    fsync(fd_foo);
    if (Checkpoint() < 0){
      return -4;
    }

system("md5sum /mnt/snapshot/test_dir_a/foo");

    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    foo_path = mnt_dir_ + "/"+ TEST_DIR_A + "/" + TEST_FILE_FOO;    
    dummy_path = mnt_dir_ +  "/" TEST_DIR_A + "/" + TEST_FILE_DUMMY;
    dira_path = mnt_dir_ + "/" + TEST_DIR_A;
 
	string expected_md5sum = "c5c0a13588a639529c979c57c336f441";
	system("md5sum /mnt/snapshot/test_dir_a/foo");
	string actual_md5sum = get_md5sum(foo_path);
	if (expected_md5sum.compare(actual_md5sum) != 0) {
        test_result->SetError(DataTestResult::kFileDataCorrupted);
        test_result->error_description = " : md5sum of foo not maching with ";
		return 0;
	}

    /*const int fd_foo_compare = open(dummy_path.c_str(), O_RDWR);
    if (fd_foo_compare < 0) {
      std::cout << "Open failed" <<std::endl;
      return -1;
    }

        // system("stat /mnt/snapshot/test_dir_a/dummy");
        //     system("stat /mnt/snapshot/test_dir_a/foo");
    //For later comparison, copy contents of foo into text.
    unsigned int bytes_read = 0;
    do {
      // int res = pread(fd_foo_compare, (void*) ((unsigned long) text + bytes_read),
      //     TEST_TEXT_SIZE - bytes_read, 216662016);
      int res = read(fd_foo_compare, (void*) ((unsigned long) text + bytes_read),
          TEST_TEXT_SIZE - bytes_read);
      if (res < 0) {
        close(fd_foo_compare);
        return -1;
      }
      bytes_read += res;
    } while (bytes_read < TEST_TEXT_SIZE);

    //std::cout << "Bytes read = " << bytes_read << std::endl;
    //If checkpoint < 1, we are not concerned.
    if(last_checkpoint >= 1){
      // system("cp /mnt/snapshot/test_dir_a/foo /home/jayashree/foo");

      FILE* f = fopen(foo_path.c_str(), "rw");

      int res = 0; 
      unsigned int bytes_read = 0;

         //   if (fseek(fp, offset, SEEK_SET) != 0)
         // return 0;
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

        //
        if (strlen(buf) == TEST_TEXT_SIZE) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);          
          test_result->error_description = " : fpunch not persisted even after fsync";
        }

        // If buf length is the same as orginal, verify the 
        // contents of the file to check if this is the state after fsync
        // or not.
        else if (memcmp(text, buf, TEST_TEXT_SIZE) != 0) {
          test_result->SetError(DataTestResult::kFileDataCorrupted);
          test_result->error_description = " : punch_hole not persisted even after fsync";
        }
      }

      fclose(f);
      free(buf);
  
    }
    close(fd_foo_compare);*/
    return 0;
  }

   private:
    char text[TEST_TEXT_SIZE];
    string foo_path;    
    string bar_path;
    string dummy_path;
    string dummy1_path;
    string dira_path;

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
  return new fs_testing::tests::BtrfsRenameFifo;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
