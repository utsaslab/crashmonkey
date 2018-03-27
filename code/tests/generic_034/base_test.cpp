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
                
                return 0;
            }
            
            virtual int run() override {
                
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
            

                struct dirent *dir_entry;
                bool foo_present_in_A = false;
                bool bar_present_in_A = false;

                DIR *dir = opendir(A_path.c_str());

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

                //remove file foo, if present
                if(foo_present_in_A){
                  if(remove(foo_path.c_str())  < 0)
                    return -1;
                }

                //remove file bar, if present
                if(bar_present_in_A){
                  if(remove(bar_path.c_str())  < 0)
                    return -2;
                }

                //readdir again. It should be empty now
                dir = opendir(A_path.c_str());

                if((dir_entry = readdir(dir)) != NULL &&
                  (!strcmp(dir_entry->d_name, "foo") ||
                  !strcmp(dir_entry->d_name, "bar"))){
                  std::cout << "Dir not empty" << std::endl;
                  closedir(dir);
                  return -3;
                }

                else {
                  //we know directory is empty. Try remove(dir)
                  closedir(dir);
                  // If dir cannot be removed because of ENOTEMPTY, 
                  // its a bug, because we verified the directory
                  // is empty 
                  const int rem = rmdir(A_path.c_str());
                  const int err = errno;

                  if(rem < 0 && err == ENOTEMPTY){
                    test_result->SetError(DataTestResult::kFileMetadataCorrupted);
                    test_result->error_description = " : Cannot remove dir even if empty";    
                  }
                  return 0;
                }

                 return 0;
            }
            
            private:

            
        };
        
    }  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
    return new fs_testing::tests::testName;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
    delete tc;
}
