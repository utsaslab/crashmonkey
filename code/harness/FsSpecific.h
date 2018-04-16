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
   * Return the number of seconds to wait after a test case's run() method so
   * that all relevant disk I/O will be properly recorded.
   */
  virtual unsigned int GetPostRunDelaySeconds() = 0;
};

class ExtFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual unsigned int GetPostRunDelaySeconds() override;

 protected:
  ExtFsSpecific(std::string type, unsigned int delay_seconds);

 private:
  const std::string fs_type_;
  const unsigned int delay_seconds_;
};

class Ext2FsSpecific : public ExtFsSpecific {
 public:
  Ext2FsSpecific();
  static constexpr char kFsType[] = "ext2";
#if THREE_THIRTEEN == 1 || FOUR_FOUR == 1 || FOUR_FIFTEEN == 1 || \
    FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 20;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
};

class Ext3FsSpecific : public ExtFsSpecific {
 public:
  Ext3FsSpecific();
  static constexpr char kFsType[] = "ext3";
#if THREE_THIRTEEN == 1 || FOUR_FOUR == 1 || FOUR_FIFTEEN == 1 || \
    FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 42;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
};

class Ext4FsSpecific : public ExtFsSpecific {
 public:
  Ext4FsSpecific();
  static constexpr char kFsType[] = "ext4";
#if THREE_THIRTEEN == 1 || FOUR_FOUR == 1 || FOUR_FIFTEEN == 1 ||\
    FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 42;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
};

class BtrfsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual unsigned int GetPostRunDelaySeconds() override;

  static constexpr char kFsType[] = "btrfs";
#if THREE_THIRTEEN == 1 || FOUR_FOUR == 1 || FOUR_FIFTEEN == 1 || \
    FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 40;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
};

class F2fsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual unsigned int GetPostRunDelaySeconds() override;

  static constexpr char kFsType[] = "f2fs";
#if THREE_THIRTEEN == 1
  static const unsigned int kDelaySeconds = 15;
#elif FOUR_FOUR == 1
  static const unsigned int kDelaySeconds = 76;
#elif FOUR_FIFTEEN == 1 || FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 67;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
};

class XfsFsSpecific : public FsSpecific {
 public:
  virtual std::string GetFsTypeString();
  virtual std::string GetMkfsCommand(std::string &device_path);
  virtual std::string GetPostReplayMntOpts();
  virtual std::string GetFsckCommand(const std::string &fs_path);
  virtual fs_testing::FileSystemTestResult::ErrorType GetFsckReturn(
      int return_code);
  virtual unsigned int GetPostRunDelaySeconds() override;

  static constexpr char kFsType[] = "xfs";
#if FOUR_FIFTEEN == 1 || FOUR_SIXTEEN == 1
  static const unsigned int kDelaySeconds = 97;
#else
  static const unsigned int kDelaySeconds = 120;
#endif
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
