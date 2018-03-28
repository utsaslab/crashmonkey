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
                
				A_path = mnt_dir_ + "/A";
				foo_path = mnt_dir_ + "/A/foo";
				bar_path = mnt_dir_ + "/A/bar";

				if ( mkdir(A_path.c_str() , 0777) < 0){ 
					return errno;
				}


				sync(); 

                return 0;
            }
            
            virtual int run() override {
                
				A_path =  mnt_dir_ + "/A";
				foo_path =  mnt_dir_ + "/A/foo";
				bar_path =  mnt_dir_ + "/A/bar";

				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}


				if ( link(foo_path.c_str() , bar_path.c_str() ) < 0){ 
					return errno;
				}


				sync(); 


				if ( unlink(bar_path.c_str() ) < 0){ 
					return errno;
				}


				if ( fsync( fd_foo) < 0){ 
					return errno;
				}


				if ( Checkpoint() < 0){ 
					return -1;
				}


				if ( close( fd_foo) < 0){ 
					return errno;
				}

                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
                
				A_path =  mnt_dir_ + "/A";
				foo_path =  mnt_dir_ + "/A/foo";
				bar_path =  mnt_dir_ + "/A/bar";
                
                char mode[] = "0777";
                int i_mode = strtol(mode,0,8);
                chmod(foo_path.c_str(), i_mode);
                chmod(bar_path.c_str(), i_mode);

                //Remove all files within  test_dir_a
                system("rm -f /mnt/snapshot/A/*");

                //Now try removing the directory itself.
                const int rem = rmdir(A_path.c_str());
                const int err = errno;

                //Do we need a FS check to detect inconsistency?
                bool need_check = false;

                //If rmdir failed because the dir was not empty, it's a bug
                if(rem < 0 && err == ENOTEMPTY){
                  test_result->SetError(DataTestResult::kFileMetadataCorrupted);
                  test_result->error_description = " : Cannot remove dir even if empty";    
                  need_check = true;
                }

                //We do a FS check :
                // We know this is a btrfs specific bug, so we use btrfs check
                if(need_check){
                    system("umount /mnt/snapshot");
                    string check_string;
                    char tmp[128];
                    string command("btrfs check /dev/cow_ram_snapshot1_0 2>&1");
                    FILE *pipe = popen(command.c_str(), "r");
                    while (!feof(pipe)) {
                    char *r = fgets(tmp, 128, pipe);
                    // NULL can be returned on error.
                      if (r != NULL) {
                        check_string += tmp;
                       }
                    }

                    int ret = pclose(pipe);
                    if(ret != 0){
                      test_result->error_description += " \nbtrfs check result : \n " + check_string;
                    }
                  }
                return 0;
            }
            
            private:

			 string A_path; 
			 string foo_path; 
			 string bar_path; 
            
        };
        
    }  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
    return new fs_testing::tests::testName;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
    delete tc;
}
