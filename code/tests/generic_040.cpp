/*
Reproducing fstest generic/040

1. Create file foo
2. Create 3000 links for file foo in the same dir
    (link_0 to link_2999)
3. sync() everything so far
4. Add a new link link_3000 and fsync foo

If a power fail occurs now, and remount the filesystem, file foo
should contain the right number of links as expected, and
the dir A must be removable, after file foo and all links
are deleted.


This test is motivated by an fsync issue discovered in btrfs.
The issue in btrfs was that adding a new hard link to an inode that already
had a large number of hardlinks and fsync the inode, would make the fsync
log replay code update the inode with a wrong link count (smaller than the
correct value). This resulted later in dangling directory index entries,
after removing most of the hard links (correct_value - wrong_value), that
were visible to user space but it was impossible to delete them or do
any other operation on them (since they pointed to an inode that didn't
exist anymore, resulting in -ESTALE errors).

The btrfs issue was fixed by the following linux kernel patch:

	Btrfs: fix fsync when extend references are added to an inode

This issue was present in btrfs since the extrefs (extend references)
feature was added (2012).

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
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR_A "test_dir_a"


using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::WriteDataMmap;
using fs_testing::user_tools::api::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

namespace fs_testing {
namespace tests {


class Generic040: public BaseTestCase {
 public:
  virtual int setup() override {

	init_paths();

    // Create test directory A.
    int res = mkdir(dir_path.c_str(), 0777);
    if (res < 0) {
      return -1;
    }

    // Create file foo in TEST_DIR_A
    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    //create 3000 hard links
    for (int i = 0; i < 3000; i++ ){
      string foo_link = foo_link_path + std::to_string(i);
      //std::cout << foo_link << std::endl;
      if(link(foo_path.c_str(), foo_link.c_str()) < 0){
        return -2;
      }
    }

    sync();
    close(fd_foo);
    return 0;
  }

  virtual int run() override {
  
	init_paths();

    const int fd_foo = open(foo_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd_foo < 0) {
      return -1;
    }

    // create a new link for file foo
    string foo_link_new = foo_link_path + "3000";
    if (link(foo_path.c_str(), foo_link_new.c_str()) < 0){
      return -2;
    }

    //fsync foo
    int res = fsync(fd_foo);
    if (res < 0){
      return -4;
    }

    //Make a user checkpoint here. Checkpoint must be 1 beyond this point
    if (Checkpoint() < 0){
      return -5;
    }

    //Close open files  
    close(fd_foo);

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_result) override {

	init_paths();

	char mode[] = "0777";
    int i_mode = strtol(mode,0,8);
    chmod(foo_path.c_str(), i_mode);

    struct stat st;
    stat(foo_path.c_str(), &st);
    int num_links = st.st_nlink;

    if (last_checkpoint == 0) {
    	if (num_links != 3001 && num_links != 3002) {
		  test_result->SetError(DataTestResult::kFileMetadataCorrupted);
		  test_result->error_description = " : file foo doesn't contain 3001 or 3002 links";
    	}
    } else if (last_checkpoint == 1) {
    	if (num_links != 3002) {
		  test_result->SetError(DataTestResult::kFileMetadataCorrupted);
		  test_result->error_description = " : file foo doesn't contain 3002 links";
    	}
    }

    string remove_links_cmd = "rm " + foo_link_path + "*";
    system(remove_links_cmd.c_str());

    stat(foo_path.c_str(), &st);
    num_links = st.st_nlink;

    if (num_links != 1) {
	  test_result->SetError(DataTestResult::kFileMetadataCorrupted);
	  test_result->error_description = " : file foo should contain only 1 link";
    }

    // Remove all files within  test_dir_a
    string remove_all_cmd = "rm -f " + dir_path + "/*";
    system(remove_all_cmd.c_str());

    // Now try removing the directory itself.
    const int rem = rmdir(dir_path.c_str());
    const int err = errno;

    //If rmdir failed because the dir was not empty, it's a bug
    if(rem < 0 && err == ENOTEMPTY){
      test_result->SetError(DataTestResult::kFileMetadataCorrupted);
      test_result->error_description = " : Cannot remove dir even if empty";    
    }

    return 0;
  }

   private:
    string foo_path;
    string foo_link_path;
    string dir_path;

    void init_paths() {
    	dir_path = mnt_dir_ + "/" TEST_DIR_A;
    	foo_path = dir_path + "/" TEST_FILE_FOO;
    	foo_link_path = dir_path + "/" TEST_FILE_FOO_LINK;
    }
    
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::Generic040;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
