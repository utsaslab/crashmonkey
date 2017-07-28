#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include <iostream>
#include <string>
#include <vector>

#include "../utils/utils.h"
#include "Tester.h"
#include "../tests/BaseTestCase.h"
#include "ipc.h"

#define TEST_SO_PATH "tests/"
#define PERMUTER_SO_PATH "permuter/"
// TODO(ashmrtn): Find a good delay time to use for tests.
#define TEST_DIRTY_EXPIRE_TIME "500"
#define WRITE_DELAY 20
#define MOUNT_DELAY 3

#define OPTS_STRING "d:f:e:l:m:np:r:s:t:v"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using fs_testing::Tester;

static const option long_options[] = {
  {"test-dev", required_argument, NULL, 'd'},
  {"disk_size", required_argument, NULL, 'e'},
  {"flag-device", required_argument, NULL, 'f'},
  {"log-file", required_argument, NULL, 'l'},
  {"mount-opts", required_argument, NULL, 'm'},
  {"dry-run", no_argument, NULL, 'n'},
  {"permuter", required_argument, NULL, 'p'},
  {"reload-log-file", required_argument, NULL, 'r'},
  {"iterations", required_argument, NULL, 's'},
  {"fs-type", required_argument, NULL, 't'},
  {"verbose", no_argument, NULL, 'v'},
  {0, 0, 0, 0},
};

