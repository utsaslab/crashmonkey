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


				int fd_foo = open(foo_path.c_str() , O_CREAT|O_RDWR , 0777); 
				if ( fd_foo < 0 ) { 
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
				bar_path =  mnt_dir_ + "/A/bar";

				int fd_bar = open(bar_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_bar < 0 ) { 
					close( fd_bar); 
					return errno;
				}


				int fd_A = open(A_path.c_str() , O_DIRECTORY , 0777); 
				if ( fd_A < 0 ) { 
					close( fd_A); 
					return errno;
				}


				if ( fdatasync( fd_A) < 0){ 
					return errno;
				}


				if ( fdatasync( fd_bar) < 0){ 
					return errno;
				}


				if ( Checkpoint() < 0){ 
					return -1;
				}


				if ( close( fd_bar) < 0){ 
					return errno;
				}


				if ( close( fd_A) < 0){ 
					return errno;
				}

                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
            
				A_path =  mnt_dir_ + "/A";
				foo_path =  mnt_dir_ + "/A/foo";
				bar_path =  mnt_dir_ + "/A/bar";

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
