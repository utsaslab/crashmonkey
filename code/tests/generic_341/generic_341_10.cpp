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
                
				A_path = mnt_dir_ + "/A";
				X_path = mnt_dir_ + "/A/X";
				Y_path = mnt_dir_ + "/A/Y";
				foo_path = mnt_dir_ + "/A/X/foo";
				bar_path = mnt_dir_ + "/A/X/bar";

				if ( mkdir(A_path.c_str() , 0777) < 0){ 
					return errno;
				}


				if ( mkdir(X_path.c_str() , 0777) < 0){ 
					return errno;
				}


				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}


				if ( fallocate( fd_foo , 0 , 0 , 8192) < 0){ 
					 close( fd_foo);
					 return errno;
				}
				if ( WriteDataMmap( fd_foo, 0, 8192) < 0){ 
					close( fd_foo); 
					return errno;
				}


				int fd_bar = open(bar_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_bar < 0 ) { 
					close( fd_bar); 
					return errno;
				}


				close(fd_bar); 
				fd_bar = open(bar_path.c_str() , O_RDWR|O_DIRECT|O_SYNC , 0777); 
				if ( fd_bar < 0 ) { 
					close( fd_bar); 
					return errno;
				}

				void* data_bar;
				if (posix_memalign(&data_bar , 4096, 8192 ) < 0) {
					return errno;
				}

				 
				int offset_bar = 0;
				int to_write_bar = 8192 ;
				const char *text_bar  = "abcdefghijklmnopqrstuvwxyz123456";
				while (offset_bar < 8192){
					if (to_write_bar < 32){
						memcpy((char *)data_bar+ offset_bar, text_bar, to_write_bar);
						offset_bar += to_write_bar;
					}
					else {
						memcpy((char *)data_bar+ offset_bar,text_bar, 32);
						offset_bar += 32; 
					} 
				} 

				if ( pwrite( fd_bar, data_bar, 8192, 0) < 0){
					close( fd_bar); 
					return errno;
				}

				sync(); 


				if ( close( fd_foo) < 0){ 
					return errno;
				}


				if ( close( fd_bar) < 0){ 
					return errno;
				}


                return 0;
            }
            
            virtual int run() override {
                
				A_path =  mnt_dir_ + "/A";
				X_path =  mnt_dir_ + "/A/X";
				Y_path =  mnt_dir_ + "/A/Y";
				foo_path =  mnt_dir_ + "/A/X/foo";

				if ( rename(X_path.c_str() , Y_path.c_str() ) < 0){ 
					return errno;
				}


				if ( mkdir(X_path.c_str() , 0777) < 0){ 
					return errno;
				}


				int fd_X = open(X_path.c_str() , O_DIRECTORY , 0777); 
				if ( fd_X < 0 ) { 
					close( fd_X); 
					return errno;
				}


				if ( fsync( fd_X) < 0){ 
					return errno;
				}


				if ( Checkpoint() < 0){ 
					return -1;
				}


				if ( close( fd_X) < 0){ 
					return errno;
				}

				bar_path =  mnt_dir_ + "/A/X/bar";
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

				A_path =  mnt_dir_ + "/A";
				X_path =  mnt_dir_ + "/A/X";
				Y_path =  mnt_dir_ + "/A/Y";
				foo_path =  mnt_dir_ + "/A/X/foo";
				bar_path =  mnt_dir_ + "/A/X/bar";
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
            
			 string A_path; 
			 string X_path; 
			 string Y_path; 
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
