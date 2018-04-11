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
#include <map>

namespace fs_testing {

class fileAttributes {
public:
  struct dirent* dir_attr;
  struct stat* stat_attr;
  std::string md5sum;

  fileAttributes();
  ~fileAttributes();

  void set_dir_attr(struct dirent* a);
  void set_stat_attr(struct stat* a);
  void set_md5sum(std::string filepath);
  bool compare_dir_attr(struct dirent* a);
  bool compare_stat_attr(struct stat *a);
  bool compare_md5sum(std::string a);
  bool is_regular_file();
};

class DiskContents {
public:
  // Constructor and Destructor
  DiskContents(char* path, const char* type);
  ~DiskContents();
  
  int mount_disk();
  const char* get_mount_point();
  int unmount_and_delete_mount_point();
  bool compare_disk_contents(DiskContents &compare_disk, std::ofstream &diff_file);
  bool compare_entries_at_path(DiskContents &compare_disk, std::string path,
    std::ofstream &diff_file);

private:
  bool device_mounted;
  char* disk_path;
  char* mount_point;
  char* fs_type;
  std::map<std::string, fileAttributes> contents;
  void compare_contents(DiskContents &compare_disk, std::ofstream &diff_file);
  void get_contents(const char* path);
};

} // namespace fs_testing

#endif // DISK_CONTENTS_H