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
				B_path = mnt_dir_ + "/B";
				C_path = mnt_dir_ + "/C";
				foo_path = mnt_dir_ + "/A/foo";
				foo_link_path = mnt_dir_ + "/B/foo_link";
				bar_path = mnt_dir_ + "/B/bar";
				bar_moved_path = mnt_dir_ + "/C/bar_moved";

				if ( mkdir(A_path.c_str() , 0777) < 0){ 
					return errno;
				}


				if ( mkdir(B_path.c_str() , 0777) < 0){ 
					return errno;
				}


				if ( mkdir(C_path.c_str() , 0777) < 0){ 
					return errno;
				}


				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
					close( fd_foo); 
					return errno;
				}


				if ( symlink(foo_path.c_str() , foo_link_path.c_str() ) < 0){ 
					return errno;
				}


				int fd_bar = open(bar_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_bar < 0 ) { 
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
				B_path =  mnt_dir_ + "/B";
				C_path =  mnt_dir_ + "/C";
				foo_path =  mnt_dir_ + "/A/foo";
				foo_link_path =  mnt_dir_ + "/B/foo_link";
				bar_path =  mnt_dir_ + "/B/bar";
				bar_moved_path =  mnt_dir_ + "/C/bar_moved";

				if ( remove(foo_link_path.c_str() ) < 0){ 
					return errno;
				}


				if ( rename(bar_path.c_str() , bar_moved_path.c_str() ) < 0){ 
					return errno;
				}


				int fd_foo = open(foo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_foo < 0 ) { 
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

                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

				A_path =  mnt_dir_ + "/A";
				B_path =  mnt_dir_ + "/B";
				C_path =  mnt_dir_ + "/C";
				foo_path =  mnt_dir_ + "/A/foo";
				foo_link_path =  mnt_dir_ + "/B/foo_link";
				bar_path =  mnt_dir_ + "/B/bar";
				bar_moved_path =  mnt_dir_ + "/C/bar_moved";
                struct stat stats_old;
                struct stat stats_new;
                const int stat_old_res = stat(bar_path.c_str(), &stats_old);
                const int errno_old = errno;
                const int stat_new_res = stat(bar_moved_path.c_str(), &stats_new);
                const int errno_new = errno;

                // Neither stat found the file, it's gone...
                if (stat_old_res < 0 && errno_old == ENOENT &&
                    stat_new_res < 0 && errno_new == ENOENT) {
                  test_result->SetError(DataTestResult::kFileMissing);
                  test_result->error_description = " : " + bar_path + ", " + bar_moved_path + " missing";
                  return 0;
                }

                //Bar is present at both directories
                if (stat_old_res >= 0 && stat_new_res >= 0) {
                  test_result->SetError(DataTestResult::kOldFilePersisted);
                  test_result->error_description = " : " + bar_path + " still present";
                  return -1;
                }

                struct dirent *dir_entry;
                bool foo_present_in_A = false;
                bool bar_present_in_C = false;
                bool empty_B = true;

                DIR *dir = opendir(A_path.c_str());

                if (dir) {
                  //Get all files in this directory
                  while ((dir_entry = readdir(dir)) != NULL) {
                    //if (dir_entry->d_type == DT_REG){
                      if (strcmp(dir_entry->d_name, "foo") == 0){
                        foo_present_in_A = true;
                      }
                    //}
                  }
                }

                closedir(dir);

                dir = opendir(B_path.c_str());

                if (dir) {
                  //Get all files in this directory
                  while ((dir_entry = readdir(dir)) != NULL) {
                    if (dir_entry->d_type == DT_REG){
                      //std::cout << dir_entry->d_name <<std::endl;
                      if ((strcmp(dir_entry->d_name, "foo_link") == 0)||(strcmp(dir_entry->d_name, "bar") ==0)){
                        empty_B = false;
                      }
                    }
                  }
                }

                closedir(dir);

                dir = opendir(C_path.c_str());

                if (dir) {
                  //Get all files in this directory
                  while ((dir_entry = readdir(dir)) != NULL) {
                    if (dir_entry->d_type == DT_REG){
                      if (strcmp(dir_entry->d_name, "bar_moved") == 0){
                        bar_present_in_C = true;
                      }
                    }
                  }
                }

                closedir(dir);

                // If the last seen checkpoint is 1, i.e after fsync(file_foo)
                // then both the files foo must be present in A, bar in C, B empty
                if (last_checkpoint == 1 && (!foo_present_in_A || !bar_present_in_C || !empty_B)){
                  test_result->SetError(DataTestResult::kFileMissing);
                  if(!empty_B)
                    test_result->error_description = " : " + B_path + " not empty";      
                  else if(!foo_present_in_A)
                    test_result->error_description = " : " + foo_path + " missing";
                  else if(!bar_present_in_C)
                    test_result->error_description = " : " + bar_moved_path + " missing";
                  else ;
                  return 0;
                }

                return 0;


            }
            
            private:
            
			 string A_path; 
			 string B_path; 
			 string C_path; 
			 string foo_path; 
			 string foo_link_path; 
			 string bar_path; 
			 string bar_moved_path; 
        };
        
    }  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
    return new fs_testing::tests::testName;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
    delete tc;
}
