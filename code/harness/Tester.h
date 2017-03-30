#ifndef TESTER_H
#define TESTER_H

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

#define TESTS_TESTS_RUN        0
#define TESTS_TEST_FSCK_FAIL   1
#define TESTS_TEST_BAD_DATA    2
#define TESTS_TEST_FSCK_FIX    3
#define TESTS_TEST_PASS        4
#define TESTS_TEST_ERR         5

namespace fs_testing {
class Tester {
 public:
  Tester(const std::string f_type, const std::string target_test_device,
      bool v);
  const bool verbose = false;

  int test_test_stats[5];
  unsigned long int device_size;
  char* device_clone = NULL;

  const char* update_dirty_expire_time(const char* time);

  int partition_drive();
  int wipe_paritions();
  int format_drive();
  int clone_device();
  int clone_device_restore();

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
  void log_profile(std::string log_file);

 private:
  fs_testing::utils::ClassLoader<fs_testing::tests::BaseTestCase> test_loader;
  fs_testing::utils::ClassLoader<fs_testing::permuter::Permuter>
    permuter_loader;

  char dirty_expire_time[DIRTY_EXPIRE_TIME_SIZE];
  const std::string fs_type;
  const std::string device_raw;
  std::string device_mount;

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
};

}  // namespace fs_testing

#endif
