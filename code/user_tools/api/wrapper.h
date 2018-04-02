#ifndef USER_TOOLS_API_WRAPPER_H
#define USER_TOOLS_API_WRAPPER_H

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../utils/DiskMod.h"

namespace fs_testing {
namespace user_tools {
namespace api {

/*
 * So that things can be tested in a somewhat reasonable fashion by swapping out
 * the functions called.
 */
struct FsFns {
  int (*fn_mknod)(const char *pathname, mode_t mode, dev_t dev);
  int (*fn_mkdir)(const char *pathname, mode_t mode);
  // Interestingly, open() is a varargs function.
  int (*fn_open)(const char *pathname, int flags, ...);
  off_t (*fn_lseek)(int fd, off_t offset, int whence);
  ssize_t (*fn_write)(int fd, const void *buf, size_t count);
  ssize_t (*fn_pwrite)(int fd, const void *buf, size_t count, off_t offset);
  void * (*fn_mmap)(void *addr, size_t length, int prot, int flags, int fd,
      off_t offset);
  int (*fn_msync)(void *addr, size_t length, int flags);
  int (*fn_munmap)(void *addr, size_t length);
  int (*fn_fallocate)(int fd, int mode, off_t offset, off_t len);
  int (*fn_close)(int fd);
  int (*fn_unlink)(const char *pathname);
  int (*fn_remove)(const char *pathname);

  int (*cm_checkpoint)();
};

class CmFsOps {
 public:
  CmFsOps();
  CmFsOps(FsFns &functions);

  int CmMknod(const std::string &pathname, const mode_t mode, const dev_t dev);
  int CmMkdir(const std::string &pathname, const mode_t mode);
  int CmOpen(const std::string &pathname, const int flags);
  int CmOpen(const std::string &pathname, const int flags, const mode_t mode);
  off_t CmLseeks(const int fd, const off_t offset, const int whence);
  int CmWrite(const int fd, const void *buf, const size_t count);
  ssize_t CmPwrite(const int fd, const void *buf, const size_t count,
      const off_t offset);
  void * CmMmap(void *addr, const size_t length, const int prot,
      const int flags, const int fd, const off_t offset);
  int CmMsync(void *addr, const size_t length, const int flags);
  int CmMunmap(void *addr, const size_t length);
  int CmFallocate(const int fd, const int mode, const off_t offset, off_t len);
  int CmClose(const int fd);
  int CmUnlink(const std::string &pathname);
  int CmRemove(const std::string &pathname);

  int CmCheckpoint();

  std::vector<std::shared_ptr<fs_testing::utils::DiskMod>> mods;

 private:
  // So that things that require fd can be mapped to pathnames. Also holds the
  // offset into the file that the system is working with.
  std::unordered_map<int, std::pair<std::string, unsigned int>> fd_map_;
  // So that mmap pointers can be mapped to pathnames.
  std::unordered_map<int, std::string> mmap_map_;

  FsFns fns_;
};

} // api
} // user_tools
} // fs_testing

#endif // USER_TOOLS_API_WRAPPER_H
