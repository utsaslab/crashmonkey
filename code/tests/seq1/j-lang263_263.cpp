#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <errno.h>
#include <attr/xattr.h>

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
                
				test_path = mnt_dir_ ;
				A_path = mnt_dir_ + "/A";
				B_path = mnt_dir_ + "/B";
				Afoo_path = mnt_dir_ + "/A/foo";
				Abar_path = mnt_dir_ + "/A/bar";
				foo_path = mnt_dir_ + "/foo";
				bar_path = mnt_dir_ + "/bar";
                return 0;
            }
            
            virtual int run( int checkpoint ) override {
                
				test_path = mnt_dir_ ;
				A_path =  mnt_dir_ + "/A";
				B_path =  mnt_dir_ + "/B";
				Afoo_path =  mnt_dir_ + "/A/foo";
				Abar_path =  mnt_dir_ + "/A/bar";
				foo_path =  mnt_dir_ + "/foo";
				bar_path =  mnt_dir_ + "/bar";
				int local_checkpoint = 0 ;

				int fd_test = open(test_path.c_str() , O_DIRECTORY , 0777); 
				if ( fd_test < 0 ) { 
					close( fd_test); 
					return errno;
				}


				if ( rename(test_path.c_str() , B_path.c_str() ) < 0){ 
					return errno;
				}


				 fd_test = open(test_path.c_str() , O_DIRECTORY , 0777); 
				if ( fd_test < 0 ) { 
					close( fd_test); 
					return errno;
				}


				if ( fsync( fd_test) < 0){ 
					return errno;
				}


				if ( Checkpoint() < 0){ 
					return -1;
				}
				local_checkpoint += 1; 

				if ( close( fd_test) < 0){ 
					return errno;
				}

				if (local_checkpoint == checkpoint) { 
					return 1;
				}

                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
                
				test_path = mnt_dir_ ;
				A_path =  mnt_dir_ + "/A";
				B_path =  mnt_dir_ + "/B";
				Afoo_path =  mnt_dir_ + "/A/foo";
				Abar_path =  mnt_dir_ + "/A/bar";
				foo_path =  mnt_dir_ + "/foo";
				bar_path =  mnt_dir_ + "/bar";
                return 0;
            }
                       
            private:
                       
			 string test_path; 
			 string A_path; 
			 string B_path; 
			 string Afoo_path; 
			 string Abar_path; 
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
