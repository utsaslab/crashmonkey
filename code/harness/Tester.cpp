#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>

#include "../hellow_ioctl.h"
#include "Tester.h"

#define TEST_CLASS_FACTORY        "test_case_get_instance"
#define TEST_CLASS_DEFACTORY      "test_case_delete_instance"
#define PERMUTER_CLASS_FACTORY    "permuter_get_instance"
#define PERMUTER_CLASS_DEFACTORY  "permuter_delete_instance"

#define DIRTY_EXPIRE_TIME_PATH "/proc/sys/vm/dirty_expire_centisecs"
#define DROP_CACHES_PATH       "/proc/sys/vm/drop_caches"

#define FULL_WRAPPER_PATH "/dev/hwm"

// TODO(ashmrtn): Make a quiet and regular version of commands.
// TODO(ashmrtn): Make so that commands work with user given device path.
#define SILENT              " > /dev/null 2>&1"

#define MNT_WRAPPER_DEV_PATH FULL_WRAPPER_PATH
#define MNT_MNT_POINT        "/mnt/snapshot"

#define PART_PART_DRIVE   "fdisk "
#define PART_PART_DRIVE_2 " << EOF\no\nn\np\n1\n\n\nw\nEOF\n"
#define PART_DEL_PART_DRIVE   "fdisk "
#define PART_DEL_PART_DRIVE_2 " << EOF\no\nw\nEOF\n"
#define FMT_FMT_DRIVE   "mkfs -t "

#define INSMOD_MODULE_NAME "hellow.ko"
#define WRAPPER_INSMOD      "insmod " INSMOD_MODULE_NAME " target_device_path="
#define WRAPPER_RMMOD       "rmmod " INSMOD_MODULE_NAME

#define DEV_SECTORS_PATH    "/sys/block/"
#define DEV_SECTORS_PATH_2  "/size"

#define SECTOR_SIZE 512

// TODO(ashmrtn): Expand to work with other file system types.
#define TEST_CASE_FSCK "fsck -T -t "

