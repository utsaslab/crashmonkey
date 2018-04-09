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
#include <vector>

#include "../../utils/DiskMod.h"

namespace fs_testing {
namespace user_tools {
namespace api {

/*
 * So that things can be tested in a somewhat reasonable fashion by swapping out
 * the functions called.
 */
class FsFns {
 public:
  virtual ~FsFns() {};
  virtual int FnMknod(const std::string &pathname, mode_t mode, dev_t dev) = 0;
  virtual int FnMkdir(const std::string &pathname, mode_t mode) = 0;
  virtual int FnOpen(const std::string &pathname, int flags) = 0;
  virtual int FnOpen2(const std::string &pathname, int flags, mode_t mode) = 0;
  virtual off_t FnLseek(int fd, off_t offset, int whence) = 0;
  virtual ssize_t FnWrite(int fd, const void *buf, size_t count) = 0;
  virtual ssize_t FnPwrite(int fd, const void *buf, size_t count,
      off_t offset) = 0;
  virtual void * FnMmap(void *addr, size_t length, int prot, int flags, int fd,
      off_t offset) = 0;
  virtual int FnMsync(void *addr, size_t length, int flags) = 0;
  virtual int FnMunmap(void *addr, size_t length) = 0;
  virtual int FnFallocate(int fd, int mode, off_t offset, off_t len) = 0;
  virtual int FnClose(int fd) = 0;
  virtual int FnUnlink(const std::string &pathname) = 0;
  virtual int FnRemove(const std::string &pathname) = 0;

  virtual int FnStat(const std::string &pathname, struct stat *buf) = 0;
  virtual bool FnPathExists(const std::string &pathname) = 0;

  virtual int CmCheckpoint() = 0;
};

class DefaultFsFns : public FsFns {
 public:
  virtual int FnMknod(const std::string &pathname, mode_t mode,
      dev_t dev) override;
  virtual int FnMkdir(const std::string &pathname, mode_t mode) override;
  virtual int FnOpen(const std::string &pathname, int flags) override;
  virtual int FnOpen2(const std::string &pathname, int flags,
      mode_t mode) override;
  virtual off_t FnLseek(int fd, off_t offset, int whence) override;
  virtual ssize_t FnWrite(int fd, const void *buf, size_t count) override;
  virtual ssize_t FnPwrite(int fd, const void *buf, size_t count,
      off_t offset) override;
  virtual void * FnMmap(void *addr, size_t length, int prot, int flags, int fd,
      off_t offset) override;
  virtual int FnMsync(void *addr, size_t length, int flags) override;
  virtual int FnMunmap(void *addr, size_t length) override;
  virtual int FnFallocate(int fd, int mode, off_t offset, off_t len) override;
  virtual int FnClose(int fd) override;
  virtual int FnUnlink(const std::string &pathname) override;
  virtual int FnRemove(const std::string &pathname) override;

  virtual int FnStat(const std::string &pathname, struct stat *buf) override;
  virtual bool FnPathExists(const std::string &pathname) override;

  virtual int CmCheckpoint() override;
};

class CmFsOps {
 public:
  CmFsOps(FsFns *functions);
  virtual ~CmFsOps() {};

  int CmMknod(const std::string &pathname, const mode_t mode, const dev_t dev);
  int CmMkdir(const std::string &pathname, const mode_t mode);
  int CmOpen(const std::string &pathname, const int flags);
  int CmOpen(const std::string &pathname, const int flags, const mode_t mode);
  off_t CmLseek(const int fd, const off_t offset, const int whence);
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


  // Protected for testing purposes.
 protected:
  // So that things that require fd can be mapped to pathnames.
  std::unordered_map<int, std::string> fd_map_;

  // So that mmap pointers can be mapped to pathnames.
  std::unordered_map<int, std::string> mmap_map_;
  std::vector<fs_testing::utils::DiskMod> mods_;

  // Set of functions to call for different file system operations. Tracked as a
  // set of function pointers so that this class can be tested in a somewhat
  // sane manner.
  FsFns *fns_;

 private:
  /*
   * Common code for open with 2 and 3 arguments.
   */
  void CmOpenCommon(const int fd, const std::string &pathname,
      const bool exists, const int flags);
};

} // api
} // user_tools
} // fs_testing

#endif // USER_TOOLS_API_WRAPPER_H
