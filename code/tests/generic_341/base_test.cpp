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
                
                return 0;
            }
            
            virtual int run() override {
                
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

                string bar_moved_path =  mnt_dir_ + "/A/Y/bar";
                string foo_moved_path =  mnt_dir_ + "/A/Y/foo";
                
                struct stat stats_old;
                struct stat stats_new;
                const int stat_old_res_foo = stat(foo_path.c_str(), &stats_old);
                const int errno_old_foo = errno;
                const int stat_new_res_foo = stat(foo_moved_path.c_str(), &stats_new);
                const int errno_new_foo = errno;

                const int stat_old_res_bar = stat(bar_path.c_str(), &stats_old);
                const int errno_old_bar = errno;
                const int stat_new_res_bar = stat(bar_moved_path.c_str(), &stats_new);
                const int errno_new_bar = errno;

                bool foo_missing = false, bar_missing = false;
                bool foo_present_both = false, bar_present_both = false;

                // Neither stat found the file, it's gone...
                if (stat_old_res_foo < 0 && errno_old_foo == ENOENT &&
                    stat_new_res_foo < 0 && errno_new_foo == ENOENT) {
                  foo_missing = true;
                }
                if (stat_old_res_bar < 0 && errno_old_bar == ENOENT &&
                    stat_new_res_bar < 0 && errno_new_bar == ENOENT) {
                  bar_missing = true;
                }

                if(foo_missing || bar_missing){
                  test_result->SetError(DataTestResult::kFileMissing); 
                  if(foo_missing && bar_missing)
                    test_result->error_description = " : " + foo_path + ", " + bar_path + " missing in both dir X and Y";
                  else if(foo_missing){
                    test_result->error_description = " : " + foo_path + " missing in dir X and Y";
                  }
                  else
                    test_result->error_description = " : " + bar_path + " missing in dir X and Y";
                return 0;
                }

                //Files present at both directories
                if (stat_old_res_foo >= 0 && stat_new_res_foo >= 0) 
                  foo_present_both = true;
                if (stat_old_res_bar >= 0 && stat_new_res_bar >= 0) 
                  bar_present_both = true;
                
                if(foo_present_both || bar_present_both){
                  test_result->SetError(DataTestResult::kOldFilePersisted); 
                  if(foo_present_both && bar_present_both)
                    test_result->error_description = " : " + foo_path + ", " + bar_path + " present in both dir X and Y";
                  else if(foo_present_both)
                    test_result->error_description = " : " + foo_path + " present in dir X and Y";
                  else
                    test_result->error_description = " : " + bar_path + " present in dir X and Y";
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