namespace fs_testing {

using std::calloc;
using std::cerr;
using std::cout;
using std::endl;
using std::free;
using std::ifstream;
using std::ofstream;
using std::shared_ptr;
using std::string;
using std::vector;

using fs_testing::tests::test_create_t;
using fs_testing::tests::test_destroy_t;
using fs_testing::permuter::Permuter;
using fs_testing::permuter::permuter_create_t;
using fs_testing::permuter::permuter_destroy_t;
using fs_testing::utils::disk_write;

Tester::Tester(const string f_type, const string target_test_device, bool v) :
  fs_type(f_type), device_raw(target_test_device), verbose(v) {
    // Start off by assuming we will make a partition for ourselves and use that
    // parition.
    device_mount = device_raw + "1";

    // /sys/block/<dev> has the number of 512 byte sectors on the disk.
    string dev = device_raw.substr(device_raw.find_last_of("/") + 1);
    string path(DEV_SECTORS_PATH + dev + DEV_SECTORS_PATH_2);
    int size_fd = open(path.c_str(), O_RDONLY);
    if (size_fd < 0) {
      int err = errno;
      cerr << "Unable to device size file " << errno << endl;
      device_size = 0;
      return;
    }
    unsigned int buf_size = 50;
    char size_buf[buf_size];
    unsigned int bytes_read = 0;
    int res = 0;
    do {
      res = read(size_fd, size_buf + bytes_read, buf_size - 1 - bytes_read);
      if (res < 0) {
        int err = errno;
        device_size = 0;
        cerr << "Unable to get device size " << err << endl;
        close(size_fd);
        return;
      }

      bytes_read += res;
    } while (res != 0 && bytes_read < buf_size - 1);
    close(size_fd);

    // Null terminate string.
    size_buf[bytes_read] = '\0';
    device_size = strtol(size_buf, NULL, 10) * SECTOR_SIZE;
}

int Tester::clone_device() {
  std::cout << "cloning device " << device_raw << std::endl;
  if (device_clone != NULL) {
    cerr << "Device clone already exists" << endl;
    return DRIVE_CLONE_EXISTS_ERR;
  }
  int raw_dev_fd = open(device_raw.c_str(), O_RDONLY);
  if (raw_dev_fd < 0) {
    cerr << "Error opening raw device" << endl;
    return DRIVE_CLONE_ERR;
  }

  if (device_size == 0) {
    cerr << "Device size is 0" << endl;
    return DRIVE_CLONE_ERR;
  }
  device_clone = new char[device_size];
  unsigned int bytes_read = 0;
  do {
    int res = read(raw_dev_fd, device_clone + bytes_read,
        device_size - bytes_read);
    if (res < 0) {
      cerr << "Error reading from raw device" << endl;
      close(raw_dev_fd);
      return DRIVE_CLONE_ERR;
    }
    bytes_read += res;
    if (res == 0 && bytes_read < device_size) {
      cerr << "Reached end of device before device size" << endl;
      close(raw_dev_fd);
      return DRIVE_CLONE_ERR;
    }
  } while (bytes_read < device_size);
  close(raw_dev_fd);

  return SUCCESS;
}

int Tester::clone_device_restore(bool reread) {
  if (device_clone == NULL) {
    cerr << "No device clone to restore" << endl;
    return DRIVE_CLONE_ERR;
  }

  int raw_dev_fd = open(device_raw.c_str(), O_WRONLY);
  if (raw_dev_fd < 0) {
    cerr << "Error opening raw device" << endl;
    return DRIVE_CLONE_ERR;
  }

  unsigned int bytes_written = 0;
  do {
    int res = write(raw_dev_fd, device_clone + bytes_written,
        device_size - bytes_written);
    if (res < 0) {
      cerr << "Error writing to raw device" << endl;
      close(raw_dev_fd);
      return DRIVE_CLONE_RESTORE_ERR;
    }
    bytes_written += res;
  } while (bytes_written < device_size);
  int res = fsync(raw_dev_fd);
  if (res < 0) {
    cerr << "Error flushing restored image" << endl;
  }
  if (reread) {
    // TODO(ashmrtn): Fixme by moving me to a better place.
    do {
      res = ioctl(raw_dev_fd, BLKRRPART, NULL);
    } while (errno == EBUSY);
    if (res < 0) {
      int errnum = errno;
      cerr << "Error re-reading partition table " << errnum << endl;
    }
  }
  close(raw_dev_fd);

  return SUCCESS;
}

int Tester::mount_device_raw(const char* opts) {
  return mount_device(device_mount.c_str(), opts);
}

int Tester::mount_wrapper_device(const char* opts) {
  // TODO(ashmrtn): Make some sort of boolean that tracks if we should use the
  // first parition or not?
  string dev(MNT_WRAPPER_DEV_PATH);
  dev += "1";
  return mount_device(dev.c_str(), opts);
}

int Tester::mount_device(const char* dev, const char* opts) {
  if (mount(dev, MNT_MNT_POINT, fs_type.c_str(), 0, (void*) opts) < 0) {
    disk_mounted = false;
    return MNT_MNT_ERR;
  }
  disk_mounted = true;
  return SUCCESS;
}

int Tester::umount_device() {
  if (disk_mounted) {
    if (umount(MNT_MNT_POINT) < 0) {
      disk_mounted = true;
      return MNT_UMNT_ERR;
    }
  }
  disk_mounted = false;
  return SUCCESS;
}

int Tester::insert_wrapper() {
  if (!wrapper_inserted) {
    string command(WRAPPER_INSMOD);
    command += device_mount;
    if (!verbose) {
      command += SILENT;
    }
    if (system(command.c_str()) != 0) {
      wrapper_inserted = false;
      return WRAPPER_INSERT_ERR;
    }
  }
  wrapper_inserted = true;
  return SUCCESS;
}

int Tester::remove_wrapper() {
  if (wrapper_inserted) {
    if (system(WRAPPER_RMMOD) != 0) {
      wrapper_inserted = true;
      return WRAPPER_REMOVE_ERR;
    }
  }
  wrapper_inserted = false;
  return SUCCESS;
}

int Tester::get_wrapper_ioctl() {
  ioctl_fd = open(FULL_WRAPPER_PATH, O_RDONLY | O_CLOEXEC);
  if (ioctl_fd == -1) {
    return WRAPPER_OPEN_DEV_ERR;
  }
  return SUCCESS;
}

void Tester::put_wrapper_ioctl() {
  if (ioctl_fd != -1) {
    close(ioctl_fd);
    ioctl_fd = -1;
  }
}

void Tester::begin_wrapper_logging() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_LOG_ON);
  }
}

void Tester::end_wrapper_logging() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_LOG_OFF);
  }
}

