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

namespace fs_testing {
    namespace tests {


        class Example: public BaseTestCase {

            public:

              virtual int setup() override {

                // initialize paths.
                // mnt_dir_ points to the root directory by default
                  A_path = mnt_dir_ + "/A";
                  Afoo_path =  mnt_dir_ + "/A/foo";

                  // create the directory
                  if ( mkdir(A_path.c_str() , 0777) < 0 ){
                      return errno;
                  }

                  // create the file
                  int fd_Afoo = open(Afoo_path.c_str() , O_RDWR|O_CREAT , 0777);
                  if ( fd_Afoo < 0 ) {
                    close( fd_Afoo);
                    return errno;
                  }

                  sync();

                  //close the file decriptor.
                  if ( close ( fd_Afoo ) < 0 ) {
                      return errno;
                  }

                  return 0;
              }

              virtual int run( int checkpoint ) override {

                B_path = mnt_dir_ + "/B";
                Bfoo_path =  mnt_dir_ + "/B/foo";
                Afoo_path =  mnt_dir_ + "/A/foo";
                int local_checkpoint = 0 ;

                // create the directory
                if ( mkdir( B_path.c_str() , 0777 ) < 0 ) {
                    return errno;
                }

                //link files
                if ( link( Afoo_path.c_str() , Bfoo_path.c_str() ) < 0 ) {
                  return errno;
                }

                int fd_Afoo = cm_->CmOpen(Afoo_path.c_str() , O_RDWR|O_CREAT , 0777);
                if ( fd_Afoo < 0 ) {
                  cm_->CmClose( fd_Afoo);
                  return errno;
                }

                if ( cm_->CmFsync( fd_Afoo ) < 0 ) {
                  return errno;
                }

                if ( cm_->CmCheckpoint() < 0){
                    return -1;
                }
                local_checkpoint += 1;
                if (local_checkpoint == checkpoint) {
                    return 1;
                }

                if ( cm_->CmClose ( fd_Afoo) < 0){
                    return errno;
                }

                return 0;
            }

            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {

              Bfoo_path =  mnt_dir_ + "/B/foo";
              Afoo_path =  mnt_dir_ + "/A/foo";

              struct stat stats;

              /*
              Because we persisted A/foo in setup, and didn't rename or unlink
              it, we must find A/foo after crash.
              */
              int res_Afoo = stat(Afoo_path.c_str(), &stats);
              if (res_Afoo < 0) {
                  test_result->SetError(DataTestResult::kFileMissing);
                  test_result->error_description = " : Missing file " + Afoo_path;
                  return 0;
              }

              /*
              We need to check for the existance of B/foo only after A/foo has
              been persisted, which is tracked by checkpoint.
              */

              int res_Bfoo = stat(Bfoo_path.c_str(), &stats);
              if ( last_checkpoint == 1 && res_Bfoo < 0 ) {
                  test_result->SetError(DataTestResult::kFileMissing);
                  test_result->error_description = " : Missing file " + Bfoo_path;
                  return 0;
              }

              return 0;
            }

          private:

            string A_path;
            string B_path;
            string Afoo_path;
            string Bfoo_path;

      };

  }  // namespace tests
  }  // namespace fs_testing

 extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
     return new fs_testing::tests::Example;
 }

 extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
     delete tc;
 }
