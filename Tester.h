#ifndef TESTER_H
#define TESTER_H

#include <vector>

#include "test_case.h"
#include "utils.h"

#define SUCCESS                0
#define LVM_PV_INIT_ERR        -1
#define LVM_PV_REMOVE_ERR      -2
#define LVM_VG_INIT_ERR        -3
#define LVM_VG_REMOVE_ERR      -4
#define LVM_LV_INIT_ERR        -5
#define LVM_SN_INIT_ERR        -6
#define LVM_SN_REMOVE_ERR      -7
#define TEST_CASE_HANDLE_ERR   -8
#define TEST_CASE_INIT_ERR     -9
#define TEST_CASE_DEST_ERR     -10
#define TEST_CASE_FILE_ERR     -11
#define MNT_BAD_DEV_ERR        -12
#define MNT_MNT_ERR            -13
#define MNT_UMNT_ERR           -14
#define FMT_FMT_ERR            -15
#define WRAPPER_INSERT_ERR     -16
#define WRAPPER_REMOVE_ERR     -17
#define WRAPPER_OPEN_DEV_ERR   -18
#define WRAPPER_DATA_ERR       -19
#define WRAPPER_MEM_ERR        -20

#define MNT_LVM_LV_DEV         0
#define MNT_LVM_SN_DEV         1
#define MNT_WRAPPER_DEV        2

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
  int test_test_stats[5];

  const char* update_dirty_expire_time(const char* time);

  int lvm_init();
  int lvm_destroy();
  int init_snapshot();
  int destroy_snapshot();

  int format_lvm_drive(const int type);

  int test_load_class(const char* path);
  void test_unload_class();
  int test_setup();
  int test_run();
  int test_check_random_permutations(const int num_rounds);

  int mount_device(const int dev, const char* opts);
  int umount_device();

  int insert_wrapper();
  int remove_wrapper();
  int get_wrapper_ioctl();
  void put_wrapper_ioctl();
  void begin_wrapper_logging();
  void end_wrapper_logging();
  int get_wrapper_log();
  void clear_wrapper_log();

  void cleanup_harness();

 private:
  void* loader_handle = NULL;
  void* test_factory = NULL;
  void* test_killer = NULL;
  test_case* test = NULL;

  char dirty_expire_time[DIRTY_EXPIRE_TIME_SIZE];

  bool lvm_pv_active = false;
  bool lvm_vg_active = false;
  bool lvm_sn_active = false;

  bool wrapper_inserted = false;

  bool disk_mounted = false;
  char* disk_mount_path = NULL;

  int ioctl_fd = -1;
  std::vector<disk_write> log_data;

  bool read_dirty_expire_time(int fd);
  bool write_dirty_expire_time(int fd, const char* time);

  bool test_write_data(const int disk_fd,
      const std::vector<disk_write>::iterator& start,
      const std::vector<disk_write>::iterator& end);
};

}  // namespace fs_testing

#endif
