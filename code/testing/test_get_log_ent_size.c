#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../hellow_ioctl.h"

#define TEXT "hello great big world out there"

// Assumes that the hwm module is already inserted into the kernel and is
// properly running.
int main(int argc, char** argv) {
  int device_fd = open("/dev/hwm1", O_RDONLY);
  if (device_fd == -1) {
    printf("Error opening device file\n");
    return -1;
  }
  int fd = open("/mnt/snapshot/testing/test_file2", O_RDWR | O_CREAT);
  if (fd == -1) {
    printf("Error opening test file\n");
    return -1;
  }
  ioctl(device_fd, HWM_CLR_LOG);
  ioctl(device_fd, HWM_LOG_ON);

  unsigned int str_len = strlen(TEXT) * sizeof(char);
  unsigned int written = 0;
  while (written != str_len) {
    written = write(fd, TEXT + written, str_len - written);
    if (written == -1) {
      printf("Error while writing to test file\n");
      goto out_1;
    }
  }
  fsync(fd);
  close(fd);
  unsigned int delay = 30;
  while (delay != 0) {
    delay = sleep(delay);
  }

  ioctl(device_fd, HWM_LOG_OFF);
  struct disk_write_op_meta* metadata;
  void* data = NULL;
  unsigned int entry_size;
  while (1) {
    int result = ioctl(device_fd, HWM_GET_LOG_ENT_SIZE, &entry_size);
    if (result == -1) {
      if (errno == ENODATA) {
        printf("No log data reported by block device\n");
      } else if (errno == EFAULT) {
        printf("efault occurred\n");
        goto out;
      }
    }
    data = calloc(entry_size, sizeof(char));
    result = ioctl(device_fd, HWM_GET_LOG_ENT, data);
    if (result == -1) {
      if (errno == ENODATA) {
        printf("end of log data\n");
        free(data);
        break;
      } else if (errno == EFAULT) {
        printf("efault occurred\n");
        free(data);
        goto out;
      }
    }
    metadata = (struct disk_write_op_meta*) data;
    char* data_string = data + sizeof(struct disk_write_op_meta);
    printf("operation with flags: %lx\n~~~\n%s\n~~~\n", metadata->bi_rw,
        data_string);
    free(data);
  }

  close(device_fd);
  return 0;

 out_1:
  close(fd);
 out:
  ioctl(device_fd, HWM_LOG_OFF);
  close(device_fd);
  return -1;
}
