#include "DiskContents.h"

namespace fs_testing {

fileAttributes::fileAttributes() {
  dir_attr = NULL;
  stat_attr = NULL;
}

fileAttributes::~fileAttributes() {
}

void fileAttributes::set_dir_attr(struct dirent* a) {
  dir_attr = (struct dirent *) malloc(sizeof(struct dirent));
  dir_attr->d_ino = a->d_ino;
  dir_attr->d_off = a->d_off;
  dir_attr->d_reclen = a->d_reclen;
  dir_attr->d_type = a->d_type;
  strncpy(dir_attr->d_name, a->d_name, sizeof(a->d_name));
  dir_attr->d_name[sizeof(a->d_name) - 1] = '\0';
}

void fileAttributes::set_stat_attr(struct stat* a) {
  stat_attr = (struct stat *) malloc(sizeof(struct stat));

  stat_attr->st_dev = a->st_dev;
  stat_attr->st_ino = a->st_ino;
  stat_attr->st_mode = a->st_mode;
  stat_attr->st_nlink = a->st_nlink;
  stat_attr->st_uid = a->st_uid;
  stat_attr->st_gid = a->st_gid;
  stat_attr->st_rdev = a->st_rdev;
  stat_attr->st_size = a->st_size;
  stat_attr->st_atime = a->st_atime;
  stat_attr->st_mtime = a->st_mtime;
  stat_attr->st_ctime = a->st_ctime;
  stat_attr->st_blksize = a->st_blksize;
  stat_attr->st_blocks = a->st_blocks;
}

bool fileAttributes::compare_dir_attr(struct dirent* a) {
  if (a == NULL) {
    return false;
  }
  return ((dir_attr->d_ino == a->d_ino) &&
    (dir_attr->d_off == a->d_off) &&
    (dir_attr->d_reclen == a->d_reclen) &&
    (dir_attr->d_type == a->d_type) &&
    (strcmp(dir_attr->d_name, a->d_name) == 0));
}

bool fileAttributes::compare_stat_attr(struct stat *a) {
  if (a == NULL) {
    return false;
  }
  return ((stat_attr->st_dev == a->st_dev) &&
    (stat_attr->st_ino == a->st_ino) &&
    (stat_attr->st_mode == a->st_mode) &&
    (stat_attr->st_nlink == a->st_nlink) &&
    (stat_attr->st_uid == a->st_uid) &&
    (stat_attr->st_gid == a->st_gid) &&
    (stat_attr->st_rdev == a->st_rdev) &&
    (stat_attr->st_size == a->st_size) &&
    (stat_attr->st_blksize == a->st_blksize) &&
    (stat_attr->st_blocks == a->st_blocks));
}

DiskContents::DiskContents(char* path, const char* type) {
  disk_path = (char *) malloc(sizeof(char)*30);
  mount_point = (char *) malloc(sizeof(char)*40);
  fs_type = (char *) malloc(sizeof(char)*10);
  strcpy(disk_path, path);
  strcpy(fs_type, type);
  device_mounted = false;
}

DiskContents::~DiskContents() {
  free(disk_path);
  free(mount_point);
  free(fs_type);
}

int DiskContents::mount_disk() {
  // Construct and set mount_point
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
  device_mounted = true;
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
  device_mounted = false;
  return 0;
}

void DiskContents::get_contents(const char* path) {
  DIR *directory;
  struct dirent *dir_entry;
  // open both the directories
  if (!(directory = opendir(path))) {
    return;
  }
  // get the contents in both the directories
  if (!(dir_entry = readdir(directory))) {
    return;
  }
  do {
    std::string parent_path(path);
    std::string filename(dir_entry->d_name);
    std::string current_path = parent_path + "/" + filename;
    std::cout << filename << std::endl;
    struct stat statbuf;
    fileAttributes fa;
    if (stat(current_path.c_str(), &statbuf) == -1) {
      return;
    }
    if (dir_entry->d_type == DT_DIR) {
      if ((strcmp(dir_entry->d_name, ".") == 0) || (strcmp(dir_entry->d_name, "..") == 0)) {
        return;
      }
      fa.set_dir_attr(dir_entry);
      fa.set_stat_attr(&statbuf);
      contents[filename] = fa;
      // If the entry is a directory and not . or .. make a recursive call
      get_contents(current_path.c_str());
    } else if (dir_entry->d_type == DT_LNK) {
      // compare lstat outputs
      struct stat lstat_buf;
      if (stat(parent_path.c_str(), &statbuf) == -1) {
        return;
      }
      fa.set_stat_attr(&lstat_buf);
      contents[filename] = fa;
    } else {
      fa.set_stat_attr(&statbuf);
      contents[filename] = fa;
    }
  } while (dir_entry = readdir(directory));
}

const char* DiskContents::get_mount_point() {
  return mount_point;
}

void DiskContents::compare_disk_contents(DiskContents &compare_disk, std::ofstream &diff_file) {
  if (!device_mounted) {
    if (mount_disk() != 0) {
      std::cout << "Mounting " << disk_path << " failed" << std::endl;
    }
  }
  if (compare_disk.mount_disk() != 0) {
    std::cout << "Mounting " << compare_disk.disk_path << " failed" << std::endl;
  }
  get_contents(mount_point);
  compare_disk.get_contents(compare_disk.get_mount_point());

  // TODO(P.S.): Write to the diff_file afer properly iterating through the contents
  // Clean this up.
  std::cout << "ContentSizes:" << contents.size() << compare_disk.contents.size() << std::endl;
  for (auto i : contents) {
    for (auto j : compare_disk.contents) {
      if ( i.first == j.first) {
        std::cout << i.first << std::endl;
        if (i.second.dir_attr != NULL && j.second.dir_attr != NULL)
          std::cout << i.second.compare_dir_attr(j.second.dir_attr);
        if (i.second.stat_attr != NULL && j.second.stat_attr != NULL)
          std::cout << i.second.compare_stat_attr(j.second.stat_attr);
      }
    }
  }
  return;
}

} // namespace fs_testing