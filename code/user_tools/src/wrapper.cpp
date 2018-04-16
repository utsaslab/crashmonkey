#include "../api/wrapper.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>


#include <iostream>

#include "../api/actions.h"

namespace fs_testing {
namespace user_tools {
namespace api {

using std::pair;
using std::shared_ptr;
using std::string;
using std::tuple;
using std::unordered_map;
using std::vector;

using fs_testing::utils::DiskMod;


int DefaultFsFns::FnMknod(const std::string &pathname, mode_t mode, dev_t dev) {
  return mknod(pathname.c_str(), mode, dev);
}

int DefaultFsFns::FnMkdir(const std::string &pathname, mode_t mode) {
  return mkdir(pathname.c_str(), mode);
}

int DefaultFsFns::FnOpen(const std::string &pathname, int flags) {
  return open(pathname.c_str(), flags);
}

int DefaultFsFns::FnOpen2(const std::string &pathname, int flags, mode_t mode) {
  return open(pathname.c_str(), flags, mode);
}

off_t DefaultFsFns::FnLseek(int fd, off_t offset, int whence) {
  return lseek(fd, offset, whence);
}

ssize_t DefaultFsFns::FnWrite(int fd, const void *buf, size_t count) {
  return write(fd, buf, count);
}

ssize_t DefaultFsFns::FnPwrite(int fd, const void *buf, size_t count,
    off_t offset) {
  return pwrite(fd, buf, count, offset);
}

void * DefaultFsFns::FnMmap(void *addr, size_t length, int prot, int flags,
    int fd, off_t offset) {
  return mmap(addr, length, prot, flags, fd, offset);
}

int DefaultFsFns::FnMsync(void *addr, size_t length, int flags) {
  return msync(addr, length, flags);
}

int DefaultFsFns::FnMunmap(void *addr, size_t length) {
  return munmap(addr, length);
}

int DefaultFsFns::FnFallocate(int fd, int mode, off_t offset, off_t len) {
  return fallocate(fd, mode, offset, len);
}

int DefaultFsFns::FnClose(int fd) {
  return close(fd);
}

int DefaultFsFns::FnRename(const string &old_path, const string &new_path) {
  return rename(old_path.c_str(), new_path.c_str());
}

int DefaultFsFns::FnUnlink(const std::string &pathname) {
  return unlink(pathname.c_str());
}

int DefaultFsFns::FnRemove(const std::string &pathname) {
  return remove(pathname.c_str());
}


int DefaultFsFns::FnStat(const std::string &pathname, struct stat *buf) {
  return stat(pathname.c_str(), buf);
}

bool DefaultFsFns::FnPathExists(const std::string &pathname) {
  const int res = access(pathname.c_str(), F_OK);
  // TODO(ashmrtn): Should probably have some better way to handle errors.
  std::cout << __func__ << " returns " << res << std::endl;
  if (res != 0) {
    return false;
  }

  return true;
}

int DefaultFsFns::FnFsync(const int fd) {
  return fsync(fd);
}

int DefaultFsFns::FnFdatasync(const int fd) {
  return fdatasync(fd);
}

void DefaultFsFns::FnSync() {
  sync();
}

// int DefaultFsFns::FnSyncfs(const int fd) {
//   return syncfs(fd);
// }

int DefaultFsFns::FnSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) {
  return sync_file_range(fd, offset, nbytes, flags);
}

int DefaultFsFns::CmCheckpoint() {
  return Checkpoint();
}


RecordCmFsOps::RecordCmFsOps(FsFns *functions) {
  fns_ = functions;
}

int RecordCmFsOps::CmMknod(const string &pathname, const mode_t mode,
    const dev_t dev) {
  return fns_->FnMknod(pathname.c_str(), mode, dev);
}

int RecordCmFsOps::CmMkdir(const string &pathname, const mode_t mode) {
  const int res = fns_->FnMkdir(pathname.c_str(), mode);
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.directory_mod = true;
  mod.path = pathname;
  mod.mod_type = DiskMod::kCreateMod;
  mod.mod_opts = DiskMod::kNoneOpt;

  mods_.push_back(mod);

  return res;
}

void RecordCmFsOps::CmOpenCommon(const int fd, const string &pathname,
    const bool exists, const int flags) {
  fd_map_.insert({fd, pathname});

  if (!exists || (flags & O_TRUNC)) {
    // We only want to record this op if we changed something on the file
    // system.

    DiskMod mod;

    // Need something like stat() because umask could affect the file
    // permissions.
    const int post_stat_res = fns_->FnStat(pathname.c_str(),
        &mod.post_mod_stats);
    if (post_stat_res < 0) {
      // TODO(ashmrtn): Some sort of warning here?
      return;
    }

    mod.directory_mod = S_ISDIR(mod.post_mod_stats.st_mode);

    if (!exists) {
      mod.mod_type = DiskMod::kCreateMod;
      mod.mod_opts = DiskMod::kNoneOpt;
    } else {
      mod.mod_type = DiskMod::kDataMetadataMod;
      mod.mod_opts = DiskMod::kTruncateOpt;
    }

    mod.path = pathname;

    mods_.push_back(mod);
  }
}

int RecordCmFsOps::CmOpen(const string &pathname, const int flags) {
  // Will this make a new file or is this path a directory?
  const bool exists = fns_->FnPathExists(pathname.c_str());

  const int res = fns_->FnOpen(pathname.c_str(), flags);
  if (res < 0) {
    return res;
  }

  CmOpenCommon(res, pathname, exists, flags);

  return res;
}

int RecordCmFsOps::CmOpen(const string &pathname, const int flags,
    const mode_t mode) {
  // Will this make a new file or is this path a directory?
  const bool exists = fns_->FnPathExists(pathname.c_str());

  const int res = fns_->FnOpen2(pathname.c_str(), flags, mode);
  if (res < 0) {
    return res;
  }

  CmOpenCommon(res, pathname, exists, flags);

  return res;
}

off_t RecordCmFsOps::CmLseek(const int fd, const off_t offset,
    const int whence) {
  return fns_->FnLseek(fd, offset, whence);
}

int RecordCmFsOps::CmWrite(const int fd, const void *buf, const size_t count) {
  DiskMod mod;
  mod.mod_opts = DiskMod::kNoneOpt;
  // Get current file position and size. If stat fails, then assume lseek will
  // fail too and just bail out.
  struct stat pre_stat_buf;
  // This could be an fstat(), but I don't see a reason to add another call that
  // does only reads to the already large interface of FsFns.
  int res = fns_->FnStat(fd_map_.at(fd), &pre_stat_buf);
  if (res < 0) {
    return res;
  }

  mod.file_mod_location = fns_->FnLseek(fd, 0, SEEK_CUR);
  if (mod.file_mod_location < 0) {
    return mod.file_mod_location;
  }

  const int write_res = fns_->FnWrite(fd, buf, count);
  if (write_res < 0) {
    return write_res;
  }

  mod.directory_mod = S_ISDIR(pre_stat_buf.st_mode);

  // TODO(ashmrtn): Support calling write directly on a directory.
  if (!mod.directory_mod) {
    // Copy over as much data as was written and see what the new file size is.
    // This will determine how we set the type of the DiskMod.
    mod.file_mod_len = write_res;
    mod.path = fd_map_.at(fd);

    res = fns_->FnStat(fd_map_.at(fd), &mod.post_mod_stats);
    if (res < 0) {
      return write_res;
    }

    if (pre_stat_buf.st_size != mod.post_mod_stats.st_size) {
      mod.mod_type = DiskMod::kDataMetadataMod;
    } else {
      mod.mod_type = DiskMod::kDataMod;
    }

    if (write_res > 0) {
      mod.file_mod_data.reset(new char[write_res], [](char* c) {delete[] c;});
      memcpy(mod.file_mod_data.get(), buf, write_res);
    }
  }

  mods_.push_back(mod);

  return write_res;
}

ssize_t RecordCmFsOps::CmPwrite(const int fd, const void *buf,
    const size_t count, const off_t offset) {
  DiskMod mod;
  mod.mod_opts = DiskMod::kNoneOpt;
  // Get current file position and size. If stat fails, then assume lseek will
  // fail too and just bail out.
  struct stat pre_stat_buf;
  // This could be an fstat(), but I don't see a reason to add another call that
  // does only reads to the already large interface of FsFns.
  int res = fns_->FnStat(fd_map_.at(fd), &pre_stat_buf);
  if (res < 0) {
    return res;
  }

  const int write_res = fns_->FnPwrite(fd, buf, count, offset);
  if (write_res < 0) {
    return write_res;
  }

  mod.directory_mod = S_ISDIR(pre_stat_buf.st_mode);

  // TODO(ashmrtn): Support calling write directly on a directory.
  if (!mod.directory_mod) {
    // Copy over as much data as was written and see what the new file size is.
    // This will determine how we set the type of the DiskMod.
    mod.file_mod_location = offset;
    mod.file_mod_len = write_res;
    mod.path = fd_map_.at(fd);

    res = fns_->FnStat(fd_map_.at(fd), &mod.post_mod_stats);
    if (res < 0) {
      return write_res;
    }

    if (pre_stat_buf.st_size != mod.post_mod_stats.st_size) {
      mod.mod_type = DiskMod::kDataMetadataMod;
    } else {
      mod.mod_type = DiskMod::kDataMod;
    }

    if (write_res > 0) {
      mod.file_mod_data.reset(new char[write_res], [](char* c) {delete[] c;});
      memcpy(mod.file_mod_data.get(), buf, write_res);
    }
  }

  mods_.push_back(mod);

  return write_res;
  return fns_->FnPwrite(fd, buf, count, offset);
}

void * RecordCmFsOps::CmMmap(void *addr, const size_t length, const int prot,
    const int flags, const int fd, const off_t offset) {
  void *res = fns_->FnMmap(addr, length, prot, flags, fd, offset);
  if (res == (void*) -1) {
    return res;
  }

  if (!(prot & PROT_WRITE) || flags & MAP_PRIVATE || flags & MAP_ANON ||
      flags & MAP_ANONYMOUS) {
    // In these cases, the user cannot write to the mmap-ed region, the region
    // is not backed by a file, or the changes the user makes are not reflected
    // in the file, so we can just act like this never happened.
    return res;
  }

  // All other cases we actually need to keep track of the fact that we mmap-ed
  // this region.
  mmap_map_.insert({(long long) res,
      tuple<string, unsigned int, unsigned int>(
          fd_map_.at(fd), offset, length)});
  return res;
}

int RecordCmFsOps::CmMsync(void *addr, const size_t length, const int flags) {
  const int res = fns_->FnMsync(addr, length, flags);
  if (res < 0) {
    return res;
  }

  // Check which file this belongs to. We need to do a search because they may
  // not have passed the address that was returned in mmap.
  for (const pair<long long, tuple<string, unsigned int, unsigned int>> &kv :
      mmap_map_) {
    if (addr >= (void*) kv.first &&
        addr < (void*) (kv.first + std::get<2>(kv.second))) {
      // This is the mapping you're looking for.
      DiskMod mod;
      mod.mod_type = DiskMod::kDataMod;
      mod.mod_opts = (flags & MS_ASYNC) ?
        DiskMod::kMsAsyncOpt : DiskMod::kMsSyncOpt;
      mod.path = std::get<0>(kv.second);
      // Offset into the file is the offset given in mmap plus the how far addr
      // is from the pointer returned by mmap.
      mod.file_mod_location =
        std::get<1>(kv.second) + ((long long) addr - kv.first);
      mod.file_mod_len = length;

      // Copy over the data that is being sync-ed. We don't know how it is
      // different than what was there to start with, but we'll have it!
      mod.file_mod_data.reset(new char[length], [](char* c) {delete[] c;});
      memcpy(mod.file_mod_data.get(), addr, length);

      mods_.push_back(mod);
      break;
    }
  }

  return res;
}

int RecordCmFsOps::CmMunmap(void *addr, const size_t length) {
  const int res = fns_->FnMunmap(addr, length);
  if (res < 0) {
    return res;
  }

  // TODO(ashmrtn): Assume that we always munmap with the same pointer and
  // length that we mmap-ed with. May not actually remove anything if the
  // mapping was not something that caused writes to be reflected in the
  // underlying file (i.e. the key wasn't present to begin with).
  mmap_map_.erase((long long int) addr);

  return res;
}

int RecordCmFsOps::CmFallocate(const int fd, const int mode, const off_t offset,
    off_t len) {
  struct stat pre_stat;
  const int pre_stat_res = fns_->FnStat(fd_map_[fd].c_str(), &pre_stat);
  if (pre_stat_res < 0) {
    return pre_stat_res;
  }

  const int res = fns_->FnFallocate(fd, mode, offset, len);
  if (res < 0) {
    return res;
  }

  struct stat post_stat;
  const int post_stat_res = fns_->FnStat(fd_map_[fd].c_str(), &post_stat);
  if (post_stat_res < 0) {
    return post_stat_res;
  }

  DiskMod mod;

  if (pre_stat.st_size != post_stat.st_size ||
      pre_stat.st_blocks != post_stat.st_blocks) {
    mod.mod_type = DiskMod::kDataMetadataMod;
  } else {
    mod.mod_type = DiskMod::kDataMod;
  }

  mod.path = fd_map_[fd];
  mod.file_mod_location = offset;
  mod.file_mod_len = len;

  if (mode & FALLOC_FL_PUNCH_HOLE) {
    // TODO(ashmrtn): Do we want this here? I'm not sure if it will cause
    // failures that we don't want, though man fallocate(2) says that
    // FALLOC_FL_PUNCH_HOLE must also have FALLOC_FL_KEEP_SIZE.
    assert(mode & FALLOC_FL_KEEP_SIZE);
    mod.mod_opts = DiskMod::kPunchHoleKeepSizeOpt;
  } else if (mode & FALLOC_FL_COLLAPSE_RANGE) {
    mod.mod_opts = DiskMod::kCollapseRangeOpt;
  } else if (mode & FALLOC_FL_ZERO_RANGE) {
    if (mode & FALLOC_FL_KEEP_SIZE) {
      mod.mod_opts = DiskMod::kZeroRangeKeepSizeOpt;
    } else {
      mod.mod_opts = DiskMod::kZeroRangeOpt;
    }
    /*
  } else if (mode & FALLOC_FL_INSERT_RANGE) {
    // TODO(ashmrtn): Figure out how to check with glibc defines.
    mod.mod_opts = DiskMod::kInsertRangeOpt;
    */
  } else if (mode & FALLOC_FL_KEEP_SIZE) {
    mod.mod_opts = DiskMod::kFallocateKeepSizeOpt;
  } else {
    mod.mod_opts = DiskMod::kFallocateOpt;
  }

  mods_.push_back(mod);

  return res;
}

int RecordCmFsOps::CmClose(const int fd) {
  const int res = fns_->FnClose(fd);

  if (res < 0) {
    return res;
  }

  fd_map_.erase(fd);

  return res;
}

int RecordCmFsOps::CmRename(const string &old_path, const string &new_path) {
  // check if there are any open files with the old path
  // change the file descriptors to point to the new path
  for (auto it = fd_map_.begin(); it != fd_map_.end(); it++) {
    string& open_fd_old_path = it->second;
    if (open_fd_old_path.compare(old_path) == 0) {
      fd_map_[it->first] = new_path;
    }
  }
  return fns_->FnRename(old_path, new_path);
}

int RecordCmFsOps::CmUnlink(const string &pathname) {
  const int res = fns_->FnUnlink(pathname.c_str());
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kRemoveMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mod.path = pathname;
  mods_.push_back(mod);

  return res;
}

int RecordCmFsOps::CmRemove(const string &pathname) {
  const int res = fns_->FnRemove(pathname.c_str());
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kRemoveMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mod.path = pathname;
  mods_.push_back(mod);

  return res;
}

int RecordCmFsOps::CmFsync(const int fd) {
  std::cout << __func__ << std::endl;
  const int res = fns_->FnFsync(fd);
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kFsyncMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mod.path = fd_map_.at(fd);
  for (auto i : fd_map_) {
    std::cout << i.first << " -> " << i.second << std::endl;
  }
  mods_.push_back(mod);

  return res;
}

int RecordCmFsOps::CmFdatasync(const int fd) {
  const int res = fns_->FnFdatasync(fd);
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kFsyncMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mod.path = fd_map_.at(fd);
  mods_.push_back(mod);

  return res;
}

void RecordCmFsOps::CmSync() {
  fns_->FnSync();
  
  DiskMod mod;
  mod.mod_type = DiskMod::kSyncMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mods_.push_back(mod);
}

// int RecordCmFsOps::CmSyncfs(const int fd) {
//   const int res = fns_->FnSyncfs(fd);
//   if (res < 0) {
//     return res;
//   }

//   DiskMod mod;
//   // Or should probably have a kSyncMod type with filepath (?)
//   mod.mod_type = DiskMod::kFsyncMod;
//   mod.mod_opts = DiskMod::kNoneOpt;
//   mod.path = fd_map_.at(fd);
//   mods_.push_back(mod);

//   return res;
// }

int RecordCmFsOps::CmSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) {
  const int res = fns_->FnSyncFileRange(fd, offset, nbytes, flags);
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kSyncFileRangeMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mod.path = fd_map_.at(fd);
  const int post_stat_res = fns_->FnStat(fd_map_.at(fd), &mod.post_mod_stats);
  if (post_stat_res < 0) {
    // TODO(ashmrtn): Some sort of warning here?
    return post_stat_res;
  }
  mod.file_mod_location = offset;
  mod.file_mod_len = nbytes;
  mods_.push_back(mod);
  return res;
}

int RecordCmFsOps::CmCheckpoint() {
  const int res = fns_->CmCheckpoint();
  if (res < 0) {
    return res;
  }

  DiskMod mod;
  mod.mod_type = DiskMod::kCheckpointMod;
  mod.mod_opts = DiskMod::kNoneOpt;
  mods_.push_back(mod);

  return res;
}

int RecordCmFsOps::WriteWhole(const int fd, const unsigned long long size,
    shared_ptr<char> data) {
  unsigned long long written = 0;
  while (written < size) {
    const int res = write(fd, data.get() + written, size - written);
    if (res < 0) {
      return res;
    }
    written += res;
  }

  return 0;
}

int RecordCmFsOps::Serialize(const int fd) {
  std::cout << __func__ << std::endl;
  for (auto &mod : mods_) {
    unsigned long long size;
    shared_ptr<char> serial_mod = DiskMod::Serialize(mod, &size);
    if (serial_mod == nullptr) {
      return -1;
    }

    const int res = WriteWhole(fd, size, serial_mod);
    if (res < 0) {
      return -1;
    }
  }

  return 0;
}



PassthroughCmFsOps::PassthroughCmFsOps(FsFns *functions) {
  fns_ = functions;
}

int PassthroughCmFsOps::CmMknod(const string &pathname, const mode_t mode,
    const dev_t dev) {
  return fns_->FnMknod(pathname.c_str(), mode, dev);
}

int PassthroughCmFsOps::CmMkdir(const string &pathname, const mode_t mode) {
  return fns_->FnMkdir(pathname.c_str(), mode);
}

int PassthroughCmFsOps::CmOpen(const string &pathname, const int flags) {
  return fns_->FnOpen(pathname.c_str(), flags);
}

int PassthroughCmFsOps::CmOpen(const string &pathname, const int flags,
    const mode_t mode) {
  return fns_->FnOpen2(pathname.c_str(), flags, mode);
}

off_t PassthroughCmFsOps::CmLseek(const int fd, const off_t offset,
    const int whence) {
  return fns_->FnLseek(fd, offset, whence);
}

int PassthroughCmFsOps::CmWrite(const int fd, const void *buf,
    const size_t count) {
  return fns_->FnWrite(fd, buf, count);
}

ssize_t PassthroughCmFsOps::CmPwrite(const int fd, const void *buf,
    const size_t count, const off_t offset) {
  return fns_->FnPwrite(fd, buf, count, offset);
}

void * PassthroughCmFsOps::CmMmap(void *addr, const size_t length,
    const int prot, const int flags, const int fd, const off_t offset) {
  return fns_->FnMmap(addr, length, prot, flags, fd, offset);
}

int PassthroughCmFsOps::CmMsync(void *addr, const size_t length,
    const int flags) {
  return fns_->FnMsync(addr, length, flags);
}

int PassthroughCmFsOps::CmMunmap(void *addr, const size_t length) {
  return fns_->FnMunmap(addr, length);
}

int PassthroughCmFsOps::CmFallocate(const int fd, const int mode,
    const off_t offset, off_t len) {
  return fns_->FnFallocate(fd, mode, offset, len);
}

int PassthroughCmFsOps::CmClose(const int fd) {
  return fns_->FnClose(fd);
}

int PassthroughCmFsOps::CmRename(const string &old_path,
    const string &new_path) {
  return fns_->FnRename(old_path, new_path);
}

int PassthroughCmFsOps::CmUnlink(const string &pathname) {
  return fns_->FnUnlink(pathname.c_str());
}

int PassthroughCmFsOps::CmRemove(const string &pathname) {
  return fns_->FnRemove(pathname.c_str());
}

int PassthroughCmFsOps::CmFsync(const int fd) {
  return fns_->FnFsync(fd);
}

int PassthroughCmFsOps::CmFdatasync(const int fd) {
  return fns_->FnFdatasync(fd);
}

void PassthroughCmFsOps::CmSync() {
  fns_->FnSync();
}

// int PassthroughCmFsOps::CmSyncfs(const int fd) {
//   return fns_->FnSyncfs(fd);
// }

int PassthroughCmFsOps::CmSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) {
  fns_->FnSyncFileRange(fd, offset, nbytes, flags);
}

int PassthroughCmFsOps::CmCheckpoint() {
  return fns_->CmCheckpoint();
}

} // api
} // user_tools
} // fs_testing
