#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "disk_wrapper_ioctl.h"

int main(int argc, char** argv) {
  int rd_fd = open("/dev/cow_ram_snapshot1_0", O_RDONLY);
  if (rd_fd < 0) {
    printf("error opening device\n");
    return -1;
  }

  int err = ioctl(rd_fd, COW_BRD_RESTORE_SNAPSHOT);
  if (err < 0) {
    printf("error with ioctl\n");
    return -2;
  }

  close(rd_fd);
  return err;
}
