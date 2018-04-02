#include "../api/wrapper.h"

#include <errno.h>
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

using fs_testing::utils::DirectoryDiskMod;
using fs_testing::utils::DiskMod;
using fs_testing::utils::FileDiskMod;


CmFsOpts::CmFsOpts() {
  FsFns f = {
    mknod,
    mkdir,
    open,
    lseek,
    write,
    pwrite,
    mmap,
    msync,
    munmap,
    fallocate,
    close,
    unlink,
    remove,

    Checkpoint
  };

  fns_ = f;
}

CmFsOpts::CmFsOpts(FsFns &functions) : fns_(functions) { }

int CmFsOpts::CmMknod(const string &pathname, const mode_t mode,
    const dev_t dev) {
  return fns_.fn_mknod(pathname.c_str(), mode, dev);
}

int CmFsOpts::CmMkdir(const string &pathname, const mode_t mode) {
  return fns_.fn_mkdir(pathname.c_str(), mode);
}

/*
 * TODO(ashmrtn): Clean up the logic in this. It shouldn't be possible to make a
 * new file in this function because the mode isn't an argument.
 */
int CmFsOpts::CmOpen(const string &pathname, const int flags) {
  bool new_file = false;
  // Will this make a new file or is this path a directory?
  struct stat stat_buf;
  errno = 0;
  const int stat_res = stat(pathname.c_str(), &stat_buf);
  if (stat_res < 0 && errno == ENOENT) {
    new_file = true;
  }

  const int res = fns_.fn_open(pathname.c_str(), flags);
  if (res < 0) {
    return res;
  }

  fd_map_.insert({res, {pathname, 0}});

  if (new_file || (flags & O_TRUNC)) {
    // We only want to record this op if we changed something on the file
    // system.

    DiskMod *mod = nullptr;
    if (S_ISDIR(stat_buf.st_mode)) {
      mod = new DirectoryDiskMod();
    } else {
      mod = new FileDiskMod();
    }

    if (new_file) {
      mod->mod_type = DiskMod::CREATE;
      mod->mod_opts = DiskMod::NONE;
    } else {
      mod->mod_type = DiskMod::DATA_METADATA_MOD;
      mod->mod_opts = DiskMod::TRUNCATE;
    }

    mod->path = pathname;
    const int post_stat_res = stat(pathname.c_str(), &mod->post_mod_stats);
    if (post_stat_res < 0) {
      // TODO(ashmrtn): Some sort of warning here?
      return res;
    }

    mods.push_back(shared_ptr<DiskMod>(mod));
  }

  return res;
}

int CmFsOpts::CmOpen(const string &pathname, const int flags,
    const mode_t mode) {
  return fns_.fn_open(pathname.c_str(), flags, mode);
}

off_t CmFsOpts::CmLseeks(const int fd, const off_t offset, const int whence) {
  return fns_.fn_lseek(fd, offset, whence);
}

int CmFsOpts::CmWrite(const int fd, const void *buf, const size_t count) {
  return fns_.fn_write(fd, buf, count);
}

ssize_t CmFsOpts::CmPwrite(const int fd, const void *buf, const size_t count,
    const off_t offset) {
  return fns_.fn_pwrite(fd, buf, count, offset);
}

void * CmFsOpts::CmMmap(void *addr, const size_t length, const int prot,
    const int flags, const int fd, const off_t offset) {
  return fns_.fn_mmap(addr, length, prot, flags, fd, offset);
}

int CmFsOpts::CmMsync(void *addr, const size_t length, const int flags) {
  return fns_.fn_msync(addr, length, flags);
}

int CmFsOpts::CmMunmap(void *addr, const size_t length) {
  return fns_.fn_munmap(addr, length);
}

int CmFsOpts::CmFallocate(const int fd, const int mode, const off_t offset,
    off_t len) {
  return fns_.fn_fallocate(fd, mode, offset, len);
}

int CmFsOpts::CmClose(const int fd) {
  const int res = fns_.fn_close(fd);

  if (res < 0) {
    return res;
  }

  fd_map_.erase(fd);

  return res;
}

int CmFsOpts::CmUnlink(const string &pathname) {
  return fns_.fn_unlink(pathname.c_str());
}

int CmFsOpts::CmRemove(const string &pathname) {
  return fns_.fn_remove(pathname.c_str());
}


int CmFsOpts::CmCheckpoint() {
  return fns_.cm_checkpoint();
}


} // api
} // user_tools
} // fs_testing
