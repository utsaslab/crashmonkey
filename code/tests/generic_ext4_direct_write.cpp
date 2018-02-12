/*
1. Create test_file
2. Do a buffered write 16-20K into the file
3. Now do a direct write 0-4K to the same file

If  we crash now and recover,the direct write should go through successfully and increment both 
i_size and i_blocks

This is tested to fail on ext4 (kernel 4.15 - latest now) 
https://marc.info/?l=linux-ext4&m=151669669030547&w=2

In ext4, the block count for direct write is incremented, but the i_size remains 0 
*/


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <string.h>
#include "BaseTestCase.h"
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))
#define BLOCK_SIZE (512)
#define TEXT_SIZE (4096)

namespace fs_testing {
namespace tests {

namespace {
  static constexpr char kTestFile[] = "/mnt/snapshot/test_file";
}

class genericDirectWrite: public BaseTestCase {
 public:
  virtual int setup() override {

    //Just create an empty file. 
    const int fd = open(kTestFile, O_WRONLY | O_TRUNC | O_CREAT,
        TEST_FILE_PERMS);
    if (fd < 0) {
      return -1;
    }

    syncfs(fd);
    close(fd);

    return 0;
  }

  virtual int run() override {

    //Open the file
    int fd_reg = open(kTestFile, O_RDWR);
    if (fd_reg < 0) {
      return -1;
    }

    //Write 16K-20K. This is a buffered write and hence delayed allocated
    // The i_size would be modified to 20K after this write
    if (WriteData(fd_reg, 16384, 4096) < 0) {
      close(fd_reg);
      return -2;
    }

    close(fd_reg);

    //Open the same file for direct write
    fd_reg = open(kTestFile, O_RDWR | O_DIRECT | O_SYNC, TEST_FILE_PERMS);
    if (fd_reg < 0) {
      return -3;
    }

    //Now write 0-4K into the same file. This would not update the i_size
    // because it sees the file size to be 20K due to the buffered write

    //Align memory boundaries for direct write
    void* data;
    if (posix_memalign(&data, BLOCK_SIZE, TEXT_SIZE) < 0) {
      return -4;
    }
    memset(data, 'a', TEXT_SIZE);

    if(pwrite(fd_reg, data, TEXT_SIZE, 0) < 0) {
      close(fd_reg);
      return -5;
    }

    //Let's ensure journal entries for direct write went through
    sleep(5);

    // We don't really use this checkpoint for any checks in this test case
    // We can't sync data before this checkpoint, as it would resolve the delayed 
    // write as well and the bug would disappear.
    if (Checkpoint() < 0){
      return -5;
    }
    
    close(fd_reg);
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    struct stat stats;
    int res = stat(TEST_MNT "/" TEST_FILE, &stats);

    //Should never happen. We persist the file in setup
    if (res < 0) {
      test_result->SetError(DataTestResult::kFileMissing);
      return 0;
    }

    //We don't want to print these stuff. Uncomment for debugging only
    /*std::cout << "Stat of the file :" << std::endl;
    std::cout << "Size = " << stats.st_size << std::endl;
    std::cout << "Block count = " << stats.st_blocks << std::endl;
    std::cout << "FS block size = "<< stats.st_blksize << std::endl;
    std::cout << "Inode num = "<< stats.st_ino << std::endl;

    //Just to verify, stat the file using debugfs to see the extents allocated
    std::string block_command = "debugfs -R \'stat <12>\' /dev/cow_ram_snapshot1_0 ";
    system(block_command.c_str());
	*/

    //If blocks allocated is > 0, then i_size must be > 0
    if(stats.st_blocks > 0){

      if(stats.st_size == 0) {
      	test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      	test_result->error_description =
        " : File size = 0 but block count = " +
        std::to_string(stats.st_blocks);
      	return 0;
      }	
    }

    return 0;
  }

  private :
  char text[TEXT_SIZE];
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::genericDirectWrite;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