int Tester::get_wrapper_log() {
  if (ioctl_fd != -1) {
    while (1) {
      disk_write_op_meta meta;

      int result = ioctl(ioctl_fd, HWM_GET_LOG_META, &meta);
      if (result == -1) {
        if (errno == ENODATA) {
          break;
        } else if (errno == EFAULT) {
          cerr << "efault occurred\n";
          log_data.clear();
          return WRAPPER_DATA_ERR;
        }
      }

      char* data = new char[meta.size];
      result = ioctl(ioctl_fd, HWM_GET_LOG_DATA, data);
      if (result == -1) {
        if (errno == ENODATA) {
          // Should never reach here as loop will break when getting the size
          // above.
          break;
        } else if (errno == EFAULT) {
          cerr << "efault occurred\n";
          log_data.clear();
          return WRAPPER_MEM_ERR;
        }
      }
      log_data.emplace_back(meta, data);
      delete[] data;

      result = ioctl(ioctl_fd, HWM_NEXT_ENT);
      if (result == -1) {
        if (errno == ENODATA) {
          // Should never reach here as loop will break when getting the size
          // above.
          break;
        } else {
          cerr << "Error getting next log entry\n";
          log_data.clear();
          break;
        }
      }
    }
  }
  std::cout << "fetched " << log_data.size() << " log data entries"
      << std::endl;
  return SUCCESS;
}

void Tester::clear_wrapper_log() {
  if (ioctl_fd != -1) {
    ioctl(ioctl_fd, HWM_CLR_LOG);
  }
}

int Tester::test_load_class(const char* path) {
  return test_loader.load_class<test_create_t *>(path, TEST_CLASS_FACTORY,
      TEST_CLASS_DEFACTORY);
}

void Tester::test_unload_class() {
    test_loader.unload_class<test_destroy_t *>();
}

int Tester::permuter_load_class(const char* path) {
  return permuter_loader.load_class<permuter_create_t *>(path,
      PERMUTER_CLASS_FACTORY, PERMUTER_CLASS_DEFACTORY);
}

void Tester::permuter_unload_class() {
    permuter_loader.unload_class<permuter_destroy_t *>();
}

const char* Tester::update_dirty_expire_time(const char* time) {
  const int expire_fd = open(DIRTY_EXPIRE_TIME_PATH, O_RDWR | O_CLOEXEC);
  if (!read_dirty_expire_time(expire_fd)) {
    close(expire_fd);
    return NULL;
  }
  if (!write_dirty_expire_time(expire_fd, time)) {
    // Attempt to restore old dirty expire time.
    if (!write_dirty_expire_time(expire_fd, dirty_expire_time)) {
    }
    close(expire_fd);
    return NULL;
  }
  close(expire_fd);
  return dirty_expire_time;
}

bool Tester::read_dirty_expire_time(const int fd) {
  int bytes_read = 0;
  do {
    const int res = read(fd, (void*) ((long) dirty_expire_time + bytes_read),
        DIRTY_EXPIRE_TIME_SIZE - 1 - bytes_read);
    if (res == 0) {
      break;
    } else if (res < 0) {
      return false;
    }
    bytes_read += res;
  } while (bytes_read < DIRTY_EXPIRE_TIME_SIZE - 1);
  // Null terminate character array.
  dirty_expire_time[bytes_read] = '\0';
  return true;
}

bool Tester::write_dirty_expire_time(const int fd, const char* time) {
  const int size = strnlen(time, DIRTY_EXPIRE_TIME_SIZE);
  int bytes_written = 0;
  do {
    const int res = write(fd, time + bytes_written, size - bytes_written);
    if (res < 0) {
      return false;
    }
    bytes_written += res;
  } while (bytes_written < size);
  return true;
}

int Tester::partition_drive() {
  string command(PART_PART_DRIVE + device_raw);
  if (!verbose) {
    command += SILENT;
  }
  command += PART_PART_DRIVE_2;
  if (system(command.c_str()) != 0) {
    return PART_PART_ERR;
  }
  return SUCCESS;
}

int Tester::wipe_partitions() {
  string command(PART_DEL_PART_DRIVE + device_raw);
  if (!verbose) {
    command += SILENT;
  }
  command += PART_DEL_PART_DRIVE_2;
  if (system(command.c_str()) != 0) {
    return PART_PART_ERR;
  }
  return SUCCESS;
}

