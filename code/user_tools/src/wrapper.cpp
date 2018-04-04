#include "../api/wrapper.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../api/actions.h"

namespace fs_testing {
namespace user_tools {
namespace api {

using std::shared_ptr;
using std::string;
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
  if (res != 0) {
    return false;
  }

  return true;
}


int DefaultFsFns::CmCheckpoint() {
  return Checkpoint();
}


CmFsOps::CmFsOps(FsFns *functions) {
  fns_ = functions;
}

int CmFsOps::CmMknod(const string &pathname, const mode_t mode,
    const dev_t dev) {
  return fns_->FnMknod(pathname.c_str(), mode, dev);
}

int CmFsOps::CmMkdir(const string &pathname, const mode_t mode) {
  return fns_->FnMkdir(pathname.c_str(), mode);
}

void CmFsOps::CmOpenCommon(const int fd, const string &pathname,
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
      mod.mod_type = DiskMod::CREATE;
      mod.mod_opts = DiskMod::NONE;
    } else {
      mod.mod_type = DiskMod::DATA_METADATA_MOD;
      mod.mod_opts = DiskMod::TRUNCATE;
    }

    mod.path = pathname;

    mods_.push_back(mod);
  }
}

int CmFsOps::CmOpen(const string &pathname, const int flags) {
  // Will this make a new file or is this path a directory?
  const bool exists = fns_->FnPathExists(pathname.c_str());

  const int res = fns_->FnOpen(pathname.c_str(), flags);
  if (res < 0) {
    return res;
  }

  CmOpenCommon(res, pathname, exists, flags);

  return res;
}

int CmFsOps::CmOpen(const string &pathname, const int flags,
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

off_t CmFsOps::CmLseeks(const int fd, const off_t offset, const int whence) {
  return fns_->FnLseek(fd, offset, whence);
}

int CmFsOps::CmWrite(const int fd, const void *buf, const size_t count) {
  DiskMod mod;
  mod.mod_opts = DiskMod::NONE;
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
      mod.mod_type = DiskMod::DATA_METADATA_MOD;
    } else {
      mod.mod_type = DiskMod::DATA_MOD;
    }

    if (write_res > 0) {
      mod.file_mod_data.reset(new char[write_res], [](char* c) {delete[] c;});
      memcpy(mod.file_mod_data.get(), buf, write_res);
    }
  }

  mods_.push_back(mod);

  return write_res;
}

ssize_t CmFsOps::CmPwrite(const int fd, const void *buf, const size_t count,
    const off_t offset) {
  return fns_->FnPwrite(fd, buf, count, offset);
}

void * CmFsOps::CmMmap(void *addr, const size_t length, const int prot,
    const int flags, const int fd, const off_t offset) {
  return fns_->FnMmap(addr, length, prot, flags, fd, offset);
}

int CmFsOps::CmMsync(void *addr, const size_t length, const int flags) {
  return fns_->FnMsync(addr, length, flags);
}

int CmFsOps::CmMunmap(void *addr, const size_t length) {
  return fns_->FnMunmap(addr, length);
}

int CmFsOps::CmFallocate(const int fd, const int mode, const off_t offset,
    off_t len) {
  return fns_->FnFallocate(fd, mode, offset, len);
}

int CmFsOps::CmClose(const int fd) {
  const int res = fns_->FnClose(fd);

  if (res < 0) {
    return res;
  }

  fd_map_.erase(fd);

  return res;
}

int CmFsOps::CmUnlink(const string &pathname) {
  return fns_->FnUnlink(pathname.c_str());
}

int CmFsOps::CmRemove(const string &pathname) {
  return fns_->FnRemove(pathname.c_str());
}


int CmFsOps::CmCheckpoint() {
  return fns_->CmCheckpoint();
}


} // api
} // user_tools
} // fs_testing
