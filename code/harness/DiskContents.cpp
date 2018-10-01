#include "DiskContents.h"

using std::endl;
using std::cout;
using std::string;
using std::ofstream;

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

void fileAttributes::set_md5sum(string file_path) {
  FILE *fp;
  string command = "md5sum " + file_path;
  char md5[100];
  fp = popen(command.c_str(), "r");
  fscanf(fp, "%s", md5);
  fclose(fp);
  string md5_str(md5);
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

bool fileAttributes::compare_md5sum(string a) {
  return md5sum.compare(a);
}

bool fileAttributes::is_regular_file() {
  return S_ISREG(stat_attr->st_mode);
}

ofstream& operator<< (ofstream& os, fileAttributes& a) {
  // print dir_attr
  if (a.dir_attr != NULL) {
    os << "---Directory Atrributes---" << endl;
    os << "Name   : " << (a.dir_attr)->d_name << endl;
    os << "Inode  : " << (a.dir_attr)->d_ino << endl;
    os << "Offset : " << (a.dir_attr)->d_off << endl;
    os << "Length : " << (a.dir_attr)->d_reclen << endl;
    os << "Type   : " << (a.dir_attr)->d_type << endl;
  }
  // print stat_attr
  if (a.stat_attr != NULL) {
    os << "---File Stat Atrributes---" << endl;
    os << "Inode     : " << (a.stat_attr)->st_ino << endl;
    os << "TotalSize : " << (a.stat_attr)->st_size << endl;
    os << "BlockSize : " << (a.stat_attr)->st_blksize << endl;
    os << "#Blocks   : " << (a.stat_attr)->st_blocks << endl;
    os << "#HardLinks: " << (a.stat_attr)->st_nlink << endl;
    os << "Mode      : " << (a.stat_attr)->st_mode << endl;
    os << "User ID   : " << (a.stat_attr)->st_uid << endl;
    os << "Group ID  : " << (a.stat_attr)->st_gid << endl;
    os << "Device ID : " << (a.stat_attr)->st_rdev << endl;
    os << "RootDev ID: " << (a.stat_attr)->st_dev << endl;
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
    cout << "creating mountpoint failed" << endl;
    return -1;
  }
  // Mount the disk

  if (string(fs_type).compare("fscq") == 0) {
        string command = "sudo /home/jayashree/fscq/src/fscq " + string(disk_path) + " -o big_writes,atomic_o_trunc -f " + mount_point + " &";

        cout << "COmmand for mount : " << command << endl;
        system(command.c_str());
        cout << "Sleeping for 2 sec after mount.." << endl;
        sleep(2);
  } else {
        if (mount(disk_path, mount_point, fs_type, MS_RDONLY, NULL) < 0) {
                return -1;
        }
  }

  // sleep after mount
  unsigned int to_sleep = 1;
  do {
    to_sleep = sleep(to_sleep);
  } while (to_sleep > 0);

  device_mounted = true;
  string com = "tree " + string(mount_point);
  system(com.c_str());
  return 0;
}

int DiskContents::unmount_and_delete_mount_point() {
  // umount till successful
  int umount_res;
  int err;

        if (string(fs_type).compare("fscq") == 0) {
                string command = "sudo fusermount -u " + string(mount_point);
                cout << "Command for umount : " << command << endl;
                system(command.c_str());
                cout << "Sleeping for 2 sec after unmount" << endl;
                sleep(2);
        } else {
		 string command = "umount ";
  		command += mount_point;
 		 system(command.c_str());
  		do {
    			umount_res = umount(mount_point);
    			if (umount_res < 0) {
      				err = errno;
      				usleep(500);
    			}
  		} while (umount_res < 0 && err == EBUSY);
        }

  // Delete the mount directory
  if (unlink(mount_point) != 0) {
    return -1;
  }
  device_mounted = false;
  return 0;
}

void DiskContents::set_mount_point(string path) {
  strcpy(mount_point, path.c_str());
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
    string parent_path(path);
    string filename(dir_entry->d_name);
    string current_path = parent_path + "/" + filename;
    string relative_path = current_path;
    relative_path.erase(0, strlen(mount_point));
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
      contents[relative_path] = fa;
      // If the entry is a directory and not . or .. make a recursive call
      get_contents(current_path.c_str());
    } else if (dir_entry->d_type == DT_LNK) {
      // compare lstat outputs
      struct stat lstatbuf;
      if (lstat(current_path.c_str(), &lstatbuf) == -1) {
        continue;
      }
      fa.set_stat_attr(&lstatbuf);
      contents[relative_path] = fa;
    } else if (dir_entry->d_type == DT_REG) {
      fa.set_md5sum(current_path);
      fa.set_stat_attr(&statbuf);
      contents[relative_path] = fa;
    } else {
      fa.set_stat_attr(&statbuf);
      contents[relative_path] = fa;
    }
  } while (dir_entry = readdir(directory));
  closedir(directory);
}

