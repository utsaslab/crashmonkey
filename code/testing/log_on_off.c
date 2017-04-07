#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../disk_wrapper_ioctl.h"

// Assumes that the hwm module is already inserted into the kernel and is
// properly running.
int main(int argc, char** argv) {
  int fd = open("/dev/hwm1", O_RDONLY);
  if (fd == -1) {
    printf("Error opening device file\n");
    return -1;
  }
  ioctl(fd, HWM_LOG_ON);
  ioctl(fd, HWM_LOG_OFF);
  close(fd);
  return 0;
}
