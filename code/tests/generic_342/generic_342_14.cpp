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


				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}


				close(fd_foo); 
				fd_foo = open(foo_path.c_str() , O_RDWR|O_DIRECT|O_SYNC , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}

				void* data_foo;
				if (posix_memalign(&data_foo , 4096, 16384 ) < 0) {
					return errno;
				}

				 
				int offset_foo = 0;
				int to_write_foo = 16384 ;
				const char *text_foo  = "abcdefghijklmnopqrstuvwxyz123456";
				while (offset_foo < 16384){
					if (to_write_foo < 32){
						memcpy((char *)data_foo+ offset_foo, text_foo, to_write_foo);
						offset_foo += to_write_foo;
					}
					else {
						memcpy((char *)data_foo+ offset_foo,text_foo, 32);
						offset_foo += 32; 
					} 
				} 

				if ( pwrite( fd_foo, data_foo, 16384, 0) < 0){
					close( fd_foo); 
					return errno;
				}

				sync(); 


				if ( close( fd_foo) < 0){ 
					return errno;
				}


                return 0;
            }
            
            virtual int run() override {
                
				A_path =  mnt_dir_ + "/A";
				foo_path =  mnt_dir_ + "/A/foo";

				if ( rename(foo_path.c_str() , bar_path.c_str() ) < 0){ 
					return errno;
				}


				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}


				if ( fallocate( fd_foo , 0 , 0 , 4096) < 0){ 
					 close( fd_foo);
					 return errno;
				}
				if ( WriteDataMmap( fd_foo, 0, 4096) < 0){ 
					close( fd_foo); 
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

				bar_path =  mnt_dir_ + "/A/bar";
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

				A_path =  mnt_dir_ + "/A";
				foo_path =  mnt_dir_ + "/A/foo";
				bar_path =  mnt_dir_ + "/A/bar";

                struct stat stats_old;
                struct stat stats_new;
                const int stat_foo = stat(foo_path.c_str(), &stats_old);
                const int errno_foo = errno;
                const int stat_bar = stat(bar_path.c_str(), &stats_new);
                const int errno_bar = errno;

                // Neither stat found the file, it's gone...
                if (stat_foo < 0 && errno_foo == ENOENT &&
                    stat_bar < 0 && errno_bar == ENOENT) {
                  test_result->SetError(DataTestResult::kFileMissing); 
                  test_result->error_description = " : " + foo_path + " and " + bar_path+  " missing";
                  }

                // We renamed foo-> bar and created a new file foo. So old file foo's contents should be 
                // present in file bar. Else we have lost data present in old file foo during
                // rename.

                //We have lost file bar(contents of old foo)
                if(last_checkpoint ==1 && stat_bar < 0 && errno_bar == ENOENT && stats_old.st_size != 16384){
                  test_result->SetError(DataTestResult::kFileMissing); 
                  test_result->error_description = " : " + foo_path + " has new data " + bar_path + " missing";
                }

                //if bar is present, verify bar is the old foo 
                if(stat_bar == 0 && (stats_old.st_size != 4096 || stats_new.st_size != 16384)){
                  test_result->SetError(DataTestResult::kFileDataCorrupted); 
                  test_result->error_description = " : " + foo_path + " and " +bar_path + " has incorrect data";
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