const char* DiskContents::get_mount_point() {
  return mount_point;
}

bool DiskContents::compare_disk_contents(DiskContents &compare_disk, ofstream &diff_file) {
  bool retValue = true;

  if (strcmp(disk_path, compare_disk.disk_path) == 0) {
    return retValue;
  }

  string base_path = "/mnt/snapshot";
  get_contents(base_path.c_str());

  if (compare_disk.mount_disk() != 0) {
    cout << "Mounting " << compare_disk.disk_path << " failed" << endl;
  }

  compare_disk.get_contents(compare_disk.get_mount_point());

  // Compare the size of contents
  if (contents.size() != compare_disk.contents.size()) {
    diff_file << "DIFF: Mismatch" << endl;
    diff_file << "Unequal #entries in " << disk_path << ", " << compare_disk.disk_path;
    diff_file << endl << endl;
    diff_file << disk_path << " contains:" << endl;
    for (auto i : contents) {
      diff_file << i.first << endl;
    }
    diff_file << endl;

    diff_file << compare_disk.disk_path << " contains:" << endl;
    for (auto i : compare_disk.contents) {
      diff_file << i.first << endl;
    }
    diff_file << endl;
    retValue = false;
  }

  // entry-wise comparision
  for (auto i : contents) {
    fileAttributes i_fa = i.second;
    if (compare_disk.contents.find((i.first)) == compare_disk.contents.end()) {
      diff_file << "DIFF: Missing " << i.first << endl;
      diff_file << "Found in " << disk_path << " only" << endl;
      diff_file << i_fa << endl << endl;
      retValue = false;
      continue;
    }
    fileAttributes j_fa = compare_disk.contents[(i.first)];
    if (!(i_fa.compare_dir_attr(j_fa.dir_attr)) ||
          !(i_fa.compare_stat_attr(j_fa.stat_attr))) {
        diff_file << "DIFF: Content Mismatch " << i.first << endl << endl;
        diff_file << disk_path << ":" << endl;
        diff_file << i_fa << endl << endl;
        diff_file << compare_disk.disk_path << ":" << endl;
        diff_file << j_fa << endl << endl;
        retValue = false;
        continue;
    }
    // compare user data if the entry corresponds to a regular files
    if (i_fa.is_regular_file()) {
      // check md5sum of the file contents
      if (i_fa.compare_md5sum(j_fa.md5sum) != 0) {
        diff_file << "DIFF : Data Mismatch of " << (i.first) << endl;
        diff_file << disk_path << " has md5sum " << i_fa.md5sum << endl;
        diff_file << compare_disk.disk_path << " has md5sum " << j_fa.md5sum;
        diff_file << endl << endl;
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
  string path, ofstream &diff_file) {
  bool retValue = true;

  if (strcmp(disk_path, compare_disk.disk_path) == 0) {
    return retValue;
  }

  string base_path = "/mnt/snapshot" + path;

  if (compare_disk.mount_disk() != 0) {
    cout << "Mounting " << compare_disk.disk_path << " failed" << endl;
  }

  string compare_disk_mount_point(compare_disk.get_mount_point());
  string compare_path = compare_disk_mount_point + path;

  fileAttributes base_fa, compare_fa;
  bool failed_stat = false;
  struct stat base_statbuf, compare_statbuf;
  if (stat(base_path.c_str(), &base_statbuf) == -1) {
    diff_file << "Failed stating the file " << base_path << endl;
    failed_stat = true;
  }
  if (stat(compare_path.c_str(), &compare_statbuf) == -1) {
    diff_file << "Failed stating the file " << compare_path << endl;
    failed_stat = true;
  }

  if (failed_stat) {
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  base_fa.set_stat_attr(&base_statbuf);
  compare_fa.set_stat_attr(&compare_statbuf);
  if (!(base_fa.compare_stat_attr(compare_fa.stat_attr))) {
    diff_file << "DIFF: Content Mismatch " << path << endl << endl;
    diff_file << base_path << ":" << endl;
    diff_file << base_fa << endl << endl;
    diff_file << compare_path << ":" << endl;
    diff_file << compare_fa << endl << endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  if (base_fa.is_regular_file()) {
    base_fa.set_md5sum(base_path);
    compare_fa.set_md5sum(compare_path);
    if (base_fa.compare_md5sum(compare_fa.md5sum) != 0) {
      diff_file << "DIFF : Data Mismatch of " << path << endl;
      diff_file << base_path << " has md5sum " << base_fa.md5sum << endl;
      diff_file << compare_path << " has md5sum " << compare_fa.md5sum;
      diff_file << endl << endl;
      compare_disk.unmount_and_delete_mount_point();
      return false;
    }
  }

  // TODO(P.S.) Fix the unmount issue and uncomment the function below.
  compare_disk.unmount_and_delete_mount_point();
  return retValue;
}

bool DiskContents::compare_file_contents(DiskContents &compare_disk, string path,
    int offset, int length, ofstream &diff_file) {
  bool retValue = true;
  if (strcmp(disk_path, compare_disk.disk_path) == 0) {
    return retValue;
  }

  string base_path = "/mnt/snapshot" + path;
  if (compare_disk.mount_disk() != 0) {
    cout << "Mounting " << compare_disk.disk_path << " failed" << endl;
    return false;
  }
  string compare_disk_mount_point(compare_disk.get_mount_point());
  string compare_path = compare_disk_mount_point + path;

  fileAttributes base_fa, compare_fa;
  bool failed_stat = false;
  struct stat base_statbuf, compare_statbuf;
  if (stat(base_path.c_str(), &base_statbuf) == -1) {
    diff_file << "Failed stating the file " << base_path << endl;
    failed_stat = true;
  }
  if (stat(compare_path.c_str(), &compare_statbuf) == -1) {
    diff_file << "Failed stating the file " << compare_path << endl;
    failed_stat = true;
  }

  if (failed_stat) {
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  std::ifstream f1(base_path, std::ios::binary);
  std::ifstream f2(compare_path, std::ios::binary);

  if (!f1 || !f2) {
    cout << "Error opening input file streams " << base_path  << " and ";
    cout << compare_path << endl;
    compare_disk.unmount_and_delete_mount_point();
    return false;
  }

  f1.seekg(offset, std::ifstream::beg);
  f2.seekg(offset, std::ifstream::beg);

  char * buffer_f1 = new char[length + 1];
  char * buffer_f2 = new char[length + 1];

  f1.read(buffer_f1, length);
  f2.read(buffer_f2, length);
  f1.close();
  f2.close();

  buffer_f1[length] = '\0';
  buffer_f2[length] = '\0';

  if (strcmp(buffer_f1, buffer_f2) == 0) {
    compare_disk.unmount_and_delete_mount_point();
    return true;
  }

  diff_file << __func__ << " failed" << endl;
  diff_file << "Content Mismatch of file " << path << " from ";
  diff_file << offset << " of length " << length << endl;
  diff_file << base_path << " has " << buffer_f1 << endl;
  diff_file << compare_path << " has " << buffer_f2 << endl;
  compare_disk.unmount_and_delete_mount_point();
  return false;
}

bool isEmptyDirOrFile(string path) {
  DIR *directory = opendir(path.c_str());
  if (directory == NULL) {
    return true;
  }

  struct dirent *dir_entry;
  int num_dir_entries = 0;
  while (dir_entry = readdir(directory)) {
    if (++num_dir_entries > 2) {
      break;
    }
  }
  closedir(directory);
  if (num_dir_entries <= 2) {
    return true;
  }
  return false;
}

bool isFile(string path) {
  struct stat sb;
  if (stat(path.c_str(), &sb) < 0) {
    cout << __func__ << ": Failed stating " << path << endl;
    return false;
  }
  if (S_ISDIR(sb.st_mode)) {
    return false;
  }
  return true;
}

bool DiskContents::deleteFiles(string path, ofstream &diff_file) {
  if (path.empty()) {
    return true;
  }

  if (isEmptyDirOrFile(path) == true) {
    if (path.compare("/mnt/snapshot") == 0) {
      return true;
    }
    if (isFile(path) == true) {
      return (unlink(path.c_str()) == 0);
    } else {
      return (rmdir(path.c_str()) == 0);
    }
  }

  DIR *directory = opendir(path.c_str());
  if (directory == NULL) {
    cout << "Couldn't open the directory " << path << endl;
    diff_file << "Couldn't open the directory " << path << endl;
    return false;
  }

  struct dirent *dir_entry;
  while (dir_entry = readdir(directory)) {
    if ((strcmp(dir_entry->d_name, ".") == 0) ||
        (strcmp(dir_entry->d_name, "..") == 0)) {
      continue;
    }

    string subpath = path + "/" + string(dir_entry->d_name);
    bool subpathIsFile = isFile(subpath);
    bool res = deleteFiles(subpath, diff_file);
    if (!res) {
      closedir(directory);
      diff_file << "Couldn't remove directory " << subpath << " " << strerror(errno) << endl;
      cout << "Couldn't remove directory " << subpath << " " << strerror(errno) << endl;
      return res;
    }

    if (!subpathIsFile) {
      if (rmdir(subpath.c_str()) < 0) {
        diff_file << "Couldn't remove directory " << subpath << " "  << strerror(errno) << endl;
        cout << "Couldn't remove directory " << subpath << " " << strerror(errno) << endl;
        return false;
      }
    }
  }
  closedir(directory);
  return true;
}

bool DiskContents::makeFiles(string base_path, ofstream &diff_file) {
  get_contents(base_path.c_str());
  for (auto &i : contents) {
    if (S_ISDIR((i.second).stat_attr->st_mode)) {
      string filepath = base_path + i.first + "/" + "_dummy";
      int fd = open(filepath.c_str(), O_CREAT|O_RDWR);
      if (fd < 0) {
        diff_file <<  "Couldn't create file " << filepath << endl;
        cout <<  "Couldn't create file " << filepath << endl;
        return false;
      }
      close(fd);
    }
  }
  return true;
}

bool DiskContents::sanity_checks(ofstream &diff_file) {
  cout << __func__ << endl;
  string base_path = "/mnt/snapshot";
  if (!makeFiles(base_path, diff_file)) {
    cout << "Failed: Couldn't create files in all directories" << endl;
    diff_file << "Failed: Couldn't create files in all directories" << endl;
    return false;
  }

  if (!deleteFiles(base_path, diff_file)) {
    cout << "Failed: Couldn't delete all the existing directories" << endl;
    diff_file << "Failed: Couldn't delete all the existing directories" << endl;
    return false;
  }

  cout << "Passed sanity checks" << endl;
  return true;
}


} // namespace fs_testing
