#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <wait.h>

#include <ctime>

#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include "../tests/BaseTestCase.h"
#include "../utils/communication/ServerSocket.h"
#include "../utils/communication/SocketUtils.h"
#include "../utils/utils.h"
#include "Tester.h"

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define TEST_SO_PATH "tests/"
#define PERMUTER_SO_PATH "permuter/"
// TODO(ashmrtn): Find a good delay time to use for tests.
#define TEST_DIRTY_EXPIRE_TIME_CENTISECS 3000
#define TEST_DIRTY_EXPIRE_TIME_STRING \
  TO_STRING(TEST_DIRTY_EXPIRE_TIME_CENTISECS)
#define MOUNT_DELAY 1

#define DIRECTORY_PERMS \
  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define OPTS_STRING "bd:cf:e:l:m:np:r:s:t:vFIPS:"

namespace {

static const unsigned int kSocketQueueDepth = 2;
static constexpr char kChangePath[] = "run_changes";

}  // namespace

using std::cerr;
using std::cout;
using std::endl;
using std::ofstream;
using std::string;
using std::to_string;
using fs_testing::Tester;
using fs_testing::utils::communication::kSocketNameOutbound;
using fs_testing::utils::communication::ServerSocket;
using fs_testing::utils::communication::SocketError;
using fs_testing::utils::communication::SocketMessage;

static const option long_options[] = {
  {"background", no_argument, NULL, 'b'},
  {"automate_check_test", no_argument, NULL, 'c'},
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
  {"full-bio-replay", no_argument, NULL, 'F'},
  {"no-in-order-replay", no_argument, NULL, 'I'},
  {"no-permuted-order-replay", no_argument, NULL, 'P'},
  {"sector-size", required_argument, NULL, 'S'},
  {0, 0, 0, 0},
};

