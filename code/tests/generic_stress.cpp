/*
A simple fsstress workload
*/


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
#include "../user_tools/api/actions.h"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class StressTest: public BaseTestCase {
 public:
  virtual int setup() override {
    
    sync();	

    return 0;
  }

  virtual int run() override {

	//Create a stress workload with 25 ops
        system("$FSSTRESS -z -n 25 \
		       -f creat=5 \
		       -f mkdir=5 \
		       -f fsync=5 \
		       -d /mnt/snapshot -v");
    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

    //need to integrate auto checker

    return 0;
  }

   private:
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::StressTest;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
