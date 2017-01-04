#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include <cerrno>
#include <cstdio>
#include <iostream>

#include "hellow_ioctl.h"
#include "test_case.h"

#define SO_PATH "tests/"
#define DEVICE_PATH "/dev/hwm"
#define WRITE_DELAY 30

using std::cerr;
using fs_testing::create_t;
using fs_testing::destroy_t;
using fs_testing::test_case;

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
        cerr << "efault occurred\n";
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
        cerr << "efault occurred\n";
        free(data);
        return;
      }
    }
    metadata = (struct disk_write_op_meta*) data;
    char* data_string =
      (char*) ((unsigned long) data + sizeof(struct disk_write_op_meta));
    printf("operation with flags: %lx\n~~~\n%s\n~~~\n", metadata->bi_rw,
        data_string);
    free(data);
  }
}

int main(int argc, char** argv) {
  int device_fd = open(DEVICE_PATH, O_RDONLY | O_CLOEXEC);
  if (device_fd == -1) {
    cerr << "Error opening device file\n";
    return -1;
  }

  // Load the class being tested.
  void* handle = dlopen(argv[1], RTLD_LAZY);
  if (handle == NULL) {
    cerr << "Error loading test from class " << argv[1] << "\n" << dlerror()
      << "\n";
    return -1;
  }
  // Get needed methods from loaded class.
  void* factory = dlsym(handle, "get_instance");
  const char* dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig factory method " << dl_error << "\n";
    dlclose(handle);
    return -1;
  }
  void* killer = dlsym(handle, "delete_instance");
  dl_error = dlerror();
  if (dl_error) {
    cerr << "Error gettig deleter method " << dl_error << "\n";
    dlclose(handle);
    return -1;
  }

  ioctl(device_fd, HWM_LOG_OFF);
  test_case* test = ((create_t*)(factory))();
  if (test->setup() != 0) {
    cerr << "Error in test setup\n";
    dlclose(handle);
    return -1;
  }

  ioctl(device_fd, HWM_CLR_LOG);
  ioctl(device_fd, HWM_LOG_ON);

  pid_t child = fork();
  if (child == -1) {
    cerr << "Error spinning off test process\n";
    close(device_fd);
    return -1;
  } else if (child != 0) {
    pid_t status;
    wait(&status);
    sleep(WRITE_DELAY);
    if (status != 0) {
      cerr << "Error in test process\n";
      close(device_fd);
      return -1;
    }
  } else {
    // Forked process' stuff.
    int res = test->run();
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    return res;
  }

  ioctl(device_fd, HWM_LOG_OFF);
  print_log(device_fd);
  close(device_fd);
  dlclose(handle);
  return 0;
}
