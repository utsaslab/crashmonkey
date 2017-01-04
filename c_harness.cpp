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
#include <vector>

#include "hellow_ioctl.h"
#include "test_case.h"

#define SO_PATH "tests/"
#define DEVICE_PATH "/dev/hwm"
#define WRITE_DELAY 30

using std::cerr;
using std::vector;
using fs_testing::create_t;
using fs_testing::destroy_t;
using fs_testing::test_case;

struct disk_write {
  disk_write_op_meta metadata;
  void* data;
};

void clear_data(vector<struct disk_write>* data) {
  for (auto it = data->begin(); it != data->end(); ++it) {
    free(it->data);
  }
}

vector<struct disk_write> get_log(int device_fd) {
  vector<struct disk_write> res_data;
  while (1) {
    struct disk_write write;

    int result = ioctl(device_fd, HWM_GET_LOG_META, &write.metadata);
    if (result == -1) {
      if (errno == ENODATA) {
        break;
      } else if (errno == EFAULT) {
        cerr << "efault occurred\n";
        clear_data(&res_data);
        return vector<struct disk_write>();
      }
    }

    write.data = calloc(write.metadata.size, sizeof(char));
    if (write.data == NULL) {
      cerr << "Error getting temporary memory for log data\n";
      clear_data(&res_data);
      return vector<struct disk_write>();
    }
    result = ioctl(device_fd, HWM_GET_LOG_DATA, write.data);
    if (result == -1) {
      if (errno == ENODATA) {
        // Should never reach here as loop will break when getting the size
        // above.
        free(write.data);
        break;
      } else if (errno == EFAULT) {
        cerr << "efault occurred\n";
        free(write.data);
        clear_data(&res_data);
        return vector<struct disk_write>();
      }
    }

    res_data.push_back(write);
    printf("operation with flags: %lx\n~~~\n%s\n~~~\n",
        res_data.back().metadata.bi_rw, (char*) res_data.back().data);
    result = ioctl(device_fd, HWM_NEXT_ENT);
    if (result == -1) {
      if (errno == ENODATA) {
        // Should never reach here as loop will break when getting the size
        // above.
        free(write.data);
        break;
      } else {
        cerr << "Error getting next log entry\n";
        break;
      }
    }
  }
  return res_data;
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
    ioctl(device_fd, HWM_LOG_OFF);
  } else {
    // Forked process' stuff.
    int res = test->run();
    ((destroy_t*)(killer))(test);
    dlclose(handle);
    // Exit forked process after test.
    return res;
  }

  vector<struct disk_write> data = get_log(device_fd);
  close(device_fd);
  dlclose(handle);
  clear_data(&data);
  return 0;
}
