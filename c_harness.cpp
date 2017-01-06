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
#define WRITE_DELAY 30
#define EXPIRE_TIME_SIZE 10
#define DIRTY_EXPIRE_TIME_PATH "/proc/sys/vm/dirty_expire_centisecs"
#define TEST_EXPIRE_TIME "1500"

#define PV_DISK "/dev/ram0"
#define VG_DISK "fs_consist_test"
#define LV_DISK "fs_consist_test_dev"
#define SN_DISK "fs_consist_test_snap"
#define LV_SIZE "50%FREE"
#define SN_SIZE "100%FREE"

#define TEST_PATH "/dev/" VG_DISK "/" LV_DISK
#define MNT_POINT "/mnt/snapshot"

#define INIT_PV     "pvcreate " PV_DISK
#define DESTROY_PV  "pvremove -f " PV_DISK
#define INIT_VG     "vgcreate " VG_DISK " " PV_DISK
#define DESTROY_VG  "vgremove -f " VG_DISK
#define INIT_LV     "lvcreate " VG_DISK " -n " LV_DISK " -l " LV_SIZE
#define INIT_SN     \
  "lvcreate -s -n " SN_DISK " -l " SN_SIZE " /dev/" VG_DISK "/" LV_DISK
#define DESTROY_SN  "lvremove -f /dev/" VG_DISK "/" SN_DISK
#define INSMOD      "insmod " MODULE_NAME
#define RMMOD       "rmmod " MODULE_NAME
// TODO(ashmrtn): Expand to work with other file system types.
#define FMT_DRIVE   "mkfs -t ext4 /dev/" VG_DISK "/" LV_DISK

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
  void* handle;
  void* factory;
  void* killer;
  const char* dl_error;
  test_case* test;
  int err = 0;
  int device_fd = -1;
  pid_t child;
  pid_t status;
  vector<struct disk_write> data;
  int sn_fd;
  // For error reporting on exit on error.
  int res2 = -1;
  cout << "Reading old dirty expire time\n";
  char old_expire_time[EXPIRE_TIME_SIZE];
  int res = get_expire_time(old_expire_time, EXPIRE_TIME_SIZE);
  if (res < 0) {
    cerr << "Error saving old expire_dirty_centisecs\n";
    return -1;
  }
  cout << "Writing test dirty_expire_centisecs\n";
  res = put_expire_time(TEST_EXPIRE_TIME);
  if (res < 0) {
    cerr << "Error writing new dirty_expire_centisecs\n";
    return -1;
  }
  cout << "Making new physical volume\n";
  res = system(INIT_PV);
  if (res != 0) {
    cerr << "Error creating physical volume\n";
    goto exit_expire_time;
  }
  cout << "Making new volume group\n";
  res = system(INIT_VG);
  if (res != 0) {
    cerr << "Error creating volume group\n";
    goto exit_pv;
  }
  cout << "Making new logical volume\n";
  res = system(INIT_LV);
  if (res != 0) {
    cerr << "Error creating logical volume\n";
    goto exit_vg;
  }
  cout << "Formatting test drive\n";
  res = system(FMT_DRIVE);
  if (res < 0) {
    cerr << "Error formatting test drive\n";
    goto exit_vg;
  }

  // Load the class being tested.
  cout << "Loading test class\n";
  handle = dlopen(argv[1], RTLD_LAZY);
  if (handle == NULL) {
    cerr << "Error loading test from class " << argv[1] << "\n" << dlerror()
      << "\n";
    goto exit_vg;
  }
  // Get needed methods from loaded class.
  cout << "Loading factory method\n";
  factory = dlsym(handle, "get_instance");
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig factory method " << dl_error << "\n";
    dlclose(handle);
    goto exit_vg;
  }
  cout << "Loading destory method\n";
  killer = dlsym(handle, "delete_instance");
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig deleter method " << dl_error << "\n";
    dlclose(handle);
    goto exit_vg;
  }

  // Mount test file system for pre-test setup.
  cout << "Mounting test file system for pre-test setup\n";
  // TODO(ashmrtn): Fix to work with all file system types.
  res = mount(TEST_PATH, MNT_POINT, "ext4", 0, NULL);
  if (res < 0) {
    cerr < "Error mounting test file system for pre-test setup\n";
    dlclose(handle);
    goto exit_vg;
  }

  // TODO(ashmrtn): Spin off as a new thread so that the test file system can
  // still be unmounted even if the test case doesn't properly close all file
  // handles.
  // Run pre-test setup stuff.
  cout << "Running test setup\n";
  test = ((create_t*)(factory))();
  if (test->setup() != 0) {
    cerr << "Error in test setup\n";
    umount(MNT_POINT);
    goto exit_test_case;
  }

  // Unmount the test file system after pre-test setup.
  cout << "Unmounting test file system after pre-test setup\n";
  res = umount(MNT_POINT);
  if (res < 0 && errno != EBUSY) {
    cerr << "Error unmounting test file system after pre-test setup " << err
      << "\n";
    goto exit_test_case;
  }

  // Create snapshot of disk for testing.
  cout << "Making new snapshot\n";
  res = system(INIT_SN);
  if (res != 0) {
    cerr << "Error creating snapshot\n";
    goto exit_test_case;
  }

  // Insert the disk block wrapper into the kernel.
  cout << "Inserting wrapper module into kernel\n";
  res = system(INSMOD);
  if (res < 0) {
    cerr << "Error inserting kernel wrapper module\n";
    goto exit_test_case;
  }

  // Mount the file system under the wrapper module for profiling.
  // TODO(ashmrtn): Fix to work with all file system types.
  cout << "Mounting wrapper file system\n";
  res = mount(DEVICE_PATH, MNT_POINT, "ext4", 0, NULL);
  if (res < 0) {
    cerr << "Error mounting wrapper file system\n";
    goto exit_module;
  }

  // Get access to wrapper module ioctl functions via FD.
  cout << "Getting wrapper device ioctl fd\n";
  device_fd = open(DEVICE_PATH, O_RDONLY | O_CLOEXEC);
  if (device_fd == -1) {
    cerr << "Error opening device file\n";
    goto exit_module;
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
  if (child == -1) {
    cerr << "Error spinning off test process\n";
    goto exit_device_fd;
  } else if (child != 0) {
    wait(&status);
    sleep(WRITE_DELAY);
    if (status != 0) {
      cerr << "Error in test process\n";
      goto exit_device_fd;
    }
  } else {
    // Forked process' stuff.
    int res = test->run();
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    // Exit forked process after test.
    return res;
  }

  cout << "Unmounting wrapper file system after test profiling\n";
  res = umount(MNT_POINT);
  if (res < 0) {
    cerr << "Error cleaning up: cannot unmount wrapper file system\n";
    goto exit_module;
  }

  ioctl(device_fd, HWM_LOG_OFF);
  cout << "Getting profile data\n";
  data = get_log(device_fd);
  clear_data(&data);

  // Normal termination.
  cout << "Close wrapper ioctl fd\n";
  close(device_fd);
  cout << "Removing wrapper module from kernel\n";
  res = system(RMMOD);
  if (res < 0) {
    cerr << "Error cleaning up: remove wrapper module\n";
    goto exit_test_case;
  }
  cout << "Destorying test case\n";
  ((destroy_t*)(killer))(test);
  dlclose(handle);
  // Kill snapshot.
  cout << "Destroying profiling snapshot\n";
  res = system(DESTROY_SN);
  if (res != 0) {
    cerr << "Error cleaning up: destroy profiling snapshot\n";
    goto exit_vg;
  }
  // Create new snapshot.
  cout << "Making new snapshot\n";
  res = system(INIT_SN);
  if (res != 0) {
    cerr << "Error creating snapshot\n";
    goto exit_vg;
  }
  // Write recorded data out to block device to make sure this is the data we're
  // looking for.
  cout << "Opening snapshot block device\n";
  sn_fd = open("/dev/" VG_DISK "/" SN_DISK, O_WRONLY);
  if (sn_fd < 0) {
    cerr << "Error opening snapshot as block device\n";
    goto exit_vg;
  }
  cout << "Writing profiled data to block device\n";
  for (const auto write_op : data) {
    // Operation is not a write so skip it.
    if (!(write_op.metadata.bi_rw & 1ULL)) {
      continue;
    }
    unsigned long int byte_addr = write_op.metadata.write_sector * SECTOR_SIZE;
    //cout << "Moving to offset " << byte_addr << "\n";
    /*
    printf("Moving to offset 0x%lx to write %ld bytes\n", byte_addr,
        write_op.metadata.size);
    */
    res = lseek(sn_fd, byte_addr, SEEK_SET);
    if (res < 0) {
      cerr << "Error seeking in block device\n";
      break;
    }
    int bytes_written = 0;
    do {
      res = write(sn_fd,
          (void*) ((unsigned long) write_op.data + bytes_written),
          write_op.metadata.size - bytes_written);
      if (res < 0) {
        cerr << "Error writing profiled data to block device\n";
        break;
      }
      bytes_written += res;
    } while (bytes_written < write_op.metadata.size);
  }


  // Finish cleaning everything up.
  cout << "Closing snapshot block device\n";
  close(sn_fd);
  cout << "Removing volume group\n";
  res = system(DESTROY_VG);
  if (res != 0) {
    cerr << "Error cleaning up: remove volume group(s)\n";
    res2 = -2;
    goto exit_pv;
  }
  cout << "Removing physical volume\n";
  res = system(DESTROY_PV);
  if (res != 0) {
    cerr << "Error cleaning up: remove physical volume(s)\n";
    res2 = -2;
    goto exit_expire_time;
  }
  cout << "Restoring dirty_expire_time\n";
  res = put_expire_time(old_expire_time);
  if (res < 0) {
    cerr << "Error cleaning up: fix dirty_expire_centisecs\n";
    return -1;
  }
  return 0;

 exit_device_fd:
  close(device_fd);
 exit_unmount:
  res = umount(MNT_POINT);
  if (res < 0) {
    cerr << "Error cleaning up: unmount wrapper file system\n";
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    res2 = -2;
    goto exit_expire_time;
  }
 exit_module:
  res = system(RMMOD);
  if (res < 0) {
    cerr << "Error cleaning up: remove wrapper module\n";
  }
 exit_test_case:
  ((destroy_t*)(killer))(test);
  dlclose(handle);
 exit_vg:
  // Remove volume groups that were created.
  res = system(DESTROY_VG);
  if (res != 0) {
    cerr << "Error cleaning up: remove volume group(s)\n";
    res2 = -2;
  }
 exit_pv:
  // Remove physical volumes that were created.
  res = system(DESTROY_PV);
  if (res != 0) {
    cerr << "Error cleaning up: remove physical volume(s)\n";
    res2 = -2;
  }
 exit_expire_time:
  // Restore old dirty_expire_centisecs.
  res = put_expire_time(old_expire_time);
  if (res < 0) {
    cerr << "Error cleaning up: fix dirty_expire_centisecs\n";
    res2 = -2;
  }
  return res2;
}