int main(int argc, char** argv) {
  string dirty_expire_time_centisecs(TEST_DIRTY_EXPIRE_TIME);
  unsigned long int test_sleep_delay = WRITE_DELAY;
  string fs_type("ext4");
  string flags_dev("/dev/vda");
  string test_dev("/dev/ram0");
  string mount_opts("");
  string log_file_save("");
  string log_file_load("");
  string permuter(PERMUTER_SO_PATH "RandomPermuter.so");
  bool dry_run = false;
  bool no_lvm = false;
  bool verbose = false;
  int iterations = 1000;
  int disk_size = 10240;
  int option_idx = 0;

  // Parse command line arguments.
  for (int c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx);
        c != -1;
        c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx)) {
    switch (c) {
      case 'f':
        flags_dev = string(optarg);
        break;
      case 'd':
        test_dev = string(optarg);
        break;
      case 'e':
        disk_size = atoi(optarg);
        break;
      case 'l':
        log_file_save = string(optarg);
        break;
      case 'm':
        mount_opts = string(optarg);
        break;
      case 'n':
        dry_run = 1;
        break;
      case 'p':
        permuter = string(optarg);
        break;
      case 'r':
        log_file_load = string(optarg);
        break;
      case 's':
        iterations = atoi(optarg);
        break;
      case 't':
        fs_type = string(optarg);
        break;
      case 'v':
        verbose = true;
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

  if (iterations < 0) {
    cerr << "Please give a positive number of iterations to run" << endl;
    return -1;
  }

  if (disk_size <= 0) {
    cerr << "Please give a positive number for the RAM disk size to use"
      << endl;
    return -1;
  }

  Tester test_harness(disk_size, verbose);

  cout << "Inserting RAM disk module" << endl;
  if (test_harness.insert_cow_brd() != SUCCESS) {
    cerr << "Error inserting RAM disk module" << endl;
    return -1;
  }
  test_harness.set_fs_type(fs_type);
  test_harness.set_device(test_dev);

  // Load the class being tested.
  cout << "Loading test case" << endl;
  if (test_harness.test_load_class(argv[test_case_idx]) != SUCCESS) {
    test_harness.cleanup_harness();
      return -1;
  }

  // Load the permuter to use for the test.
  // TODO(ashmrtn): Consider making a line in the test file which specifies the
  // permuter to use?
  cout << "Loading permuter" << endl;
  if (test_harness.permuter_load_class(permuter.c_str()) != SUCCESS) {
    test_harness.cleanup_harness();
      return -1;
  }

  // Update dirty_expire_time.
  cout << "Updating dirty_expire_time_centisecs" << endl;
  const char* old_expire_time =
    test_harness.update_dirty_expire_time(dirty_expire_time_centisecs.c_str());
  if (old_expire_time == NULL) {
    cerr << "Error updating dirty_expire_time_centisecs" << endl;
    test_harness.cleanup_harness();
    return -1;
  }

  // Run the normal test setup stuff if we don't have a log file.
  if (log_file_load.empty()) {
    if (flags_dev.empty()) {
      cerr << "No device to copy flags from given" << endl;
      return -1;
    }
    test_harness.set_flag_device(flags_dev);

    // Format test drive to desired type.
    cout << "Formatting test drive" << endl;
    if (test_harness.format_drive() != SUCCESS) {
      cerr << "Error formatting test drive" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // Mount test file system for pre-test setup.
    cout << "Mounting test file system for pre-test setup\n";
    if (test_harness.mount_device_raw(mount_opts.c_str()) != SUCCESS) {
      cerr << "Error mounting test device" << endl;
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
    cout << "Making new snapshot\n";
    if (test_harness.clone_device() != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }

    // If we're logging this test run then also save the snapshot.
    if (!log_file_save.empty()) {
      cout << "Saving snapshot to log file" << endl;
      if (test_harness.log_snapshot_save(log_file_save + "_snap")
          != SUCCESS) {
        test_harness.cleanup_harness();
        return -1;
      }
    }
  } else {
    // Load the snapshot in the log file and then write it to disk.
    cout << "Loading saved snapshot" << endl;
    if (test_harness.log_snapshot_load(log_file_load + "_snap") != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }
  }

  // TODO(ashmrtn): Consider making a flag for this?
  cout << "Clearing caches" << endl;
  if (test_harness.clear_caches() != SUCCESS) {
    cerr << "Error clearing caches" << endl;
    test_harness.cleanup_harness();
    return -1;
  }


  // No log file given so run the test profile.
  if (log_file_load.empty()) {

    // Insert the disk block wrapper into the kernel.
    cout << "Inserting wrapper module into kernel\n";
    if (test_harness.insert_wrapper() != SUCCESS) {
      cerr << "Error inserting kernel wrapper module\n";
      test_harness.cleanup_harness();
      return -1;
    }

    // Mount the file system under the wrapper module for profiling.
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
    cout << "Enabling wrapper device logging\n";
    test_harness.begin_wrapper_logging();

    // Fork off a new process and run test profiling. Forking makes it easier to
    // handle making sure everything is taken care of in profiling and ensures
    // that even if all file handles aren't closed in the test process the parent
    // won't hang due to a busy mount point.
    cout << "Running test profile\n";
    {
      char *path = (char *) "harness.socket"; // TMP: test IPC module
      const pid_t child = fork();
      if (child < 0) {
        cerr << "Error spinning off test process\n";
        test_harness.cleanup_harness();
        return -1;
      } else if (child != 0) {
        // TMP: test IPC module
        int server = start_server(path);
        int client = accept_connection(server);
        pid_t status;
        wait(&status);
        if (status != 0) {
          cerr << "Error in test process\n";
          test_harness.cleanup_harness();
          return -1;
        }
        cout << "Waiting for writeback delay\n";
        sleep(WRITE_DELAY);

        // Wait a small amount of time for writes to propogate to the block
        // layer and then stop logging writes.
        cout << "Disabling wrapper device logging" << std::endl;
        test_harness.end_wrapper_logging();
        cout << "Getting wrapper data\n";
        if (test_harness.get_wrapper_log() != SUCCESS) {
          test_harness.cleanup_harness();
          return -1;
        }
      } else {
        // Forked process' stuff.
        int r = test_harness.test_run();
        connect_server(path); // TMP: test IPC module
        return r;
      }
    }

    cout << "Unmounting wrapper file system after test profiling\n";
    if (test_harness.umount_device() != SUCCESS) {
      cerr << "Error unmounting wrapper file system\n";
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

    // Write log data out to file if we're given a file.
    if (!log_file_save.empty()) {
      cout << "Saving logged profile data to disk" << endl;
      if (test_harness.log_profile_save(log_file_save + "_profile") != SUCCESS) {
        cerr << "Error saving logged test file" << endl;
        // TODO(ashmrtn): Remove this in later versions?
        test_harness.cleanup_harness();
        return -1;
      }
    }
  } else {
    // Logged test data given, load it into the test harness.
    cout << "Loading logged profile data from disk" << endl;
    if (test_harness.log_profile_load(log_file_load + "_profile") != SUCCESS) {
      cerr << "Error loading logged test file" << endl;
      test_harness.cleanup_harness();
      return -1;
    }
  }

  /***************************************************/
  /*
#define TEST_CASE_FSCK "fsck -T -t "

  // Create a new partition table on test device.
  cout << "Partitioning test drive" << endl;
  if (test_harness.partition_drive() != SUCCESS) {
    test_harness.cleanup_harness();
    return -1;
  }

  cout << "Restoring device clone" << std::endl;
  if (test_harness.clone_device_restore() != SUCCESS) {
    test_harness.cleanup_harness();
    cerr << "Error restoring device clone" << std::endl;
    return -1;
  }

  cout << "Restoring log data" << std::endl;
  if (test_harness.test_restore_log() != SUCCESS) {
    test_harness.cleanup_harness();
    cerr << "Error restoring log data" << std::endl;
    return -1;
  }

  std::cout << "Result of current test is " << test_harness.test_check_current()
    << std::endl;
  */

  /***************************************************/

  if (!dry_run) {
    cout << "Writing profiled data to block device and checking with fsck\n";
    test_harness.test_check_random_permutations(iterations);
    test_harness.remove_cow_brd();

    cout << "Ran " << test_harness.test_test_stats[Tester::TESTS_RUN] << " tests"
          << endl
      << '\t' << test_harness.test_test_stats[Tester::TEST_FSCK_FAIL]
          << " tests fsck failed" << endl
      << '\t' << test_harness.test_test_stats[Tester::TEST_BAD_DATA]
          << " tests bad data" << endl
      << '\t' << test_harness.test_test_stats[Tester::TEST_FSCK_FIX]
          << " tests fsck fix" << endl
      << '\t' << test_harness.test_test_stats[Tester::TEST_PASS]
          << " tests passed" << endl
      << '\t' << test_harness.test_test_stats[Tester::TEST_ERR]
          << " tests errored" << endl;
    cout << "Time stats:" << endl;

    for (unsigned int i = 0; i < Tester::NUM_TIME; ++i) {
      cout << "\t" << (Tester::time_stats) i << ": "
        << test_harness.get_timing_stat((Tester::time_stats) i).count() << " ms"
        << endl;
    }
  }

  // Finish cleaning everything up.
  test_harness.cleanup_harness();
  return 0;
}
