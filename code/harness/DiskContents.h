#ifndef DISK_CONTENTS_H
#define DISK_CONTENTS_H

#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs_testing {
namespace diskcontents {

struct linux_dirent {
  long d_ino;
  off_t d_off;
  unsigned short d_reclen;
  char d_name[];
  char d_type;
};

struct fileAttributes {
  struct linux_dirent dir_attr;
  struct stat stat_attr;
};

class DiskContents {
public:
  // Constructor and Destructor
  DiskContents(char* path, const char* type);
  ~DiskContents();
  
  int mount_disk();
  int unmount_and_delete_mount_point();
  void populate_contents();
  void compare_disk_contents(DiskContents &compare_disk, std::ofstream &diff_file);

private:
  char* disk_path;
  char* mount_point;
  char* fs_type;
  std::vector<fileAttributes> contents;
  void get_contents(const char* path, std::vector<fileAttributes> &contents);
};

} //namespace diskcontents
} // namespace fs_testing

#endif // DISK_CONTENTS_H