#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEXT "hello great big world out there"

// Assumes that the hwm module is already inserted into the kernel and is
// properly running.
int main(int argc, char** argv) {
  int fd = open("/mnt/snapshot/testing/test_file2", O_RDWR | O_CREAT);
  if (fd == -1) {
    printf("Error opening test file\n");
    return -1;
  }

  unsigned int str_len = strlen(TEXT) * sizeof(char);
  unsigned int written = 0;
  while (written != str_len) {
    written = write(fd, TEXT + written, str_len - written);
    if (written == -1) {
      printf("Error while writing to test file\n");
      goto out;
    }
  }
  //fsync(fd);
  close(fd);
  return 0;

 out:
  close(fd);
  return -1;
}
