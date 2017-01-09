#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <vector>

#include "hellow_ioctl.h"
#include "test_case.h"

#define SO_PATH "tests/"
#define MODULE_NAME "hellow.ko"
#define DEVICE_PATH "/dev/hwm"
#define EXPIRE_TIME_SIZE 10
#define DIRTY_EXPIRE_TIME_PATH "/proc/sys/vm/dirty_expire_centisecs"
// TODO(ashmrtn): Find a good delay time to use for tests.
#define TEST_EXPIRE_TIME "500"
#define WRITE_DELAY 10

#define PV_DISK       "/dev/ram0"
#define VG_DISK       "fs_consist_test"
#define LV_DISK       "fs_consist_test_dev"
#define SN_DISK       "fs_consist_test_snap"
#define FULL_LV_PATH  "/dev/" VG_DISK "/" LV_DISK
#define FULL_SN_PATH  "/dev/" VG_DISK "/" SN_DISK

#define LV_SIZE       "50%FREE"
#define SN_SIZE       "100%FREE"

#define TEST_PATH     FULL_LV_PATH
#define MNT_POINT     "/mnt/snapshot"

#define INIT_PV     "pvcreate " PV_DISK
#define DESTROY_PV  "pvremove -f " PV_DISK
#define INIT_VG     "vgcreate " VG_DISK " " PV_DISK
#define DESTROY_VG  "vgremove -f " VG_DISK
#define INIT_LV     "lvcreate " VG_DISK " -n " LV_DISK " -l " LV_SIZE
#define INIT_SN     \
  "lvcreate -s -n " SN_DISK " -l " SN_SIZE " " FULL_LV_PATH
#define DESTROY_SN  "lvremove -f " FULL_SN_PATH
#define INSMOD      "insmod " MODULE_NAME
#define RMMOD       "rmmod " MODULE_NAME
// TODO(ashmrtn): Expand to work with other file system types.
#define FMT_DRIVE   "mkfs -t ext4 " FULL_LV_PATH
#define FSCK        "fsck -t ext4 " FULL_SN_PATH " -- -y"

#define SECTOR_SIZE 512

using std::cerr;
using std::cout;
using std::vector;
using fs_testing::create_t;
using fs_testing::destroy_t;
using fs_testing::test_case;

struct disk_write {
  disk_write_op_meta metadata;
  void* data;
};

void clear_data(vector<struct disk_write>* data) {
  for (auto it = data->begin(); it != data->end(); ++it) {
    free(it->data);
  }
}

vector<struct disk_write> get_log(int device_fd) {
  vector<struct disk_write> res_data;
  while (1) {
    struct disk_write write;

    int result = ioctl(device_fd, HWM_GET_LOG_META, &write.metadata);
    if (result == -1) {
      if (errno == ENODATA) {
        break;
      } else if (errno == EFAULT) {
        cerr << "efault occurred\n";
        clear_data(&res_data);
        return vector<struct disk_write>();
      }
    }

    write.data = calloc(write.metadata.size, sizeof(char));
    if (write.data == NULL) {
      cerr << "Error getting temporary memory for log data\n";
      clear_data(&res_data);
      return vector<struct disk_write>();
    }
    result = ioctl(device_fd, HWM_GET_LOG_DATA, write.data);
    if (result == -1) {
      if (errno == ENODATA) {
        // Should never reach here as loop will break when getting the size
        // above.
        free(write.data);
        break;
      } else if (errno == EFAULT) {
        cerr << "efault occurred\n";
        free(write.data);
        clear_data(&res_data);
        return vector<struct disk_write>();
      }
    }

    res_data.push_back(write);
    /*
    printf("operation with flags: %lx\n~~~\n%s\n~~~\n",
        res_data.back().metadata.bi_rw, (char*) res_data.back().data);
    */
    result = ioctl(device_fd, HWM_NEXT_ENT);
    if (result == -1) {
      if (errno == ENODATA) {
        // Should never reach here as loop will break when getting the size
        // above.
        free(write.data);
        break;
      } else {
        cerr << "Error getting next log entry\n";
        break;
      }
    }
  }
  return res_data;
}

