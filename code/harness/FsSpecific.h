#ifndef HARNESS_TESTER_H
#define HARNESS_TESTER_H

#include <string>

#include "../results/FileSystemTestResult.h"

namespace fs_testing {

class FsSpecific {
 public:
  /*
   * Returns a string representing the file system type (ex. "ext4" or "btrfs").
   */
  virtual std::string GetFsTypeString() = 0;

  /*
   * Returns a string containing the shell command to run to make a file system
   * of a specific format. Takes as an argument the path to the device that will
   * hold the newly created file system.
   *
   * May need to be expanded later to take user arguments.
   */
  virtual std::string GetMkfsCommand(std::string &device_path) = 0;

  /*
   * Returns a string of arguments (to be passed to mount(2)) the file system
   * may want to be mounted with when it is mounted after the crash state has
   * been replayed but before fsck is run.
   */
  virtual std::string GetPostReplayMntOpts() = 0;

  /*
   * Returns a string containing the shell command to run to run the file system
   * specific checker. Takes as an argument the device the file system checker
   * should be run on.
   *
   * May need to be expanded later to take user arguments.
   */
  virtual std::string GetFsckCommand(const std::string &fs_path) = 0;

  /*
   * Returns an enum representing the exit status of the file system specific
   * file system checker used. Takes as an argument the return value that was
   * retrieved from the file system checker subprocess.
   */
  virtual fs_testing::FileSystemTestResult::ErrorType
    GetFsckReturn(int return_code) = 0;

  /*
   * Whether or not to run fsck (or equivalent) each time we have a new crash
   * state or just when there are mount problems with the file system.
   */
  virtual bool AlwaysRunFsck() = 0;
};

class Ext4FsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual bool AlwaysRunFsck();

  static constexpr char kFsType[] = "ext4";

 private:
  static const bool kAlwaysRunFsck = true;
};

class BtrfsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual bool AlwaysRunFsck();

  static constexpr char kFsType[] = "btrfs";

 private:
  static const bool kAlwaysRunFsck = true;
};

class F2fsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual bool AlwaysRunFsck();

  static constexpr char kFsType[] = "f2fs";

 private:
  static const bool kAlwaysRunFsck = true;
};

class XfsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual bool AlwaysRunFsck();

  static constexpr char kFsType[] = "xfs";

 private:
  static const bool kAlwaysRunFsck = false;
};

/*
 * Return a subclass of FsSpecific corresponding to the given file system type.
 * The parameter fs_type should be an all lower case string representing the
 * type of the file system. The caller is responsible for destroying the object
 * returned by this method.
 *
 * Supported types include: ext4, btrfs, xfs, f2fs.
 */
FsSpecific* GetFsSpecific(std::string &fs_type);

}  // namespace fs_testing

#endif  // HARNESS_TESTER_H
