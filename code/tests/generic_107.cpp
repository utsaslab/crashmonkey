/*
Reproducing xfstest generic/107

1. Create file foo, create links foo_link_ and foo_link_2 under directory test_dir_a, and sync
2. Unlink foo_link_2, and fsync foo

After a crash, only foo_link_ should be present inside test_dir_a (if crashed after fsync foo),
and after the link foo_link_ is removed, the directory should be removeable.

https://github.com/kdave/xfstests/blob/master/tests/generic/107
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
#include "../user_tools/api/workload.h"
#include "../user_tools/api/actions.h"
#define TEST_FILE_FOO "foo"
#define TEST_FILE_FOO_LINK "foo_link_"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic107: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    // Create test directory A.
	string dir_path = mnt_dir_ + "/" TEST_DIR_A;
    int res = mkdir(dir_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    //Create file foo in TEST_DIR_A 
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }
    // Create two links to foo
    if(link(foo_path.c_str(), foo_link_path.c_str()) < 0){
      return -2;
    }
    if(link(foo_path.c_str(), foo_link_path2.c_str()) < 0){
      return -2;
    }

    sync();
    close(fd_foo);

    return 0;
  }

  virtual int run() override {

	init_paths();
  
    // Unlink foo_link2
    if(unlink(foo_link_path2.c_str()) < 0){
      return -3;
    }

	const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -5;
    }

    if (fsync(fd_foo) < 0) {
    	return -6;
    }

    // Make a user checkpoint here
    if (Checkpoint() < 0){
      return -7;
    }

    //Close open files  
    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

	string dir = mnt_dir_ + "/" TEST_DIR_A;
	string rm_cmd = "rm -f " + dir + "/*";

	if (last_checkpoint > 0) { // If foo_link2 was unlinked
		string command = "ls -1 " + dir;
		string output = get_system_output(command);
		if (output.compare(TEST_FILE_FOO_LINK) != 0) {
	        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
	        test_result->error_description = " : Only foo_link_ should be present in directory";
	        return 0;
		}
	}

	// Remove files within the directory
	system(rm_cmd.c_str());

	// Directory should be removeable
	if (rmdir(dir.c_str()) < 0) {
        test_result->SetError(DataTestResult::kFileMetadataCorrupted);
        test_result->error_description = " : Unable to remove directory after deleting files";
	}
    return 0;
  }

   private:
    string foo_path;
    string foo_link_path;
    string foo_link_path2;
    
    void init_paths() {
        foo_path = mnt_dir_ + "/" TEST_FILE_FOO;
        foo_link_path = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK;
        foo_link_path2 = mnt_dir_ + "/" TEST_DIR_A "/" TEST_FILE_FOO_LINK + "2";
    }

    string get_system_output(string command) {
        FILE *fp;
        fp = popen(command.c_str(), "r");
        char out[1000]; // Assuming output won't be longer than this
        fscanf(fp,"%s", out);
        fclose(fp);

        string out_str(out);
        return out_str;
    }
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic107;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