int get_expire_time(char* time, const int size) {
  const int expire_fd = open(DIRTY_EXPIRE_TIME_PATH, O_RDONLY);
  if (expire_fd < 0) {
    return -1;
  }
  int bytes_read = 0;
  do {
    const int res = read(expire_fd, time + bytes_read, size - bytes_read - 1);
    if (res == 0) {
      break;
    } else if (res < 0) {
      close(expire_fd);
      return -1;
    }
    bytes_read += res;
  } while (bytes_read < size - 1);
  close(expire_fd);
  // Null terminate character array.
  time[bytes_read] = '\0';
  return 0;
}

// time should be null terminated array of digits representing the new
// dirty_expire_centisecs.
int put_expire_time(const char* time) {
  const int expire_fd = open(DIRTY_EXPIRE_TIME_PATH, O_WRONLY);
  if (expire_fd < 0) {
    return -1;
  }
  const int size = strnlen(time, EXPIRE_TIME_SIZE);
  int bytes_written = 0;
  do {
    const int res = write(expire_fd, time + bytes_written,
        size - bytes_written);
    if (res < 0) {
      close(expire_fd);
      return -1;
    }
    bytes_written += res;
  } while (bytes_written < size);
  close(expire_fd);
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    cerr << "Please give a .so test case to load\n";
    return -1;
  }
  // So that jumps to error exit points work.
  void* handle = NULL;
  void* factory = NULL;
  void* killer = NULL;
  void* checker = NULL;
  const char* dl_error = NULL;
  test_case* test = NULL;
  int device_fd = -1;
  pid_t child = -1;
  pid_t status = -1;
  vector<struct disk_write> data;
  int sn_fd = -1;
  // For error reporting on exit on error.
  int res = -1;
  int res2 = -1;

  // Load the class being tested.
  cout << "Loading test class\n";
  handle = dlopen(argv[1], RTLD_LAZY);
  if (handle == NULL) {
    cerr << "Error loading test from class " << argv[1] << "\n" << dlerror()
      << "\n";
    return -1;
  }
  // Get needed methods from loaded class.
  cout << "Loading factory method\n";
  factory = dlsym(handle, "get_instance");
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig factory method " << dl_error << "\n";
    goto exit_handle;
  }
  cout << "Loading destory method\n";
  killer = dlsym(handle, "delete_instance");
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig deleter method " << dl_error << "\n";
    goto exit_handle;
  }
  test = ((create_t*)(factory))();

  cout << "Reading old dirty expire time\n";
  char old_expire_time[EXPIRE_TIME_SIZE];
  if (get_expire_time(old_expire_time, EXPIRE_TIME_SIZE) < 0) {
    cerr << "Error saving old expire_dirty_centisecs\n";
    goto exit_test_case;
  }
  cout << "Writing test dirty_expire_centisecs\n";
  if (put_expire_time(TEST_EXPIRE_TIME) < 0) {
    cerr << "Error writing new dirty_expire_centisecs\n";
    goto exit_expire_time;
  }
  cout << "Making new physical volume\n";
  if (system(INIT_PV) != 0) {
    cerr << "Error creating physical volume\n";
    goto exit_expire_time;
  }
  cout << "Making new volume group\n";
  if (system(INIT_VG) != 0) {
    cerr << "Error creating volume group\n";
    goto exit_pv;
  }
  cout << "Making new logical volume\n";
  if (system(INIT_LV) != 0) {
    cerr << "Error creating logical volume\n";
    goto exit_vg;
  }
  cout << "Formatting test drive\n";
  if (system(FMT_DRIVE) != 0) {
    cerr << "Error formatting test drive\n";
    goto exit_vg;
  }

  // Mount test file system for pre-test setup.
  cout << "Mounting test file system for pre-test setup\n";
  // TODO(ashmrtn): Fix to work with all file system types.
  if (mount(TEST_PATH, MNT_POINT, "ext4", 0, NULL) < 0) {
    cerr < "Error mounting test file system for pre-test setup\n";
    goto exit_vg;
  }

  // TODO(ashmrtn): Spin off as a new thread so that the test file system can
  // still be unmounted even if the test case doesn't properly close all file
  // handles.
  // Run pre-test setup stuff.
  cout << "Running pre-test setup\n";
  child = fork();
  if (child < 0) {
    cerr << "Error creating child process to run pre-test setup\n";
    if (umount(MNT_POINT) < 0) {
      cerr << "Error cleaning up: unmount test file system\n";
      res2 = -2;
      goto exit_expire_time;
    }
  } else if (child != 0) {
    // Parent process should wait for child to terminate before proceeding.
    wait(&status);
    sleep(WRITE_DELAY);
    child = -1;
    if (status != 0) {
      cerr << "Error in pre-test setup\n";
      if (umount(MNT_POINT) < 0) {
        cerr << "Error cleaning up: unmount test file system\n";
        res2 = -2;
        goto exit_expire_time;
      }
      goto exit_vg;
    }
    status = -1;
  } else {
    // Child should run test setup.
    res = test->setup();
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    // Exit forked process after test.
    return res;
  }

  // Unmount the test file system after pre-test setup.
  cout << "Unmounting test file system after pre-test setup\n";
  if (umount(MNT_POINT) < 0) {
    cerr << "Error unmounting test file system after pre-test setup\n";
    goto exit_expire_time;
  }

  // Create snapshot of disk for testing.
  cout << "Making new snapshot\n";
  if (system(INIT_SN) != 0) {
    cerr << "Error creating snapshot\n";
    goto exit_vg;
  }

  // Insert the disk block wrapper into the kernel.
  cout << "Inserting wrapper module into kernel\n";
  if (system(INSMOD) != 0) {
    cerr << "Error inserting kernel wrapper module\n";
    goto exit_vg;
  }

  // Mount the file system under the wrapper module for profiling.
  // TODO(ashmrtn): Fix to work with all file system types.
  cout << "Mounting wrapper file system\n";
  if (mount(DEVICE_PATH, MNT_POINT, "ext4", 0, NULL) < 0) {
    cerr << "Error mounting wrapper file system\n";
    goto exit_module;
  }

  // Get access to wrapper module ioctl functions via FD.
  cout << "Getting wrapper device ioctl fd\n";
  device_fd = open(DEVICE_PATH, O_RDONLY | O_CLOEXEC);
  if (device_fd == -1) {
    cerr << "Error opening device file\n";
    goto exit_unmount;
  }

  // Clear wrapper module logs prior to test profiling.
  cout << "Clearing wrapper device logs\n";
  ioctl(device_fd, HWM_CLR_LOG);
  ioctl(device_fd, HWM_LOG_ON);

  // Fork off a new process and run test profiling. Forking makes it easier to
  // handle making sure everything is taken care of in profiling and ensures
  // that even if all file handles aren't closed in the test process the parent
  // won't hang due to a busy mount point.
  cout << "Running test profile\n";
  child = fork();
  if (child < 0) {
    cerr << "Error spinning off test process\n";
    goto exit_device_fd;
  } else if (child != 0) {
    wait(&status);
    sleep(WRITE_DELAY);
    child = -1;
    if (status != 0) {
      cerr << "Error in test process\n";
      goto exit_device_fd;
    }
    status = -1;
  } else {
    // Forked process' stuff.
    res = test->run();
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    // Exit forked process after test.
    return res;
  }

  cout << "Unmounting wrapper file system after test profiling\n";
  if (umount(MNT_POINT) < 0) {
    cerr << "Error unmounting wrapper file system\n";
    goto exit_expire_time;
  }

  ioctl(device_fd, HWM_LOG_OFF);
  cout << "Getting profile data\n";
  data = get_log(device_fd);

  cout << "Close wrapper ioctl fd\n";
  close(device_fd);
  cout << "Removing wrapper module from kernel\n";
  if (system(RMMOD) != 0) {
    cerr << "Error cleaning up: remove wrapper module\n";
    goto exit_expire_time;
  }
  // Kill snapshot.
  cout << "Destroying profiling snapshot\n";
  if (system(DESTROY_SN) != 0) {
    cerr << "Error cleaning up: destroy profiling snapshot\n";
    goto exit_vg;
  }
  // Create new snapshot.
  cout << "Making new snapshot\n";
  if (system(INIT_SN) != 0) {
    cerr << "Error creating snapshot\n";
    goto exit_vg;
  }
  // Write recorded data out to block device in different orders so that we can
  // see if they are all valid or not.
  cout << "Opening snapshot block device\n";
  sn_fd = open(FULL_SN_PATH, O_WRONLY);
  if (sn_fd < 0) {
    cerr << "Error opening snapshot as block device\n";
    goto exit_vg;
  }
  cout << "Writing profiled data to block device and checking with fsck\n";
  for (const auto write_op : data) {
    // Operation is not a write so skip it.
    if (!(write_op.metadata.bi_rw & 1ULL)) {
      continue;
    }
    const unsigned long int byte_addr =
      write_op.metadata.write_sector * SECTOR_SIZE;
    //cout << "Moving to offset " << byte_addr << "\n";
    /*
    printf("Moving to offset 0x%lx to write %ld bytes\n", byte_addr,
        write_op.metadata.size);
    */
    res = lseek(sn_fd, byte_addr, SEEK_SET);
    if (res < 0) {
      cerr << "Error seeking in block device\n";
      close(sn_fd);
      goto exit_vg;
    }
    int bytes_written = 0;
    do {
      res = write(sn_fd,
          (void*) ((unsigned long) write_op.data + bytes_written),
          write_op.metadata.size - bytes_written);
      if (res < 0) {
        cerr << "Error writing profiled data to block device\n";
        close(sn_fd);
        goto exit_vg;
      }
      bytes_written += res;
    } while (bytes_written < write_op.metadata.size);
  }
  cout << "Closing snapshot block device\n";
  close(sn_fd);
  cout << "Running fsck on snapshot block device\n";
  res = system(FSCK);
  if (!(res == 0 || WEXITSTATUS(res) == 1)) {
    cerr << "Error running fsck on snapshot file system: " <<
      WEXITSTATUS(res) << "\n";
    goto exit_test_case;
  }
  cout << "Mounting snapshot file system for check_test\n";
  if (mount(FULL_SN_PATH, MNT_POINT, "ext4", 0, NULL) < 0) {
    cerr << "Error mounting snapshot file system for check_test\n";
    goto exit_vg;
  }
  cout << "Running check_test on snapshot file system\n";
  if (test->check_test() < 0) {
    cerr << "check_test failed: " << res << "\n";
    if (umount(MNT_POINT) < 0) {
      res2 = -2;
      cerr << "Error cleaning up: unmount snapshot file system\n";
      goto exit_expire_time;
    }
    goto exit_vg;
  }

  // Finish cleaning everything up.
  cout << "Unmounting snapshot file system\n";
  if (umount(MNT_POINT) < 0) {
    cerr << "Error cleaning up: unmount snapshot file system\n";
    res2 = -2;
    goto exit_expire_time;
  }
  cout << "Clearing write operations data\n";
  clear_data(&data);
  cout << "Removing volume group\n";
  if (system(DESTROY_VG) != 0) {
    cerr << "Error cleaning up: remove volume group(s)\n";
    res2 = -2;
    goto exit_expire_time;
  }
  cout << "Removing physical volume\n";
  if (system(DESTROY_PV) != 0) {
    cerr << "Error cleaning up: remove physical volume(s)\n";
    res2 = -2;
    goto exit_expire_time;
  }
  cout << "Restoring dirty_expire_time\n";
  if (put_expire_time(old_expire_time) < 0) {
    cerr << "Error cleaning up: fix dirty_expire_centisecs\n";
    res2 = -2;
    goto exit_test_case;
  }
  cout << "Destroying test case\n";
  ((destroy_t*)(killer))(test);
  dlclose(handle);
  return 0;

 exit_device_fd:
  close(device_fd);
 exit_unmount:
  if (umount(MNT_POINT) < 0) {
    cerr << "Error cleaning up: unmount wrapper file system\n";
    res2 = -2;
    goto exit_expire_time;
  }
 exit_module:
  if (system(RMMOD) != 0) {
    cerr << "Error cleaning up: remove wrapper module\n";
    res2 = -2;
    goto exit_expire_time;
  }
 exit_vg:
  // Remove volume groups that were created.
  if (system(DESTROY_VG) != 0) {
    cerr << "Error cleaning up: remove volume group(s)\n";
    res2 = -2;
  }
 exit_pv:
  // Remove physical volumes that were created.
  if (system(DESTROY_PV) != 0) {
    cerr << "Error cleaning up: remove physical volume(s)\n";
    res2 = -2;
  }
 exit_expire_time:
  // Restore old dirty_expire_centisecs.
  if (put_expire_time(old_expire_time) < 0) {
    cerr << "Error cleaning up: fix dirty_expire_centisecs\n";
    res2 = -2;
  }
 exit_test_case:
  ((destroy_t*)(killer))(test);
 exit_handle:
  dlclose(handle);
  return res2;
}
