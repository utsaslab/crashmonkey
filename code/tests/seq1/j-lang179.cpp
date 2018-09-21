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

#include "../BaseTestCase.h"
#include "../../user_tools/api/workload.h"
#include "../../user_tools/api/actions.h"

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
				AC_path = mnt_dir_ + "/A/C";
				B_path = mnt_dir_ + "/B";
				Afoo_path = mnt_dir_ + "/A/foo";
				Abar_path = mnt_dir_ + "/A/bar";
				Bfoo_path = mnt_dir_ + "/B/foo";
				Bbar_path = mnt_dir_ + "/B/bar";
				ACfoo_path = mnt_dir_ + "/A/C/foo";
				ACbar_path = mnt_dir_ + "/A/C/bar";
                return 0;
            }
            
            virtual int run( int checkpoint ) override {
                
				test_path = mnt_dir_ ;
				A_path =  mnt_dir_ + "/A";
				AC_path =  mnt_dir_ + "/A/C";
				B_path =  mnt_dir_ + "/B";
				Afoo_path =  mnt_dir_ + "/A/foo";
				Abar_path =  mnt_dir_ + "/A/bar";
				Bfoo_path =  mnt_dir_ + "/B/foo";
				Bbar_path =  mnt_dir_ + "/B/bar";
				ACfoo_path =  mnt_dir_ + "/A/C/foo";
				ACbar_path =  mnt_dir_ + "/A/C/bar";
				int local_checkpoint = 0 ;

				if ( mkdir(B_path.c_str() , 0777) < 0){ 
					return errno;
				}


				int fd_Bfoo = cm_->CmOpen(Bfoo_path.c_str() , O_RDWR|O_CREAT , 0777); 
				if ( fd_Bfoo < 0 ) { 
					cm_->CmClose( fd_Bfoo); 
					return errno;
				}


				if ( WriteData ( fd_Bfoo, 0, 32768) < 0){ 
					cm_->CmClose( fd_Bfoo); 
					return errno;
				}


				if ( fallocate( fd_Bfoo , 0 , 0 , 8192) < 0){ 
					cm_->CmClose( fd_Bfoo);
					 return errno;
				}
				char *filep_Bfoo = (char *) cm_->CmMmap(NULL, 8192 + 0, PROT_WRITE|PROT_READ, MAP_SHARED, fd_Bfoo, 0);
				if (filep_Bfoo == MAP_FAILED) {
					 return -1;
				}

				int moffset_Bfoo = 0;
				int to_write_Bfoo = 8192 ;
				const char *mtext_Bfoo  = "mmmmmmmmmmklmnopqrstuvwxyz123456";

				while (moffset_Bfoo < 8192){
					if (to_write_Bfoo < 32){
						memcpy(filep_Bfoo + 0 + moffset_Bfoo, mtext_Bfoo, to_write_Bfoo);
						moffset_Bfoo += to_write_Bfoo;
					}
					else {
						memcpy(filep_Bfoo + 0 + moffset_Bfoo,mtext_Bfoo, 32);
						moffset_Bfoo += 32; 
					} 
				}

				if ( cm_->CmMsync ( filep_Bfoo + 0, 8192 , MS_SYNC) < 0){
					cm_->CmMunmap( filep_Bfoo,0 + 8192); 
					return -1;
				}
				cm_->CmMunmap( filep_Bfoo , 0 + 8192);


				if ( cm_->CmCheckpoint() < 0){ 
					return -1;
				}
				local_checkpoint += 1; 
				if (local_checkpoint == checkpoint) { 
					return 1;
				}


				if ( cm_->CmClose ( fd_Bfoo) < 0){ 
					return errno;
				}

                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
                
				test_path = mnt_dir_ ;
				A_path =  mnt_dir_ + "/A";
				AC_path =  mnt_dir_ + "/A/C";
				B_path =  mnt_dir_ + "/B";
				Afoo_path =  mnt_dir_ + "/A/foo";
				Abar_path =  mnt_dir_ + "/A/bar";
				Bfoo_path =  mnt_dir_ + "/B/foo";
				Bbar_path =  mnt_dir_ + "/B/bar";
				ACfoo_path =  mnt_dir_ + "/A/C/foo";
				ACbar_path =  mnt_dir_ + "/A/C/bar";
                return 0;
            }
                       
            private:
                       
			 string test_path; 
			 string A_path; 
			 string AC_path; 
			 string B_path; 
			 string Afoo_path; 
			 string Abar_path; 
			 string Bfoo_path; 
			 string Bbar_path; 
			 string ACfoo_path; 
			 string ACbar_path; 
                       
            };
                       
    }  // namespace tests
    }  // namespace fs_testing
                       
   extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
       return new fs_testing::tests::testName;
   }
                       
   extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
       delete tc;
   }