int main(int argc, char** argv) {
  cout << "running " << argv << endl;

  string dirty_expire_time_centisecs(TEST_DIRTY_EXPIRE_TIME_STRING);
  string fs_type("ext4");
  string flags_dev("/dev/vda");
  string test_dev("/dev/ram0");
  string mount_opts("");
  string log_file_save("");
  string log_file_load("");
  string permuter(PERMUTER_SO_PATH "RandomPermuter.so");
  bool background = false;
  bool automate_check_test = false;
  bool dry_run = false;
  bool no_lvm = false;
  bool verbose = false;
  bool in_order_replay = true;
  bool permuted_order_replay = true;
  bool full_bio_replay = false;
  int iterations = 10000;
  int disk_size = 10240;
  unsigned int sector_size = 512;
  int option_idx = 0;
  ServerSocket* background_com = NULL;

  // Parse command line arguments.
  for (int c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx);
        c != -1;
        c = getopt_long(argc, argv, OPTS_STRING, long_options, &option_idx)) {
    switch (c) {
      case 'b':
        background = true;
        break;
      case 'c':
        automate_check_test = true;
        break;
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
        in_order_replay = false;
        permuted_order_replay = false;
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
        // Convert to lower so we can compare against it later if we want.
        for (auto c : fs_type) {
          c = std::tolower(c);
        }
        break;
      case 'v':
        verbose = true;
        break;
      case 'F':
        full_bio_replay = true;
        break;
      case 'I':
        in_order_replay = false;
        break;
      case 'P':
        permuted_order_replay = false;
        break;
      case 'S':
        sector_size = atoi(optarg);
        break;
      case '?':
      default:
        return -1;
    }
  }


  /*****************************************************************************
   * PHASE 0:
   * Basic setup of the test harness:
   * 1. check arguments are sane
   * 2. load up socket connections if/when needed
   * 3. load basic kernel modules
   * 4. load static objects for permuter and test case
   ****************************************************************************/
  const unsigned int test_case_idx = optind;
  const string path = argv[test_case_idx];

  // Get the name of the test being run.
  int begin = path.rfind('/');
  // Remove everything before the last /.
  string test_name = path.substr(begin + 1);
  // Remove the extension.
  test_name = test_name.substr(0, test_name.length() - 3);
  // Get the date and time stamp and format.
  time_t now = time(0);
  char time_st[18];
  strftime(time_st, sizeof(time_st), "%Y%m%d_%H%M%S", localtime(&now));
  string s = string(time_st) + "-" + test_name + ".log";
  ofstream logfile(s);

  // This should be changed in the option is added to mount tests in other
  // directories.
  string mount_dir = "/mnt/snapshot"; 
  if(setenv("MOUNT_FS", mount_dir.c_str(), 1) == -1){
    cerr << "Error setting environment variable MOUNT_FS" << endl;
  }
  
  cout << "========== PHASE 0: Setting up CrashMonkey basics =========="
    << endl;
  logfile << "========== PHASE 0: Setting up CrashMonkey basics =========="
    << endl;
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

  if (sector_size <= 0) {
    cerr << "Please give a positive number for the sector size" << endl;
    return -1;
  }

  // Create a socket to coordinate with the outside world.
  // TODO(ashmrtn): Fix permissions on the socket.
  /*
  struct stat socket_dir;
  int res = stat(SOCKET_DIR, &socket_dir);
  // Directory does not exist.
  if (res < 0 && errno == 2) {
    if (mkdir(SOCKET_DIR, DIRECTORY_PERMS) < 0) {
      cerr << "Error creating temp directory" << endl;
      delete background_com;
      return -1;
    }
  } else if (res < 0) {
    // Some other error.
    cerr << "Error trying to find temp directory" << endl;
    delete background_com;
    return -1;
  } else if (!S_ISDIR(socket_dir.st_mode)) {
    // Something there that's not a directory.
    cerr << "Something not a directory already exists at " << SOCKET_DIR
      << endl;
    delete background_com;
    return -1;
  }

  // Incorrect permissions.
  if ((socket_dir.st_mode & DIRECTORY_PERMS) != DIRECTORY_PERMS) {
    cout << "Changing permissions on " << SOCKET_DIR << " to be world "
      << "readable and writable" << endl;
    if (chmod(SOCKET_DIR, DIRECTORY_PERMS) < 0) {
      cerr << "Error changing permissions on " << SOCKET_DIR << endl;
      delete background_com;
      return -1;
    }
  }
  */

  background_com = new ServerSocket(kSocketNameOutbound);
  if (background_com->Init(kSocketQueueDepth) < 0) {
    int err_no = errno;
    cerr << "Error starting socket to listen on " << err_no << endl;
    delete background_com;
    return -1;
  }


  Tester test_harness(disk_size, sector_size, verbose);
  test_harness.StartTestSuite();

  cout << "Inserting RAM disk module" << endl;
  logfile << "Inserting RAM disk module" << endl;
  if (test_harness.insert_cow_brd() != SUCCESS) {
    cerr << "Error inserting RAM disk module" << endl;
    return -1;
  }
  test_harness.set_fs_type(fs_type);
  test_harness.set_device(test_dev);
  FILE *input;
  char buf[512];
  if(!(input = popen(("fdisk -l " + test_dev + " | grep " + test_dev + ": ").c_str(), "r"))){
    cerr << "Error finding the filesize of mounted filesystem" << endl;  
  }
  string filesize;
  while(fgets(buf, 512, input)){
    filesize += buf;
  }
  pclose(input);
  char *filesize_cstr = new char[filesize.length() + 1];
  strcpy(filesize_cstr, filesize.c_str()); 
  char * tok = strtok(filesize_cstr, " ");
  int pos = 0;
  while (pos < 4 && tok != NULL){
    pos ++;
    tok = strtok(NULL, " ");
  }
  long test_dev_size = 0;
  if(tok != NULL){
    test_dev_size = atol(tok);
  }
  delete [] filesize_cstr;
  if(setenv("FILESYS_SIZE", filesize.c_str(), 1) == -1){
    cerr << "Error setting environment variable FILESYS_SIZE" << endl;
  }
  
  // Load the class being tested.
  cout << "Loading test case" << endl;
  if (test_harness.test_load_class(argv[test_case_idx]) != SUCCESS) {
    test_harness.cleanup_harness();
      return -1;
  }
  
  test_harness.test_init_values(mount_dir, test_dev_size);
  
  // Load the permuter to use for the test.
  // TODO(ashmrtn): Consider making a line in the test file which specifies the
  // permuter to use?
  cout << "Loading permuter" << endl;
  logfile << "Loading permuter" << endl;
  if (test_harness.permuter_load_class(permuter.c_str()) != SUCCESS) {
    test_harness.cleanup_harness();
      return -1;
  }

  // Update dirty_expire_time.
  cout << "Updating dirty_expire_time_centisecs to "
    << dirty_expire_time_centisecs << endl;
  logfile << "Updating dirty_expire_time_centisecs to "
    << dirty_expire_time_centisecs << endl;
  const char* old_expire_time =
    test_harness.update_dirty_expire_time(dirty_expire_time_centisecs.c_str());
  if (old_expire_time == NULL) {
    cerr << "Error updating dirty_expire_time_centisecs" << endl;
    test_harness.cleanup_harness();
    return -1;
  }


  /*****************************************************************************
   * PHASE 1:
   * Setup the base image of the disk for snapshots later. This could happen in
   * one of several ways:
   * 1. The -r flag specifies that there are log files which contain the disk
   *    image. These will now be loaded from disk
   * 2. The -b flag specifies that CrashMonkey is running as a "background"
   *    process of sorts and should listen on its socket for commands from the
   *    user telling it when to perform certain actions. The user is responsible
   *    for running their own pre-test setup methods at the proper times
   * 3. CrashMonkey is running as a standalone program. It will run the pre-test
   *    setup methods defined in the test case static object it loaded
   ****************************************************************************/

  cout << endl << "========== PHASE 1: Creating base disk image =========="
    << endl;
  logfile << endl << "========== PHASE 1: Creating base disk image =========="
    << endl;
  // Run the normal test setup stuff if we don't have a log file.
  if (log_file_load.empty()) {
    /***************************************************************************
     * Setup for both background operation and standalone mode operation.
     **************************************************************************/
    if (flags_dev.empty()) {
      cerr << "No device to copy flags from given" << endl;
      return -1;
    }

    // Device flags only need set if we are logging requests.
    test_harness.set_flag_device(flags_dev);

    // Format test drive to desired type.
    cout << "Formatting test drive" << endl;
    logfile << "Formatting test drive" << endl;
    if (test_harness.format_drive() != SUCCESS) {
      cerr << "Error formatting test drive" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // Mount test file system for pre-test setup.
    cout << "Mounting test file system for pre-test setup" << endl;
    logfile << "Mounting test file system for pre-test setup" << endl;
    if (test_harness.mount_device_raw(mount_opts.c_str()) != SUCCESS) {
      cerr << "Error mounting test device" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // TODO(ashmrtn): Close startup socket fd here.

    if (background) {
      cout << "+++++ Please run any needed pre-test setup +++++" << endl;
      logfile << "+++++ Please run any needed pre-test setup +++++" << endl;
      /*************************************************************************
       * Background mode user setup. Wait for the user to tell use that they
       * have finished the pre-test setup phase.
       ************************************************************************/
      SocketMessage command;
      do {
        if (background_com->WaitForMessage(&command) != SocketError::kNone) {
          cerr << "Error getting message from socket" << endl;
          delete background_com;
          test_harness.cleanup_harness();
          return -1;
        }

        if (command.type != SocketMessage::kBeginLog) {
          if (background_com->SendCommand(SocketMessage::kInvalidCommand) !=
              SocketError::kNone) {
            cerr << "Error sending response to client" << endl;
            delete background_com;
            test_harness.cleanup_harness();
            return -1;
          }
          background_com->CloseClient();
        }
      } while (command.type != SocketMessage::kBeginLog);
    } else {
      /*************************************************************************
       * Standalone mode user setup. Run the pre-test "setup()" method defined
       * in the test case. Run as a separate process for the sake of
       * cleanliness.
       ************************************************************************/
      cout << "Running pre-test setup" << endl;
      logfile << "Running pre-test setup" << endl;
      {
        const pid_t child = fork();
        if (child < 0) {
          cerr << "Error creating child process to run pre-test setup" << endl;
          test_harness.cleanup_harness();
          return -1;
        } else if (child != 0) {
          // Parent process should wait for child to terminate before proceeding.
          pid_t status;
          wait(&status);
          if (status != 0) {
            cerr << "Error in pre-test setup" << endl;
            test_harness.cleanup_harness();
            return -1;
          }
        } else {
          return test_harness.test_setup();
        }
      }
    }

    /***************************************************************************
     * Pre-test setup complete. Unmount the test file system and snapshot the
     * disk for use in workload and tests.
     **************************************************************************/
    // Unmount the test file system after pre-test setup.
    cout << "Unmounting test file system after pre-test setup" << endl;
    logfile << "Unmounting test file system after pre-test setup" << endl;
    if (test_harness.umount_device() != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }

    // Create snapshot of disk for testing.
    cout << "Making new snapshot" << endl;
    logfile << "Making new snapshot" << endl;
    if (test_harness.clone_device() != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }

    // If we're logging this test run then also save the snapshot.
    if (!log_file_save.empty()) {
      /*************************************************************************
       * The -l flag specifies that we should save the information for this
       * harness execution. Therefore, save the disk image we are using as the
       * base image for our snapshots.
       ************************************************************************/
      cout << "Saving snapshot to log file" << endl;
      logfile << "Saving snapshot to log file" << endl;
      if (test_harness.log_snapshot_save(log_file_save + "_snap")
          != SUCCESS) {
        test_harness.cleanup_harness();
        return -1;
      }
    }
  } else {
    /***************************************************************************
     * The -r flag specifies that we should load information from the provided
     * log file. Load the base disk image for snapshots here.
     **************************************************************************/
    // Load the snapshot in the log file and then write it to disk.
    cout << "Loading saved snapshot" << endl;
    logfile << "Loading saved snapshot" << endl;
    if (test_harness.log_snapshot_load(log_file_load + "_snap") != SUCCESS) {
      test_harness.cleanup_harness();
      return -1;
    }
  }


  /*****************************************************************************
   * PHASE 2:
   * Obtain a series of disk epochs to operate on in the test phase of the
   * harness. Again, this could happen in one of several ways:
   * 1. The -r flag specifies that there are log files which contain the disk
   *    epochs. These will now be loaded from disk
   * 2. The -b flag specifies that CrashMonkey is running as a "background"
   *    process of sorts and should listen on its socket for commands from the
   *    user telling it when to perform certain actions. The user is responsible
   *    for running their own workload methods at the proper times
   * 3. CrashMonkey is running as a standalone program. It will run the workload
   *    methods defined in the test case static object it loaded
   ****************************************************************************/

  cout << endl << "========== PHASE 2: Recording user workload =========="
    << endl;
  logfile << endl << "========== PHASE 2: Recording user workload =========="
    << endl;
  // TODO(ashmrtn): Consider making a flag for this?
  cout << "Clearing caches" << endl;
  logfile << "Clearing caches" << endl;
  if (test_harness.clear_caches() != SUCCESS) {
    cerr << "Error clearing caches" << endl;
    test_harness.cleanup_harness();
    return -1;
  }

  // No log file given so run the test profile.
  if (log_file_load.empty()) {
    /***************************************************************************
     * Preparations for both background operation and standalone mode operation.
     **************************************************************************/

    // Insert the disk block wrapper into the kernel.
    cout << "Inserting wrapper module into kernel" << endl;
    logfile << "Inserting wrapper module into kernel" << endl;
    if (test_harness.insert_wrapper() != SUCCESS) {
      cerr << "Error inserting kernel wrapper module" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // Get access to wrapper module ioctl functions via FD.
    cout << "Getting wrapper device ioctl fd" << endl;
    logfile << "Getting wrapper device ioctl fd" << endl;
    if (test_harness.get_wrapper_ioctl() != SUCCESS) {
      cerr << "Error opening device file" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // Clear wrapper module logs prior to test profiling.
    cout << "Clearing wrapper device logs" << endl;
    logfile << "Clearing wrapper device logs" << endl;
    test_harness.clear_wrapper_log();
    cout << "Enabling wrapper device logging" << endl;
    logfile << "Enabling wrapper device logging" << endl;
    test_harness.begin_wrapper_logging();

    // We also need to log the changes made by mount of the FS
    // because the snapshot is taken after an unmount.
    
    // Mount the file system under the wrapper module for profiling.
    cout << "Mounting wrapper file system" << endl;
    if (test_harness.mount_wrapper_device(mount_opts.c_str()) != SUCCESS) {
      cerr << "Error mounting wrapper file system" << endl;
      test_harness.cleanup_harness();
      return -1;
    }

    // TODO(ashmrtn): Can probably remove this...
    /*
    cout << "Sleeping after mount" << endl;
    unsigned int to_sleep = MOUNT_DELAY;
    do {
      to_sleep = sleep(to_sleep);
    } while (to_sleep > 0);
    */

    /***************************************************************************
     * Run the actual workload that we will be testing.
     **************************************************************************/
    if (background) {
      /************************************************************************
       * Background mode user workload. Tell the user we have finished workload
       * preparations and are ready for them to run the workload since we are
       * now logging requests.
       ***********************************************************************/
      if (background_com->SendCommand(SocketMessage::kBeginLogDone) !=
          SocketError::kNone) {
        cerr << "Error telling user ready for workload" << endl;
        delete background_com;
        test_harness.cleanup_harness();
        return -1;
      }
      background_com->CloseClient();

      cout << "+++++ Please run workload +++++" << endl;
      logfile << "+++++ Please run workload +++++" << endl;

      // Wait for the user to tell us they are done with the workload.
      SocketMessage command;
      bool done = false;
      do {
        if (background_com->WaitForMessage(&command) != SocketError::kNone) {
          cerr << "Error getting command from socket" << endl;
          delete background_com;
          test_harness.cleanup_harness();
          return -1;
        }

        switch (command.type) {
          case SocketMessage::kEndLog:
            done = true;
            break;
          case SocketMessage::kCheckpoint:
            if (test_harness.CreateCheckpoint() == SUCCESS) {
              if (background_com->SendCommand(SocketMessage::kCheckpointDone) !=
                  SocketError::kNone) {
                cerr << "Error telling user done with checkpoint" << endl;
                delete background_com;
                test_harness.cleanup_harness();
                return -1;
              }
            } else {
              if (background_com->SendCommand(SocketMessage::kCheckpointFailed)
                  != SocketError::kNone) {
                cerr << "Error telling user checkpoint failed" << endl;
                delete background_com;
                test_harness.cleanup_harness();
                return -1;
              }
            }
            background_com->CloseClient();
            break;
          default:
            if (background_com->SendCommand(SocketMessage::kInvalidCommand) !=
                SocketError::kNone) {
              cerr << "Error sending response to client" << endl;
              delete background_com;
              test_harness.cleanup_harness();
              return -1;
            }
            background_com->CloseClient();
            break;
        }
      } while (!done);
    } else {
      /*************************************************************************
       * Standalone mode user workload. Fork off a new process and run test
       * profiling. Forking makes it easier to handle making sure everything is
       * taken care of in profiling and ensures that even if all file handles
       * aren't closed in the process running the worload, the parent won't hang
       * due to a busy mount point.
       ************************************************************************/
      cout << "Running test profile" << endl;
      logfile << "Running test profile" << endl;
      bool last_checkpoint = false;
      int checkpoint = 0;
      /*************************************************************************
       * If automated_check_test is enabled, a snapshot is taken at every checkpoint
       * in the run() workload. The first iteration is the complete execution of run()
       * and is profiled. Subsequent iterations save snapshots at every checkpoint()
       * present in the run() workload.
       ************************************************************************/
      do {
        {
          const pid_t child = fork();
          if (child < 0) {
            cerr << "Error spinning off test process" << endl;
            test_harness.cleanup_harness();
            return -1;
          } else if (child != 0) {
            pid_t status = -1;
            pid_t wait_res = 0;
            do {
              SocketMessage m;
              SocketError se;

              se = background_com->TryForMessage(&m);

              if (se == SocketError::kNone) {
                if (m.type == SocketMessage::kCheckpoint) {
                  if (test_harness.CreateCheckpoint() == SUCCESS) {
                    if (background_com->SendCommand(
                            SocketMessage::kCheckpointDone)
                          != SocketError::kNone) {
                        // TODO(ashmrtn): Handle better.
                        cerr << "Error telling user done with checkpoint" << endl;
                        delete background_com;
                        test_harness.cleanup_harness();
                        return -1;
                    }
                  } else {
                    if (background_com->SendCommand(
                          SocketMessage::kCheckpointFailed)
                        != SocketError::kNone) {
                      // TODO(ashmrtn): Handle better.
                      cerr << "Error telling user checkpoint failed" << endl;
                      delete background_com;
                      test_harness.cleanup_harness();
                      return -1;
                    }
                  }
                } else {
                  if (background_com->SendCommand(
                        SocketMessage::kInvalidCommand)
                      != SocketError::kNone) {
                    cerr << "Error sending response to client" << endl;
                    delete background_com;
                    test_harness.cleanup_harness();
                    return -1;
                  }
                }
                background_com->CloseClient();
              }
              wait_res = waitpid(child, &status, WNOHANG);
            } while (wait_res == 0);
            if (WIFEXITED(status) == 0) {
              cerr << "Error terminating test_run process, status: " << status << endl;
              test_harness.cleanup_harness();
              return -1;
            } else {
              if (WEXITSTATUS(status) == 1) {
                last_checkpoint = true;
              } else if (WEXITSTATUS(status) == 0) {
                if (checkpoint == 0) {
                  cout << "Completely executed run process" << endl;
                } else {
                  cout << "Run process hit checkpoint " << checkpoint << endl;
                }
              } else {
                cerr << "Error in test run, exits with status: " << status << endl;
                test_harness.cleanup_harness();
                return -1;
              }
            }
          } else {
            // Forked process' stuff.
            int change_fd;
            if (checkpoint == 0) {
              change_fd = open(kChangePath, O_CREAT | O_WRONLY | O_TRUNC,
                S_IRUSR | S_IWUSR);
              if (change_fd < 0) {
                return change_fd;
              }
            }
            const int res = test_harness.test_run(change_fd, checkpoint);

            if (checkpoint == 0) {
              close(change_fd);
            }
            return res;
          }
        }
        // End wrapper logging for profiling the complete execution of run process
        if (checkpoint == 0) {
          cout << "Waiting for writeback delay" << endl;
          logfile << "Waiting for writeback delay" << endl;
          unsigned int sleep_time = test_harness.GetPostRunDelay();
          while (sleep_time > 0) {
            sleep_time = sleep(sleep_time);
          }

          cout << "Disabling wrapper device logging" << endl;
          logfile << "Disabling wrapper device logging" << endl;
          test_harness.end_wrapper_logging();
          cout << "Getting wrapper data" << endl;
          logfile << "Getting wrapper data" << endl;
          if (test_harness.get_wrapper_log() != SUCCESS) {
            test_harness.cleanup_harness();
            return -1;
          }

          cout << "Unmounting wrapper file system after test profiling" << endl;
          logfile << "Unmounting wrapper file system after test profiling" << endl;
          if (test_harness.umount_device() != SUCCESS) {
            cerr << "Error unmounting wrapper file system" << endl;
            test_harness.cleanup_harness();
            return -1;
          }

          cout << "Close wrapper ioctl fd" << endl;
          logfile << "Close wrapper ioctl fd" << endl;
          test_harness.put_wrapper_ioctl();
          cout << "Removing wrapper module from kernel" << endl;
          logfile << "Removing wrapper module from kernel" << endl;
          if (test_harness.remove_wrapper() != SUCCESS) {
            cerr << "Error cleaning up: remove wrapper module" << endl;
            test_harness.cleanup_harness();
            return -1;
          }

          // Getting the tracking data
          cout << "Getting change data" << endl;
          logfile << "Getting change data" << endl;
          const int change_fd = open(kChangePath, O_RDONLY);
          if (change_fd < 0) {
            cerr << "Error reading change data" << endl;
            test_harness.cleanup_harness();
            return -1;
          }

          if (lseek(change_fd, 0, SEEK_SET) < 0) {
            cerr << "Error reading change data" << endl;
            test_harness.cleanup_harness();
            return -1;
          }

          if (test_harness.GetChangeData(change_fd) != SUCCESS) {
            test_harness.cleanup_harness();
            return -1;
          }
        } 

        if (automate_check_test) {
          // Map snapshot of the disk to the current checkpoint and unmount the clone
          test_harness.mapCheckpointToSnapshot(checkpoint);
          if (checkpoint != 0) {
            if (test_harness.umount_snapshot() != SUCCESS) {
              test_harness.cleanup_harness();
              return -1;
            }
          }
          // get a new diskclone and mount it for next the checkpoint
          test_harness.getNewDiskClone(checkpoint);
          if (!last_checkpoint) {
            if (test_harness.mount_snapshot() != SUCCESS) {
              test_harness.cleanup_harness();
              return -1;
            }
          }
        }
        // reset the snapshot path if we completed all the executions
        if (automate_check_test && last_checkpoint) {
          test_harness.getCompleteRunDiskClone();
        }
        // Increment the checkpoint at which run exits
        checkpoint += 1;
      } while (!last_checkpoint && automate_check_test);
    }

    /***************************************************************************
     * Worload complete, Clean up things and end logging.
     **************************************************************************/

    // Wait a small amount of time for writes to propogate to the block
    // layer and then stop logging writes.
    // TODO (P.S.) pull out the common code between the code path when
    // checkpoint is zero above and if background mode is on here
    if (background) {
      cout << "Waiting for writeback delay" << endl;
      logfile << "Waiting for writeback delay" << endl;
      unsigned int sleep_time = test_harness.GetPostRunDelay();
      while (sleep_time > 0) {
        sleep_time = sleep(sleep_time);
      }

      cout << "Disabling wrapper device logging" << endl;
      logfile << "Disabling wrapper device logging" << endl;
      test_harness.end_wrapper_logging();
      cout << "Getting wrapper data" << endl;
      logfile << "Getting wrapper data" << endl;
      if (test_harness.get_wrapper_log() != SUCCESS) {
        test_harness.cleanup_harness();
        return -1;
      }

      cout << "Unmounting wrapper file system after test profiling" << endl;
      logfile << "Unmounting wrapper file system after test profiling" << endl;
      if (test_harness.umount_device() != SUCCESS) {
        cerr << "Error unmounting wrapper file system" << endl;
        test_harness.cleanup_harness();
        return -1;
      }

      cout << "Close wrapper ioctl fd" << endl;
      logfile << "Close wrapper ioctl fd" << endl;
      test_harness.put_wrapper_ioctl();
      cout << "Removing wrapper module from kernel" << endl;
      logfile << "Removing wrapper module from kernel" << endl;
      if (test_harness.remove_wrapper() != SUCCESS) {
        cerr << "Error cleaning up: remove wrapper module" << endl;
        test_harness.cleanup_harness();
        return -1;
      }

      // Getting the tracking data
      cout << "Getting change data" << endl;
      logfile << "Getting change data" << endl;
      const int change_fd = open(kChangePath, O_RDONLY);
      if (change_fd < 0) {
        cerr << "Error reading change data" << endl;
        test_harness.cleanup_harness();
        return -1;
      }

      if (lseek(change_fd, 0, SEEK_SET) < 0) {
        cerr << "Error reading change data" << endl;
        test_harness.cleanup_harness();
        return -1;
      }

      if (test_harness.GetChangeData(change_fd) != SUCCESS) {
        test_harness.cleanup_harness();
        return -1;
      }
    }

    logfile << endl << endl << "Recorded workload:" << endl;
    test_harness.log_disk_write_data(logfile);
    logfile << endl << endl;

    // Write log data out to file if we're given a file.
    if (!log_file_save.empty()) {
      /*************************************************************************
       * The -l flag specifies that we should save the information for this
       * harness execution. Therefore, save the series of disk epochs we just
       * logged so they can be reused later if the -r flag is given.
       ************************************************************************/
      cout << "Saving logged profile data to disk" << endl;
      logfile << "Saving logged profile data to disk" << endl;
      if (test_harness.log_profile_save(log_file_save + "_profile") != SUCCESS) {
        cerr << "Error saving logged test file" << endl;
        // TODO(ashmrtn): Remove this in later versions?
        test_harness.cleanup_harness();
        return -1;
      }
    }

    /***************************************************************************
     * Background mode. Tell the user we have finished logging and cleaning up
     * and that, if they need to, they can do a bit of cleanup on their end
     * before beginning testing.
     **************************************************************************/
    if (background) {
      if (background_com->SendCommand(SocketMessage::kEndLogDone) !=
          SocketError::kNone) {
        cerr << "Error telling user done logging" << endl;
        delete background_com;
        test_harness.cleanup_harness();
        return -1;
      }
      background_com->CloseClient();
    }
  } else {
    /***************************************************************************
     * The -r flag specifies that we should load information from the provided
     * log file. Load the series of disk epochs which we will be operating on.
     **************************************************************************/
    cout << "Loading logged profile data from disk" << endl;
    logfile << "Loading logged profile data from disk" << endl;
    if (test_harness.log_profile_load(log_file_load + "_profile") != SUCCESS) {
      cerr << "Error loading logged test file" << endl;
      test_harness.cleanup_harness();
      return -1;
    }
  }


  /*****************************************************************************
   * PHASE 3:
   * Now that we have finished gathering data, run tests to see if we can find
   * file system inconsistencies. Either:
   * 1. The -b flag specifies that CrashMonkey is running as a "background"
   *    process of sorts and should listen on its socket for the command telling
   *    it to begin testing
   * 2. CrashMonkey is running as a standalone program. It should immediately
   *    begin testing
   ****************************************************************************/

  if (background) {
    /***************************************************************************
     * Background mode. Wait for the user to tell us to start testing.
     **************************************************************************/
    SocketMessage command;
    do {
      cout << "+++++ Ready to run tests, please confirm start +++++" << endl;
      logfile << "+++++ Ready to run tests, please confirm start +++++" << endl;
      if (background_com->WaitForMessage(&command) != SocketError::kNone) {
        cerr << "Error getting command from socket" << endl;
        delete background_com;
        test_harness.cleanup_harness();
        return -1;
      }

      if (command.type != SocketMessage::kRunTests) {
        if (background_com->SendCommand(SocketMessage::kInvalidCommand) !=
            SocketError::kNone) {
          cerr << "Error sending response to client" << endl;
          delete background_com;
          test_harness.cleanup_harness();
          return -1;
        }
        background_com->CloseClient();
      }
    } while (command.type != SocketMessage::kRunTests);
  }

  cout << endl
    << "========== PHASE 3: Running tests based on recorded data =========="
    << endl;
  logfile << endl
    << "========== PHASE 3: Running tests based on recorded data =========="
    << endl;


  // TODO(ashmrtn): Fix the meaning of "dry-run". Right now it means do
  // everything but run tests (i.e. run setup and profiling but not testing.)
  /***************************************************************************
   * Run tests and print the results of said tests.
   **************************************************************************/
  if (permuted_order_replay) {
    cout << "Writing profiled data to block device and checking with fsck" <<
      endl;
    logfile << "Writing profiled data to block device and checking with fsck" <<
      endl;

    test_harness.test_check_random_permutations(full_bio_replay, iterations,
        logfile);

    for (unsigned int i = 0; i < Tester::NUM_TIME; ++i) {
      cout << "\t" << (Tester::time_stats) i << ": " <<
        test_harness.get_timing_stat((Tester::time_stats) i).count() <<
        " ms" << endl;
    }
  }

  if (in_order_replay) {
    cout << endl << endl <<
      "Writing data out to each Checkpoint and checking with fsck" << endl;
    logfile << endl << endl <<
      "Writing data out to each Checkpoint and checking with fsck" << endl;
    test_harness.test_check_log_replay(logfile, automate_check_test);
  }

  cout << endl;
  logfile << endl;
  test_harness.PrintTestStats(cout);
  test_harness.PrintTestStats(logfile);
  test_harness.EndTestSuite();

  cout << endl << "========== PHASE 4: Cleaning up ==========" << endl;
  logfile << endl << "========== PHASE 4: Cleaning up ==========" << endl;

  /*****************************************************************************
   * PHASE 4:
   * We have finished. Clean up the test harness. Tell the user we have finished
   * testing if the -b flag was given and we are running in background mode.
   ****************************************************************************/
  logfile.close();
  test_harness.remove_cow_brd();
  test_harness.cleanup_harness();

  if (background) {
    if (background_com->SendCommand(SocketMessage::kRunTestsDone) !=
        SocketError::kNone) {
      cerr << "Error telling user done testing" << endl;
      delete background_com;
      test_harness.cleanup_harness();
      return -1;
    }
  }
  delete background_com;

  return 0;
}
