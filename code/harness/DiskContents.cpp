#include "DiskContents.h"

namespace fs_testing {
namespace diskcontents {

DiskContents::DiskContents(char* path, const char* type) {
  disk_path = (char *) malloc(sizeof(char)*30);
  fs_type = (char *) malloc(sizeof(char)*10);
  strcpy(disk_path, path);
  strcpy(fs_type, type);
}

DiskContents::~DiskContents() {}

int DiskContents::mount_disk() {
  // Construct and set mount_point
  mount_point = (char *) malloc(sizeof(char)*30);
  std::string mount_dir = "/mnt/";
  strcpy(mount_point, "/mnt/");
  strcat(mount_point, (disk_path + 5));
  // Create the mount directory with read/write/search permissions for owner and group, 
  // and with read/search permissions for others.
  if (mkdir(mount_point, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    return -1;
  }
  // Mount the disk
  if (mount(disk_path, mount_point, fs_type, 0, NULL) < 0) {
    return -1;
  }
  return 0;
}

int DiskContents::unmount_and_delete_mount_point() {
  if (umount(mount_point) < 0) {
    return -1;
  }
  // Delete the mount directory
  if (unlink(mount_point) != 0) {
    return -1;
  }
  return 0;
}

void DiskContents::get_contents(const char* path, std::vector<fileAttributes> &contents) {
  DIR *dir;
  struct dirent *dir_entry;
  struct stat statbuf;
  if (!(dir = opendir(path))) {
    return;
  }
  if (!(dir_entry = readdir(dir))) {
    return;
  }
  do {
    std::string parent(path);
    std::string file(dir_entry->d_name);
    std::string final = parent + "/" + file;
    if (stat(final.c_str(), &statbuf) == -1) {
      return;
    }
    if (dir_entry->d_type == DT_DIR) {
      if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
        continue;
      }
      struct fileAttributes entry;
      // entry.name = final;
      // entry.type = "DIR";
      contents.push_back(entry);
      get_contents(final.c_str(), contents);
    } else {
      struct fileAttributes entry;
      // entry.name = final;
      // entry.type = "FILE";
      contents.push_back(entry);
    }
  } while (dir_entry = readdir(dir));
  closedir(dir);
}

void DiskContents::populate_contents() {
  get_contents(mount_point, contents);
}

void DiskContents::compare_disk_contents(DiskContents &compare_disk, std::ofstream &diff_file) {
}

} //namespace diskcontents
} // namespace fs_testing