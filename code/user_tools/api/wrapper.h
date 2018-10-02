//#include <linux/falloc.h>
//#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <tuple>
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
  virtual int FnRename(const std::string &old_path,
      const std::string &new_path) = 0;
  virtual int FnUnlink(const std::string &pathname) = 0;
  virtual int FnRemove(const std::string &pathname) = 0;

  virtual int FnStat(const std::string &pathname, struct stat *buf) = 0;
  virtual bool FnPathExists(const std::string &pathname) = 0;

  virtual int FnFsync(const int fd) = 0;
  virtual int FnFdatasync(const int fd) = 0;
  virtual void FnSync() = 0;
  // TODO(P.S.) check if we want to have syncfs
  // virtual int FnSyncfs(const int fd) = 0;
  virtual int FnSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) = 0;

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
  virtual int FnRename(const std::string &old_path,
      const std::string &new_path);
  virtual int FnUnlink(const std::string &pathname) override;
  virtual int FnRemove(const std::string &pathname) override;

  virtual int FnStat(const std::string &pathname, struct stat *buf) override;
  virtual bool FnPathExists(const std::string &pathname) override;

  virtual int FnFsync(const int fd) override;
  virtual int FnFdatasync(const int fd) override;
  virtual void FnSync() override;
  // TODO(P.S.) check if we want to have syncfs
  // virtual int FnSyncfs(const int fd) override;
  virtual int FnSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) override;

  virtual int CmCheckpoint() override;
};

class CmFsOps {
 public:
  virtual ~CmFsOps() {};

  virtual int CmMknod(const std::string &pathname, const mode_t mode,
      const dev_t dev) = 0;
  virtual int CmMkdir(const std::string &pathname, const mode_t mode) = 0;
  virtual int CmOpen(const std::string &pathname, const int flags) = 0;
  virtual int CmOpen(const std::string &pathname, const int flags,
      const mode_t mode) = 0;
  virtual off_t CmLseek(const int fd, const off_t offset, const int whence) = 0;
  virtual int CmWrite(const int fd, const void *buf, const size_t count) = 0;
  virtual ssize_t CmPwrite(const int fd, const void *buf, const size_t count,
      const off_t offset) = 0;
  virtual void * CmMmap(void *addr, const size_t length, const int prot,
      const int flags, const int fd, const off_t offset) = 0;
  virtual int CmMsync(void *addr, const size_t length, const int flags) = 0;
  virtual int CmMunmap(void *addr, const size_t length) = 0;
  virtual int CmFallocate(const int fd, const int mode, const off_t offset,
      off_t len) = 0;
  virtual int CmClose(const int fd) = 0;
  virtual int CmRename(const std::string &old_path,
      const std::string &new_path) = 0;
  virtual int CmUnlink(const std::string &pathname) = 0;
  virtual int CmRemove(const std::string &pathname) = 0;

  virtual int CmFsync(const int fd) = 0;
  virtual int CmFdatasync(const int fd) = 0;
  virtual void CmSync() = 0;
  // TODO(P.S.) check if we want to have syncfs
  // virtual int CmSyncfs(const int fd) = 0;
  virtual int CmSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags) = 0;

  virtual int CmCheckpoint() = 0;
};

/*
 * Provides an interface that will record all the changes the user makes to the
 * file system.
 */
class RecordCmFsOps : public CmFsOps {
 public:
  RecordCmFsOps(FsFns *functions);
  virtual ~RecordCmFsOps() {};

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
  int CmRename(const std::string &old_path, const std::string &new_path);
  int CmUnlink(const std::string &pathname);
  int CmRemove(const std::string &pathname);

  int CmFsync(const int fd);
  int CmFdatasync(const int fd);
  void CmSync();
  // TODO(P.S.) check if we want to have syncfs
  // int CmSyncfs(const int fd);
  virtual int CmSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags);

  int CmCheckpoint();

  int Serialize(const int fd);


  // Protected for testing purposes.
 protected:
  // So that things that require fd can be mapped to pathnames.
  std::unordered_map<int, std::string> fd_map_;

  // So that mmap pointers can be mapped to pathnames and mmap offset and
  // length.
  std::unordered_map<long long,
    std::tuple<std::string, unsigned long long, unsigned long long>> mmap_map_;
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

  /*
   * Write data out to the given file descriptor. Automatically retires until
   * all the requested data is written.
   */
  int WriteWhole(const int fd, const unsigned long long size,
      std::shared_ptr<char> data);
};

/*
 * Provides just a passthrough to the actual system calls. This is good for
 * situations where we don't want to record the individual changes a user makes
 * to the file system.
 */
class PassthroughCmFsOps : public CmFsOps {
 public:
  PassthroughCmFsOps(FsFns *functions);
  virtual ~PassthroughCmFsOps() {};

  virtual int CmMknod(const std::string &pathname, const mode_t mode,
      const dev_t dev);
  virtual int CmMkdir(const std::string &pathname, const mode_t mode);
  virtual int CmOpen(const std::string &pathname, const int flags);
  virtual int CmOpen(const std::string &pathname, const int flags,
      const mode_t mode);
  virtual off_t CmLseek(const int fd, const off_t offset, const int whence);
  virtual int CmWrite(const int fd, const void *buf, const size_t count);
  virtual ssize_t CmPwrite(const int fd, const void *buf, const size_t count,
      const off_t offset);
  virtual void * CmMmap(void *addr, const size_t length, const int prot,
      const int flags, const int fd, const off_t offset);
  virtual int CmMsync(void *addr, const size_t length, const int flags);
  virtual int CmMunmap(void *addr, const size_t length);
  virtual int CmFallocate(const int fd, const int mode, const off_t offset,
      off_t len);
  virtual int CmClose(const int fd);
  virtual int CmRename(const std::string &old_path,
      const std::string &new_path);
  virtual int CmUnlink(const std::string &pathname);
  virtual int CmRemove(const std::string &pathname);

  virtual int CmFsync(const int fd);
  virtual int CmFdatasync(const int fd);
  virtual void CmSync();
  // TODO(P.S.) check if we want to have syncfs
  // virtual int CmSyncfs(const int fd);
  virtual int CmSyncFileRange(const int fd, size_t offset, size_t nbytes,
    unsigned int flags);

  virtual int CmCheckpoint();

 protected:
  // Set of functions to call for different file system operations. Tracked as a
  // set of function pointers so that this class can be tested in a somewhat
  // sane manner.
  FsFns *fns_;
};

} // api
} // user_tools
} // fs_testing

