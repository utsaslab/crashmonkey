#ifndef TESTER_H
#define TESTER_H

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../utils/ClassLoader.h"
#include "../permuter/Permuter.h"
#include "../tests/BaseTestCase.h"
#include "../utils/utils.h"

#define SUCCESS                  0
#define DRIVE_CLONE_ERR          -1
#define DRIVE_CLONE_RESTORE_ERR  -2
#define DRIVE_CLONE_EXISTS_ERR   -3
#define TEST_TEST_ERR            -4
#define LOG_CLONE_ERR            -5
#define TEST_CASE_FILE_ERR       -11
#define MNT_BAD_DEV_ERR          -12
#define MNT_MNT_ERR              -13
#define MNT_UMNT_ERR             -14
#define FMT_FMT_ERR              -15
#define WRAPPER_INSERT_ERR       -16
#define WRAPPER_REMOVE_ERR       -17
#define WRAPPER_OPEN_DEV_ERR     -18
#define WRAPPER_DATA_ERR         -19
#define WRAPPER_MEM_ERR          -20
#define CLEAR_CACHE_ERR          -21
#define PART_PART_ERR            -22

#define FMT_EXT4               0

#define DIRTY_EXPIRE_TIME_SIZE 10

namespace fs_testing {
#ifdef TEST_CASE
namespace test {
  class TestTester;
}  // namespace test
#endif

class Tester {
  #ifdef TEST_CASE
  friend class fs_testing::test::TestTester;
  #endif

 public:
  enum test_stats {
    TESTS_RUN,
    TEST_FSCK_FAIL,
    TEST_BAD_DATA,
    TEST_FSCK_FIX,
    TEST_PASS,
    TEST_ERR,
    TEST_NUM,
  };

  enum time_stats {
    PERMUTE_TIME,
    SNAPSHOT_TIME,
    BIO_WRITE_TIME,
    FSCK_TIME,
    TEST_CASE_TIME,
    TOTAL_TIME,
    NUM_TIME,
  };

  Tester(const bool verbosity);
  const bool verbose = false;
  void set_fs_type(const std::string type);
  void set_device(const std::string device_path);
  void set_flag_device(const std::string device_path);

  int test_test_stats[TEST_NUM] = {0};

  const char* update_dirty_expire_time(const char* time);

  int partition_drive();
  int wipe_partitions();
  int format_drive();
  int clone_device();
  int clone_device_restore(bool reread);

  int permuter_load_class(const char* path);
  void permuter_unload_class();

  int test_load_class(const char* path);
  void test_unload_class();
  int test_setup();
  int test_run();
  int test_check_permutations(const int num_rounds);
  int test_check_random_permutations(const int num_rounds);
  int test_restore_log();
  int test_check_current();

  int mount_device_raw(const char* opts);
  int mount_wrapper_device(const char* opts);
  int umount_device();

  int insert_wrapper();
  int remove_wrapper();
  int get_wrapper_ioctl();
  void put_wrapper_ioctl();
  void begin_wrapper_logging();
  void end_wrapper_logging();
  int get_wrapper_log();
  void clear_wrapper_log();

  int clear_caches();
  void cleanup_harness();
  // TODO(ashmrtn): Save the fstype in the log file so that we don't
  // accidentally mix logs of one fs type with mount options for another?
  int log_profile_save(std::string log_file);
  int log_profile_load(std::string log_file);
  int log_snapshot_save(std::string log_file);
  int log_snapshot_load(std::string log_file);

  std::chrono::milliseconds get_timing_stat(time_stats timing_stat);

  // TODO(ashmrtn): Figure out why making these private slows things down a lot.
 private:
  unsigned long int device_size;
  char* device_clone = NULL;
  fs_testing::utils::ClassLoader<fs_testing::tests::BaseTestCase> test_loader;
  fs_testing::utils::ClassLoader<fs_testing::permuter::Permuter>
    permuter_loader;

  char dirty_expire_time[DIRTY_EXPIRE_TIME_SIZE];
  std::string fs_type;
  std::string device_raw;
  std::string device_mount;
  std::string flags_device;


  bool wrapper_inserted = false;

  bool disk_mounted = false;

  int ioctl_fd = -1;
  std::vector<fs_testing::utils::disk_write> log_data;

  int mount_device(const char* dev, const char* opts);

  bool read_dirty_expire_time(int fd);
  bool write_dirty_expire_time(int fd, const char* time);

  bool test_write_data(const int disk_fd,
      const std::vector<fs_testing::utils::disk_write>::iterator& start,
      const std::vector<fs_testing::utils::disk_write>::iterator& end);

  std::chrono::milliseconds timing_stats[NUM_TIME] =
      {std::chrono::milliseconds(0)};

};

}  // namespace fs_testing

std::ostream& operator<<(std::ostream& os, fs_testing::Tester::test_stats test);
std::ostream& operator<<(std::ostream& os, fs_testing::Tester::time_stats time);

#endif
