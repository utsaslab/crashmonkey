#include "FsSpecific.h"

namespace fs_testing {

using std::string;

namespace {

constexpr char kMkfsStart[] = "mkfs -t ";
constexpr char kFsckCommand[] = "fsck -T -t ";

constexpr char kExt4RemountOpts[] = "errors=remount-ro";

constexpr char kBtrfsFsckCommand[] = "btrfs check ";

constexpr char kXfsFsckCommand[] = "xfs_repair ";
}


FsSpecific* GetFsSpecific(std::string &fs_type) {
  // TODO(ashmrtn): Find an elegant way to handle errors.
  if (fs_type.compare(Ext4FsSpecific::kFsType) == 0) {
    return new Ext4FsSpecific();
  } else if (fs_type.compare(BtrfsFsSpecific::kFsType) == 0) {
    return new BtrfsFsSpecific();
  } else if (fs_type.compare(F2fsFsSpecific::kFsType) == 0) {
    return new F2fsFsSpecific();
  } else if (fs_type.compare(XfsFsSpecific::kFsType) == 0) {
    return new XfsFsSpecific();
  }
  return NULL;
}

/******************************* Ext4 *****************************************/
// Weird C++11 rule about static constexpr that aren't "simple".
constexpr char Ext4FsSpecific::kFsType[];

string Ext4FsSpecific::GetMkfsCommand(string &device_path) {
  return string(kMkfsStart) + Ext4FsSpecific::kFsType + " " +  device_path;
}

string Ext4FsSpecific::GetPostReplayMntOpts() {
  return string(kExt4RemountOpts);
}

string Ext4FsSpecific::GetFsckCommand(const string &fs_path) {
  return string(kFsckCommand) + kFsType + " " + fs_path + " -- -y";
}

FileSystemTestResult::ErrorType Ext4FsSpecific::GetFsckReturn(
    int return_code) {
  // The following is taken from the specification in man(8) fsck.ext4.
  if ((return_code & 0x8) || (return_code & 0x10) || (return_code & 0x20) ||
      // Some sort of fsck error.
      return_code & 0x80) {
    return FileSystemTestResult::kCheck;
  }

  if (return_code & 0x4) {
    return FileSystemTestResult::kCheckUnfixed;
  }

  if ((return_code & 0x1) || (return_code & 0x2)) {
    return FileSystemTestResult::kFixed;
  }

  if (return_code == 0) {
    return FileSystemTestResult::kClean;
  }

  // Default selection so at least something looks wrong.
  return FileSystemTestResult::kOther;
}

string Ext4FsSpecific::GetFsTypeString() {
  return string(Ext4FsSpecific::kFsType);
}

bool Ext4FsSpecific::AlwaysRunFsck() {
  return Ext4FsSpecific::kAlwaysRunFsck;
}

/******************************* Btrfs ****************************************/
constexpr char BtrfsFsSpecific::kFsType[];

string BtrfsFsSpecific::GetMkfsCommand(string &device_path) {
  return string(kMkfsStart) + BtrfsFsSpecific::kFsType + " " +  device_path;
}

string BtrfsFsSpecific::GetPostReplayMntOpts() {
  return string();
}

string BtrfsFsSpecific::GetFsckCommand(const string &fs_path) {
  return string(kBtrfsFsckCommand) + fs_path;
}

FileSystemTestResult::ErrorType BtrfsFsSpecific::GetFsckReturn(
    int return_code) {
  // The following is taken from the specification in man(8) btrfs-check.
  // `btrfs check` is much less expressive in its return codes than fsck.ext4.
  // Here all we get is 0/1 corresponding to success/failure respectively. For
  // 0, FileSystemTestResult::kClean will be assumed. For 1,
  // FileSystemTestResult::kOtherError will be assumed. This is temporary until
  // we know better how to assign output values.
  if (return_code == 0) {
    return FileSystemTestResult::kClean;
  }
  return FileSystemTestResult::kCheck;
}

string BtrfsFsSpecific::GetFsTypeString() {
  return string(BtrfsFsSpecific::kFsType);
}

bool BtrfsFsSpecific::AlwaysRunFsck() {
  return BtrfsFsSpecific::kAlwaysRunFsck;
}

/******************************* F2fs *****************************************/
constexpr char F2fsFsSpecific::kFsType[];

string F2fsFsSpecific::GetMkfsCommand(string &device_path) {
  return string(kMkfsStart) + F2fsFsSpecific::kFsType + " " +  device_path;
}

string F2fsFsSpecific::GetPostReplayMntOpts() {
  return string();
}

string F2fsFsSpecific::GetFsckCommand(const string &fs_path) {
  return string(kFsckCommand) + kFsType + " " + fs_path;
}

FileSystemTestResult::ErrorType F2fsFsSpecific::GetFsckReturn(
    int return_code) {
  // The following is taken from the specification in man(8) fsck.f2fs.
  // `fsck.f2fs` is much less expressive in its return codes than fsck.ext4.
  // Here all we get is 0/-1 corresponding to success/failure respectively. For
  // 0, FileSystemTestResult::kClean will be assumed. For -1,
  // FileSystemTestResult::kOther will be assumed. This is temporary until we
  // know better how to assign output values.
  if (return_code == 0) {
    return FileSystemTestResult::kClean;
  }
  return FileSystemTestResult::kCheck;
}

string F2fsFsSpecific::GetFsTypeString() {
  return string(F2fsFsSpecific::kFsType);
}

bool F2fsFsSpecific::AlwaysRunFsck() {
  return F2fsFsSpecific::kAlwaysRunFsck;
}

/******************************* Xfs ******************************************/
constexpr char XfsFsSpecific::kFsType[];

string XfsFsSpecific::GetMkfsCommand(string &device_path) {
  return string(kMkfsStart) + XfsFsSpecific::kFsType + " " +  device_path;
}

string XfsFsSpecific::GetPostReplayMntOpts() {
  return string();
}

string XfsFsSpecific::GetFsckCommand(const string &fs_path) {
  return string(kXfsFsckCommand) + fs_path;
}

FileSystemTestResult::ErrorType XfsFsSpecific::GetFsckReturn(
    int return_code) {
  // Always returns 0...
  if (return_code == 0) {
    return FileSystemTestResult::kClean;
  }
  return FileSystemTestResult::kCheck;
}

string XfsFsSpecific::GetFsTypeString() {
  return string(XfsFsSpecific::kFsType);
}

bool XfsFsSpecific::AlwaysRunFsck() {
  return XfsFsSpecific::kAlwaysRunFsck;
}

}  // namespace fs_testing
