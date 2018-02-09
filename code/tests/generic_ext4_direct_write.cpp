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
    //fd_reg = open(kTestFile, O_RDWR , TEST_FILE_PERMS);
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

    /*if (WriteData(fd_reg, 0, 4096) < 0) {
      close(fd_reg);
      return -4;
    }*/

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
    if (res < 0) {
      test_result->SetError(DataTestResult::kFileMissing);
      return 0;
    }

    std::cout << "Stat of the file :" << std::endl;
    std::cout << "Size = " << stats.st_size << std::endl;
    std::cout << "Block count = " << stats.st_blocks << std::endl;
    std::cout << "FS block size = "<< stats.st_blksize << std::endl;
    std::cout << "Inode num = "<< stats.st_ino << std::endl;

    //Just to verify, stat the file using debugfs to see the extents allocated
    std::string block_command = "debugfs -R \'stat <12>\' /dev/cow_ram_snapshot1_0 ";
    system(block_command.c_str());

    if(stats.st_blocks > 0){
      std::cout << "Blocks > 0 and size = " << stats.st_size << std::endl;
      //FILE* f = fopen(kTestFile, "rw");
      int fd = open(kTestFile, O_RDWR);
      unsigned int bytes_read = 0;
      char* buf = (char*) calloc(TEXT_SIZE, sizeof(char));
      if (buf == NULL) {
        test_result->SetError(DataTestResult::kOther);
      }

      //read offset 0-4K
      //bytes_read = fread(buf, 1, TEXT_SIZE, f);
      bytes_read = read(fd, buf, TEXT_SIZE);

      //std::cout << "Buf size strlen = " << strlen(buf) << std::endl;
      //std::cout << "Bytes read = " << bytes_read << std::endl;
      if (bytes_read < TEXT_SIZE) {
        //data corrupted
        std::cout << "File data corrupted" << std::endl;
        test_result->SetError(DataTestResult::kFileDataCorrupted);
      }

      //Just sanity checks
      if(stats.st_blocks == 8){
        block_command = "debugfs -R \'cat <12>\' /dev/cow_ram_snapshot1_0 ";
        system(block_command.c_str());

        block_command = "debugfs -R \'testb 8481\' /dev/cow_ram_snapshot1_0 ";
        system(block_command.c_str());
      }

      close(fd);
      //fclose(f);
      free(buf);
    }

    std::cout << "\n" << std::endl;

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
