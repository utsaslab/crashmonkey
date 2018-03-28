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
#define TEST_TEXT_SIZE 16384

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
    namespace tests {
        
        
        class testName: public BaseTestCase {
            
            public:
            
            virtual int setup() override {


                close(fd_foo_compare);
                sync();

                
                return 0;
            }
            
            virtual int run() override {
                
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

                 //Get ground truth to compare against
                const int fd_foo_compare = open(foo_compare_path.c_str(), O_RDWR);
                if (fd_foo_compare < 0) {
                  return -1;
                }
                //For later comparison, copy contents of foo into text.
                unsigned int bytes_read = 0;
                do {
                  int res = read(fd_foo_compare, (void*) ((unsigned long) text + bytes_read),
                      TEST_TEXT_SIZE - bytes_read);
                  if (res < 0) {
                    close(fd_foo_compare);
                    return -1;
                  }
                  bytes_read += res;
                } while (bytes_read < TEST_TEXT_SIZE);

                //If checkpoint < 1, we are not concerned.
                if(last_checkpoint >= 1){

                  FILE* f = fopen(foo_path.c_str(), "rw");

                  int res = 0; 
                  unsigned int bytes_read = 0;
                  char* buf = (char*) calloc(TEST_TEXT_SIZE, sizeof(char));
                  if (buf == NULL) {
                    test_result->SetError(DataTestResult::kOther);
                  }


                  bytes_read = fread(buf, 1, TEST_TEXT_SIZE, f);

                  if (bytes_read != TEST_TEXT_SIZE) {
                    //error reading the file
                    std::cout << "Error reading file" << std::endl;
                    test_result->SetError(DataTestResult::kOther);
                  }

                  else {

                    if (memcmp(text, buf, TEST_TEXT_SIZE) != 0) {
                      test_result->SetError(DataTestResult::kFileDataCorrupted);
                      test_result->error_description = " : falloc not persisted even after fsync";
                    }
                  }

                  fclose(f);
                  free(buf);    
              
                }
                close(fd_foo_compare);
                return 0;
            }
            
            private:
                char text[TEST_TEXT_SIZE];

            
        };
        
    }  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
    return new fs_testing::tests::testName;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
    delete tc;
}
