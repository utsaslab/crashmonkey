#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "utils.h"
#include "Tester.h"
#include "test_case.h"

#define SO_PATH "tests/"
// TODO(ashmrtn): Find a good delay time to use for tests.
#define TEST_DIRTY_EXPIRE_TIME "500"
#define WRITE_DELAY 10
#define MOUNT_DELAY 3

#define OPTS_STRING "d:lm:nt:"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using fs_testing::Tester;

static const option long_options[] = {
  {"no-lvm", no_argument, NULL, 'l'},
  {"dry-run", no_argument, NULL, 'n'},
  {"fs-type", required_argument, NULL, 't'},
  {"test-dev", required_argument, NULL, 'd'},
  {"mount-opts", required_argument, NULL, 'm'},
  {0, 0, 0, 0},
};

int main(int argc, char** argv) {
  string fs_type("ext4");
  string test_dev("/dev/ram0");
  string mount_opts("");
  bool dry_run = false;
  bool no_lvm = false;

  int option_idx = 0;

  // Parse command line arguments.
  for (int c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx);
        c != -1;
        c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx)) {
    switch (c) {
      case 'd':
        test_dev = string(optarg);
      case 'l':
        no_lvm = 1;
        break;
      case 'm':
        mount_opts = string(optarg);
        break;
      case 'n':
        dry_run = 1;
        break;
      case 't':
        fs_type = string(optarg);
        break;
      case '?':
      default:
        return -1;
    }
  }

  const unsigned int test_case_idx = optind;

  if (test_case_idx == argc) {
    cerr << "Please give a .so test case to load" << endl;
    return -1;
  }

  Tester test_harness(fs_type, test_dev);

  // Load the class being tested.
  cout << "Loading test case" << endl;
  if (test_harness.test_load_class(argv[test_case_idx]) != SUCCESS) {
    test_harness.cleanup_harness();
      return -1;
  }

  // Update dirty_expire_time.
  cout << "Updating dirty_expire_time_centisecs" << endl;
  const char* old_expire_time =
    test_harness.update_dirty_expire_time(TEST_DIRTY_EXPIRE_TIME);
  if (old_expire_time == NULL) {
    cerr << "Error updating dirty_expire_time_centisecs" << endl;
    test_harness.cleanup_harness();
    return -1;
  }

  // Initialize LVM
  if (!no_lvm) {
    cout << "Initializing LVM" << endl;
    if (test_harness.lvm_init() != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }
  }

  // Format test drive to desired type.
  cout << "Formatting test drive" << endl;
  if (test_harness.format_drive() != SUCCESS) {
    cerr << "Error formatting test drive" << endl;
    test_harness.cleanup_harness();
    return -1;
  }

  cout << "Clearing caches" << endl;
  if (test_harness.clear_caches() != SUCCESS) {
    cerr << "Error clearing caches" << endl;
    test_harness.cleanup_harness();
    return -1;
  }

  // Mount test file system for pre-test setup.
  cout << "Mounting test file system for pre-test setup\n";
  if (test_harness.mount_raw_test_device(mount_opts.c_str()) != SUCCESS) {
    test_harness.cleanup_harness();
    return -1;
  }

  // Run pre-test setup stuff.
  cout << "Running pre-test setup\n";
  {
    const pid_t child = fork();
    if (child < 0) {
      cerr << "Error creating child process to run pre-test setup\n";
      test_harness.cleanup_harness();
    } else if (child != 0) {
      // Parent process should wait for child to terminate before proceeding.
      pid_t status;
      wait(&status);
      if (status != 0) {
        cerr << "Error in pre-test setup\n";
        test_harness.cleanup_harness();
      }
    } else {
      return test_harness.test_setup();
    }
  }

  // Unmount the test file system after pre-test setup.
  cout << "Unmounting test file system after pre-test setup\n";
  if (test_harness.umount_device() != SUCCESS) {
    test_harness.cleanup_harness();
    return -1;
  }

  // Create snapshot of disk for testing.
  if (!no_lvm) {
    cout << "Making new snapshot\n";
    if (test_harness.init_snapshot() != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }
  }

  // Insert the disk block wrapper into the kernel.
  cout << "Inserting wrapper module into kernel\n";
  if (test_harness.insert_wrapper() != SUCCESS) {
    cerr << "Error inserting kernel wrapper module\n";
    test_harness.cleanup_harness();
  }

  // Mount the file system under the wrapper module for profiling.
  // TODO(ashmrtn): Fix to work with all file system types.
  cout << "Mounting wrapper file system\n";
  if (test_harness.mount_wrapper_device(mount_opts.c_str()) != SUCCESS) {
    cerr << "Error mounting wrapper file system\n";
    test_harness.cleanup_harness();
    return -1;
  }

  cout << "Sleeping after mount" << endl;
  unsigned int to_sleep = MOUNT_DELAY;
  do {
    to_sleep = sleep(to_sleep);
  } while (to_sleep > 0);

  // Get access to wrapper module ioctl functions via FD.
  cout << "Getting wrapper device ioctl fd\n";
  if (test_harness.get_wrapper_ioctl() != SUCCESS) {
    cerr << "Error opening device file\n";
    test_harness.cleanup_harness();
    return -1;
  }

  // Clear wrapper module logs prior to test profiling.
  cout << "Clearing wrapper device logs\n";
  test_harness.clear_wrapper_log();
  cout << "Enabling wrapper device loging\n";
  test_harness.begin_wrapper_logging();

  // Fork off a new process and run test profiling. Forking makes it easier to
  // handle making sure everything is taken care of in profiling and ensures
  // that even if all file handles aren't closed in the test process the parent
  // won't hang due to a busy mount point.
  cout << "Running test profile\n";
  {
    const pid_t child = fork();
    if (child < 0) {
      cerr << "Error spinning off test process\n";
      test_harness.cleanup_harness();
      return -1;
    } else if (child != 0) {
      pid_t status;
      wait(&status);
      if (status != 0) {
        cerr << "Error in test process\n";
        test_harness.cleanup_harness();
        return -1;
      }
      sleep(WRITE_DELAY);
    } else {
      // Forked process' stuff.
      return test_harness.test_run();
    }
  }

  cout << "Unmounting wrapper file system after test profiling\n";
  if (test_harness.umount_device() != SUCCESS) {
    cerr << "Error unmounting wrapper file system\n";
    test_harness.cleanup_harness();
    return -1;
  }

  cout << "Disabling wrapper device logging" << std::endl;
  test_harness.end_wrapper_logging();
  cout << "Getting wrapper data\n";
  if (test_harness.get_wrapper_log() != SUCCESS) {
    test_harness.cleanup_harness();
    return -1;
  }

  cout << "Close wrapper ioctl fd\n";
  test_harness.put_wrapper_ioctl();
  cout << "Removing wrapper module from kernel\n";
  if (test_harness.remove_wrapper() != SUCCESS) {
    cerr << "Error cleaning up: remove wrapper module\n";
    test_harness.cleanup_harness();
    return -1;
  }

  if (!dry_run) {
    cout << "Writing profiled data to block device and checking with fsck\n";
    test_harness.test_check_random_permutations(5);

    cout << "Ran " << test_harness.test_test_stats[TESTS_TESTS_RUN] << " tests"
          << endl
      << '\t' << test_harness.test_test_stats[TESTS_TEST_FSCK_FAIL]
          << " tests fsck failed" << endl
      << '\t' << test_harness.test_test_stats[TESTS_TEST_BAD_DATA]
          << " tests bad data" << endl
      << '\t' << test_harness.test_test_stats[TESTS_TEST_FSCK_FIX]
          << " tests fsck fix" << endl
      << '\t' << test_harness.test_test_stats[TESTS_TEST_PASS]
          << " tests passed" << endl
      << '\t' << test_harness.test_test_stats[TESTS_TEST_ERR]
          << " tests errored" << endl;
  }

  // Finish cleaning everything up.
  test_harness.cleanup_harness();
  return 0;
}
