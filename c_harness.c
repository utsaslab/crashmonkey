#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include "hellow_ioctl.h"

#define DEVICE_PATH "/dev/hwm"
#define TEST_FILE "tests/test_get_log_ent_size"
#define WRITE_DELAY 30

void print_log(int device_fd) {
  struct disk_write_op_meta* metadata;
  void* data = NULL;
  unsigned int entry_size;
  while (1) {
    int result = ioctl(device_fd, HWM_GET_LOG_ENT_SIZE, &entry_size);
    if (result == -1) {
      if (errno == ENODATA) {
        //printf("No log data reported by block device\n");
        break;
      } else if (errno == EFAULT) {
        printf("efault occurred\n");
        return;
      }
    }
    data = calloc(entry_size, sizeof(char));
    result = ioctl(device_fd, HWM_GET_LOG_ENT, data);
    if (result == -1) {
      if (errno == ENODATA) {
        //printf("end of log data\n");
        free(data);
        break;
      } else if (errno == EFAULT) {
        printf("efault occurred\n");
        free(data);
        return;
      }
    }
    metadata = (struct disk_write_op_meta*) data;
    char* data_string = data + sizeof(struct disk_write_op_meta);
    printf("operation with flags: %lx\n~~~\n%s\n~~~\n", metadata->bi_rw,
        data_string);
    free(data);
  }
}

int run_test() {
  execl(TEST_FILE, TEST_FILE, NULL);
}

int main(int argc, char** argv) {
  int device_fd = open(DEVICE_PATH, O_RDONLY | O_CLOEXEC);
  if (device_fd == -1) {
    printf("Error opening device file\n");
    return -1;
  }

  ioctl(device_fd, HWM_CLR_LOG);
  ioctl(device_fd, HWM_LOG_ON);

  pid_t child = fork();
  if (child == -1) {
    printf("Error spinning of test process\n");
    close(device_fd);
    return -1;
  } else if (child != 0) {
    pid_t status;
    wait(&status);
    sleep(WRITE_DELAY);
    if (status != 0) {
      printf("Error in test process\n");
      close(device_fd);
      return -1;
    }
  } else {
    run_test();
  }

  ioctl(device_fd, HWM_LOG_OFF);
  print_log(device_fd);
  close(device_fd);
  return 0;
}