int Tester::format_drive() {
  string command(FMT_FMT_DRIVE + fs_type + " " +  device_mount);
  if (!verbose) {
    command += SILENT;
  }
  if (system(command.c_str()) != 0) {
    return FMT_FMT_ERR;
  }
  return SUCCESS;
}

int Tester::test_setup() {
  return test_loader.get_instance()->setup();
}

int Tester::test_run() {
  return test_loader.get_instance()->run();
}

int Tester::test_check_random_permutations(const int num_rounds) {
  std::fill(test_test_stats, test_test_stats + 6, 0);
  test_test_stats[TESTS_TESTS_RUN] = 0;
  Permuter *p = permuter_loader.get_instance();
  p->set_data(&log_data);
  vector<disk_write> permutes = log_data;
  for (int rounds = 0; rounds < num_rounds; ++rounds) {
    p->permute(&permutes);
    const auto start_itr = permutes.begin();

    for (auto end_itr = start_itr; end_itr != permutes.end(); ++end_itr) {
      ++test_test_stats[TESTS_TESTS_RUN];
      cout << '.' << std::flush;

      // Restore disk clone.
      if (clone_device_restore(false) != SUCCESS) {
        cout << endl;
        return DRIVE_CLONE_RESTORE_ERR;
      }

      // Write recorded data out to block device in different orders so that we
      // can if they are all valid or not.
      const int sn_fd = open(device_raw.c_str(), O_WRONLY);
      if (sn_fd < 0) {
        cout << endl;
        return TEST_CASE_FILE_ERR;
      }
      if (!test_write_data(sn_fd, start_itr, end_itr)) {
        ++test_test_stats[TESTS_TEST_ERR];
        cout << "test errored in writing data" << endl;
        close(sn_fd);
        continue;
      }
      close(sn_fd);

      string command(TEST_CASE_FSCK + fs_type + " " + device_mount
          + " -- -y");
      if (!verbose) {
        command += SILENT;
      }
      const int fsck_res = system(command.c_str());
      if (!(fsck_res == 0 || WEXITSTATUS(fsck_res) == 1)) {
        cerr << "Error running fsck on snapshot file system: " <<
          WEXITSTATUS(fsck_res) << "\n";
        ++test_test_stats[TESTS_TEST_FSCK_FAIL];
      } else {
        // TODO(ashmrtn): Consider mounting with options specified for test
        // profile?
        if (mount_device_raw(NULL) != SUCCESS) {
          ++test_test_stats[TESTS_TEST_FSCK_FAIL];
          continue;
        }
        const int test_check_res = test_loader.get_instance()->check_test();
        if (test_check_res < 0) {
          ++test_test_stats[TESTS_TEST_BAD_DATA];
        } else if (test_check_res == 0 && fsck_res != 0) {
          ++test_test_stats[TESTS_TEST_FSCK_FIX];
        } else if (test_check_res == 0 && fsck_res == 0) {
          ++test_test_stats[TESTS_TEST_PASS];
        } else {
          ++test_test_stats[TESTS_TEST_ERR];
          cerr << "test errored for other reason" << endl;
        }
        umount_device();
      }
    }
  }
  cout << endl;
  return SUCCESS;
}

int Tester::test_check_current() {
  string command(TEST_CASE_FSCK + fs_type + " " + device_mount
      + " -- -y");
  if (!verbose) {
    command += SILENT;
  }
  const int fsck_res = system(command.c_str());
  if (!(fsck_res == 0 || WEXITSTATUS(fsck_res) == 1)) {
    cerr << "Error running fsck on snapshot file system: " <<
      WEXITSTATUS(fsck_res) << "\n";
    return TEST_TEST_ERR;
  } else {
    // TODO(ashmrtn): Consider mounting with options specified for test
    // profile?
    if (mount_device_raw(NULL) != SUCCESS) {
      cerr << "Error mounting file system" << endl;
      return TEST_TEST_ERR;
    }
    const int test_check_res = test_loader.get_instance()->check_test();
    if (test_check_res < 0) {
      cerr << "Bad data" << endl;
      return TEST_TEST_ERR;
    } else if (test_check_res == 0 && fsck_res != 0) {
      cerr << "fsck fix" << endl;
    } else if (test_check_res == 0 && fsck_res == 0) {
      // Does nothing, but success case
    } else {
      cerr << "Other reasons" << endl;
      return TEST_TEST_ERR;
    }
    umount_device();
  }

  return SUCCESS;
}

