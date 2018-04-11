#include "DiskContents.h"
using std::endl;

namespace fs_testing {

fileAttributes::fileAttributes() {
  dir_attr = NULL;
  stat_attr = NULL;
  md5sum = "";
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

void fileAttributes::set_md5sum(std::string file_path) {
  FILE *fp;
  std::string command = "md5sum " + file_path;
  char md5[100];
  fp = popen(command.c_str(), "r");
  fscanf(fp, "%s", md5);
  fclose(fp);
  std::string md5_str(md5);
  md5sum = md5_str;
}

bool fileAttributes::compare_dir_attr(struct dirent* a) {
  if (a == NULL && dir_attr == NULL) {
    return true;
  } else if (a == NULL || dir_attr == NULL) {
    return false;
  }
  return ((dir_attr->d_ino == a->d_ino) &&
    (dir_attr->d_off == a->d_off) &&
    (dir_attr->d_reclen == a->d_reclen) &&
    (dir_attr->d_type == a->d_type) &&
    (strcmp(dir_attr->d_name, a->d_name) == 0));
}

bool fileAttributes::compare_stat_attr(struct stat *a) {
  if (a == NULL && stat_attr == NULL) {
    return true;
  } else if (a == NULL || stat_attr == NULL) {
    return false;
  }

  return ((stat_attr->st_ino == a->st_ino) &&
    (stat_attr->st_mode == a->st_mode) &&
    (stat_attr->st_nlink == a->st_nlink) &&
    (stat_attr->st_uid == a->st_uid) &&
    (stat_attr->st_gid == a->st_gid) &&
    // (stat_attr->st_rdev == a->st_rdev) &&
    // (stat_attr->st_dev == a->st_dev) &&
    (stat_attr->st_size == a->st_size) &&
    (stat_attr->st_blksize == a->st_blksize) &&
    (stat_attr->st_blocks == a->st_blocks));
}

bool fileAttributes::compare_md5sum(std::string a) {
  return md5sum.compare(a);
}

bool fileAttributes::is_regular_file() {
  return S_ISREG(stat_attr->st_mode);
}

std::ofstream& operator<< (std::ofstream& os, fileAttributes& a) {
  // print dir_attr
  if (a.dir_attr != NULL) {
    os << "---Directory Atrributes---" << std::endl;
    os << "Name   : " << (a.dir_attr)->d_name << std::endl;
    os << "Inode  : " << (a.dir_attr)->d_ino << std::endl;
    os << "Offset : " << (a.dir_attr)->d_off << std::endl;
    os << "Length : " << (a.dir_attr)->d_reclen << std::endl;
    os << "Type   : " << (a.dir_attr)->d_type << std::endl;
  }
  // print stat_attr
  if (a.stat_attr != NULL) {
    os << "---File Stat Atrributes---" << std::endl;
    os << "Inode     : " << (a.stat_attr)->st_ino << std::endl;
    os << "TotalSize : " << (a.stat_attr)->st_size << std::endl;
    os << "BlockSize : " << (a.stat_attr)->st_blksize << std::endl;
    os << "#Blocks   : " << (a.stat_attr)->st_blocks << std::endl;
    os << "#HardLinks: " << (a.stat_attr)->st_nlink << std::endl;
    os << "Mode      : " << (a.stat_attr)->st_mode << std::endl;
    os << "User ID   : " << (a.stat_attr)->st_uid << std::endl;
    os << "Group ID  : " << (a.stat_attr)->st_gid << std::endl;
    os << "Device ID : " << (a.stat_attr)->st_rdev << std::endl;
    os << "RootDev ID: " << (a.stat_attr)->st_dev << std::endl;
  }
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
  // free(disk_path);
  // free(mount_point);
  // free(fs_type);
}

int DiskContents::mount_disk() {
  // Construct and set mount_point
  strcpy(mount_point, "/mnt/");
  strcat(mount_point, (disk_path + 5));
  // Create the mount directory with read/write/search permissions for owner and group, 
  // and with read/search permissions for others.
  int ret = mkdir(mount_point, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (ret == -1 && errno != EEXIST) {
    std::cout << "creating mountpoint failed" << std::endl;
    return -1;
  }
  // Mount the disk
  if (mount(disk_path, mount_point, fs_type, MS_RDONLY, NULL) < 0) {
    return -1;
  }
  // sleep after mount
  unsigned int to_sleep = 1;
  do {
    to_sleep = sleep(to_sleep);
  } while (to_sleep > 0);

  device_mounted = true;
  return 0;
}

int DiskContents::unmount_and_delete_mount_point() {
  // umount till successful
  int umount_res;
  int err;
  std::string command = "umount ";
  command += mount_point;
  system(command.c_str());
  do {
    umount_res = umount(mount_point);
    if (umount_res < 0) {
      err = errno;
      usleep(500);
    }
  } while (umount_res < 0 && err == EBUSY);

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
    closedir(directory);
    return;
  }
  do {
    std::string parent_path(path);
    std::string filename(dir_entry->d_name);
    std::string current_path = parent_path + "/" + filename;
    struct stat statbuf;
    fileAttributes fa;
    if (stat(current_path.c_str(), &statbuf) == -1) {
      continue;
    }
    if (dir_entry->d_type == DT_DIR) {
      if ((strcmp(dir_entry->d_name, ".") == 0) || (strcmp(dir_entry->d_name, "..") == 0)) {
        continue;
      }
      fa.set_dir_attr(dir_entry);
      fa.set_stat_attr(&statbuf);
      contents[filename] = fa;
      // If the entry is a directory and not . or .. make a recursive call
      get_contents(current_path.c_str());
    } else if (dir_entry->d_type == DT_LNK) {
      // compare lstat outputs
      struct stat lstatbuf;
      if (lstat(current_path.c_str(), &lstatbuf) == -1) {
        continue;
      }
      fa.set_stat_attr(&lstatbuf);
      contents[filename] = fa;
    } else if (dir_entry->d_type == DT_REG) {
      fa.set_md5sum(current_path);
      fa.set_stat_attr(&statbuf);
      contents[filename] = fa;
    } else {
      fa.set_stat_attr(&statbuf);
      contents[filename] = fa;
    }
  } while (dir_entry = readdir(directory));
  closedir(directory);
}

const char* DiskContents::get_mount_point() {
  return mount_point;
}

bool DiskContents::compare_disk_contents(DiskContents &compare_disk, std::ofstream &diff_file) {
  std::cout << __func__ << std::endl;
  bool retValue = true;

  if (strcmp(disk_path, compare_disk.disk_path) == 0) {
    return retValue;
  }

  std::string base_path = "/mnt/snapshot";
  get_contents(base_path.c_str());

  if (compare_disk.mount_disk() != 0) {
    std::cout << "Mounting " << compare_disk.disk_path << " failed" << std::endl;
  }

  compare_disk.get_contents(compare_disk.get_mount_point());

  // Compare the size of contents
  if (contents.size() != compare_disk.contents.size()) {
    diff_file << "DIFF: Mismatch" << std::endl;
    diff_file << "Unequal #entries in " << disk_path << ", " << compare_disk.disk_path;
    diff_file << std::endl << std::endl;
    diff_file << disk_path << " contains:" << std::endl;
    for (auto i : contents) {
      diff_file << i.first << std::endl;
    }
    diff_file << std::endl;

    diff_file << compare_disk.disk_path << " contains:" << std::endl;
    for (auto i : compare_disk.contents) {
      diff_file << i.first << std::endl;
    }
    diff_file << std::endl;
    retValue = false;
  }

  // entry-wise comparision
  for (auto i : contents) {
    fileAttributes i_fa = i.second;
    if (compare_disk.contents.find((i.first)) == compare_disk.contents.end()) {
      diff_file << "DIFF: Missing " << i.first << std::endl;
      diff_file << "Found in " << disk_path << " only" << std::endl;
      diff_file << i_fa << endl << endl;
      retValue = false;
      continue;
    }
    fileAttributes j_fa = compare_disk.contents[(i.first)];
    if (!(i_fa.compare_dir_attr(j_fa.dir_attr)) ||
          !(i_fa.compare_stat_attr(j_fa.stat_attr))) {
        diff_file << "DIFF: Content Mismatch " << i.first << std::endl << std::endl;
        diff_file << disk_path << ":" << std::endl;
        diff_file << i_fa << endl << endl;
        diff_file << compare_disk.disk_path << ":" << std::endl;
        diff_file << j_fa << endl << endl;
        retValue = false;
        continue;
    }
    // compare user data if the entry corresponds to a regular files
    if (i_fa.is_regular_file()) {
      // check md5sum of the file contents
      if (i_fa.compare_md5sum(j_fa.md5sum) != 0) {
        diff_file << "DIFF : Data Mismatch of " << (i.first) << std::endl;
        diff_file << disk_path << " has md5sum " << i_fa.md5sum << std::endl;
        diff_file << compare_disk.disk_path << " has md5sum " << j_fa.md5sum;
        diff_file << std::endl << std::endl;
        retValue = false;
      }
    }
  }
  // TODO(P.S.) Fix the unmount issue and uncomment the function below.
  compare_disk.unmount_and_delete_mount_point();
  return retValue;
}

// TODO(P.S.) Cleanup the code and pull out redundant code into separate functions
bool DiskContents::compare_entries_at_path(DiskContents &compare_disk,
  std::string path, std::ofstream &diff_file) {
  std::cout << __func__ << std::endl;
  bool retValue = true;

  if (strcmp(disk_path, compare_disk.disk_path) == 0) {
    return retValue;
  }

  std::string base_path = "/mnt/snapshot" + path;
  get_contents(base_path.c_str());

  if (compare_disk.mount_disk() != 0) {
    std::cout << "Mounting " << compare_disk.disk_path << " failed" << std::endl;
  }

  std::string compare_disk_mount_point(compare_disk.get_mount_point());
  std::string compare_path = compare_disk_mount_point + path;
  compare_disk.get_contents(compare_path.c_str());

  // Compare the size of contents
  if (contents.size() != compare_disk.contents.size()) {
    diff_file << "DIFF: Mismatch" << std::endl;
    diff_file << "Unequal #entries in " << disk_path << ", " << compare_disk.disk_path;
    diff_file << std::endl << std::endl;
    diff_file << disk_path << " contains:" << std::endl;
    for (auto i : contents) {
      diff_file << i.first << std::endl;
    }
    diff_file << std::endl;

    diff_file << compare_disk.disk_path << " contains:" << std::endl;
    for (auto i : compare_disk.contents) {
      diff_file << i.first << std::endl;
    }
    diff_file << std::endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  fileAttributes base_fa, compare_fa;
  struct stat base_statbuf, compare_statbuf;
  if (stat(base_path.c_str(), &base_statbuf) == -1) {
    std::cout << "Failed stating the file " << base_path << std::endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }
  if (stat(compare_path.c_str(), &compare_statbuf) == -1) {
    std::cout << "Failed stating the file " << compare_path << std::endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }
  base_fa.set_stat_attr(&base_statbuf);
  compare_fa.set_stat_attr(&compare_statbuf);
  if (base_fa.compare_stat_attr(compare_fa.stat_attr)) {
    diff_file << "DIFF: Content Mismatch " << path << std::endl << std::endl;
    diff_file << base_path << ":" << std::endl;
    diff_file << base_fa << endl << endl;
    diff_file << compare_path << ":" << std::endl;
    diff_file << compare_fa << endl << endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  if (base_fa.is_regular_file()) {
    base_fa.set_md5sum(base_path);
    compare_fa.set_md5sum(compare_path);
    if (base_fa.compare_md5sum(compare_fa.md5sum) != 0) {
      diff_file << "DIFF : Data Mismatch of " << path << std::endl;
      diff_file << base_path << " has md5sum " << base_fa.md5sum << std::endl;
      diff_file << compare_path << " has md5sum " << compare_fa.md5sum;
      diff_file << std::endl << std::endl;
      compare_disk.unmount_and_delete_mount_point();
      return false;
    }
  }

  // TODO(P.S.) Fix the unmount issue and uncomment the function below.
  compare_disk.unmount_and_delete_mount_point();
  return retValue;
}

} // namespace fs_testing