int Tester::test_restore_log() {
  // We need to mount the original device because we intercept bios after they
  // have been traslated to the current disk and lose information about sector
  // offset from the partition. If we were to mount and write into the partition
  // we would clobber the disk state in unknown ways.
  const int sn_fd = open(device_raw.c_str(), O_WRONLY);
  if (sn_fd < 0) {
    cout << endl;
    return TEST_CASE_FILE_ERR;
  }
  if (!test_write_data(sn_fd, log_data.begin(), log_data.end())) {
    cout << "test errored in writing data" << endl;
    close(sn_fd);
    return TEST_TEST_ERR;
  }
  close(sn_fd);
  return SUCCESS;
}

bool Tester::test_write_data(const int disk_fd,
    const vector<disk_write>::iterator& start,
    const vector<disk_write>::iterator& end) {
  for (auto current = start; current != end; ++current) {
    // Operation is not a write so skip it.
    if (!(current->has_write_flag())) {
      continue;
    }

    const unsigned long int byte_addr =
      current->metadata.write_sector * SECTOR_SIZE;
    if (lseek(disk_fd, byte_addr, SEEK_SET) < 0) {
      return false;
    }
    int bytes_written = 0;
    shared_ptr<void> data = current->get_data();
    void* data_base_addr = data.get();
    do {
      int res = write(disk_fd,
          (void*) ((unsigned long) data_base_addr + bytes_written),
          current->metadata.size - bytes_written);
      if (res < 0) {
        return false;
      }
      bytes_written += res;
    } while (bytes_written < current->metadata.size);
  }
  return true;
}

void Tester::cleanup_harness() {
  delete[] device_clone;
  device_clone = NULL;
  device_size = 0;

  if (umount_device() != SUCCESS) {
    cerr << "Unable to unmount device" << endl;
    test_unload_class();
    return;
  }

  if (remove_wrapper() != SUCCESS) {
    cerr << "Unable to remove wrapper device" << endl;
    test_unload_class();
    return;
  }

  permuter_unload_class();
  test_unload_class();
}

int Tester::clear_caches() {
  sync();
  const int cache_fd = open(DROP_CACHES_PATH, O_WRONLY);
  if (cache_fd < 0) {
    return CLEAR_CACHE_ERR;
  }

  int res;
  do {
    res = write(cache_fd, "3", 1);
    if (res < 0) {
      close(cache_fd);
      return CLEAR_CACHE_ERR;
    }
  } while (res < 1);
  close(cache_fd);
  return SUCCESS;
}

int Tester::log_profile_save(string log_file) {
  // TODO(ashmrtn): What happens if this fails?
  // Open with append flags. We should remove the log file argument and use a
  // class specific one that is set at class creation time. That way people
  // don't break our logging system.
  std::cout << "saving " << log_data.size() << " disk operations" << endl;
  ofstream log(log_file, std::ofstream::trunc);
  for (const disk_write& dw : log_data) {
    disk_write::serialize(log, dw);
  }
  log.close();
  return SUCCESS;
}

int Tester::log_profile_load(string log_file) {
  ifstream log(log_file);
  while (log.peek() != EOF) {
    log_data.push_back(disk_write::deserialize(log));
  }
  bool err = log.fail();
  int errnum = errno;
  log.close();
  if (err) {
    std::cout << "error " << strerror(errnum) << std::endl;
    return LOG_CLONE_ERR;
  }
  std::cout << "loaded " << log_data.size() << " disk operations" << endl;
  return SUCCESS;
}

int Tester::log_snapshot_save(string log_file) {
  if (device_clone == NULL) {
    return LOG_CLONE_ERR;
  }
  // TODO(ashmrtn): What happens if this fails?
  ofstream log(log_file, std::ofstream::trunc);
  log.write(device_clone, device_size);
  bool err = log.fail();
  log.flush();
  log.close();
  if (err) {
    return LOG_CLONE_ERR;
  }
  return SUCCESS;
}

int Tester::log_snapshot_load(string log_file) {
  // TODO(ashmrtn): What happens if this fails?
  if (device_clone != NULL) {
    delete[] device_clone;
  }
  ifstream log(log_file);
  device_clone = new char[device_size];
  log.read(device_clone, device_size);
  bool err = log.fail();
  int errnum = errno;
  assert(log.peek() == EOF);
  log.close();
  if (err) {
    std::cout << "error " << strerror(errnum) << std::endl;
    return LOG_CLONE_ERR;
  }
  return SUCCESS;
}

}  // namespace fs_testing
