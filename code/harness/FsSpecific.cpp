#include "FsSpecific.h"

namespace fs_testing {

using std::string;

namespace {

constexpr char kMkfsStart[] = "mkfs -t ";
constexpr char kFsckCommand[] = "fsck -T -t ";

constexpr char kExtRemountOpts[] = "errors=remount-ro";
// Disable lazy init for now.
constexpr char kExtMkfsOpts[] =
  "-E lazy_itable_init=0,lazy_journal_init=0";

// TODO(ashmrtn): See if we actually want the repair flag or not. The man page
// for btrfs check is not clear on whether it will try to cleanup the file
// system some without it. It also says to be careful about using the repair
// flag.
constexpr char kBtrfsFsckCommand[] = "yes | btrfs check ";

constexpr char kXfsFsckCommand[] = "xfs_repair ";
}


FsSpecific* GetFsSpecific(std::string &fs_type) {
  // TODO(ashmrtn): Find an elegant way to handle errors.
  if (fs_type.compare(Ext4FsSpecific::kFsType) == 0) {
    return new Ext4FsSpecific();
  } else if (fs_type.compare(Ext3FsSpecific::kFsType) == 0) {
    return new Ext3FsSpecific();
  } else if (fs_type.compare(Ext2FsSpecific::kFsType) == 0) {
    return new Ext2FsSpecific();
  } else if (fs_type.compare(BtrfsFsSpecific::kFsType) == 0) {
    return new BtrfsFsSpecific();
  } else if (fs_type.compare(F2fsFsSpecific::kFsType) == 0) {
    return new F2fsFsSpecific();
  } else if (fs_type.compare(XfsFsSpecific::kFsType) == 0) {
    return new XfsFsSpecific();
  }
  return NULL;
}

/******************************* Ext File Systems *****************************/
constexpr char Ext2FsSpecific::kFsType[];
Ext2FsSpecific::Ext2FsSpecific() : ExtFsSpecific(Ext2FsSpecific::kFsType) { }

constexpr char Ext3FsSpecific::kFsType[];
Ext3FsSpecific::Ext3FsSpecific() : ExtFsSpecific(Ext3FsSpecific::kFsType) { }

constexpr char Ext4FsSpecific::kFsType[];
Ext4FsSpecific::Ext4FsSpecific() : ExtFsSpecific(Ext4FsSpecific::kFsType) { }

ExtFsSpecific::ExtFsSpecific(std::string type) : fs_type_(type) { }

string ExtFsSpecific::GetMkfsCommand(string &device_path) {
  return string(kMkfsStart) + fs_type_ + " " +
    kExtMkfsOpts + " " + device_path;
}

string ExtFsSpecific::GetPostReplayMntOpts() {
  return string(kExtRemountOpts);
}

string ExtFsSpecific::GetFsckCommand(const string &fs_path) {
  return string(kFsckCommand) + fs_type_ + " " + fs_path + " -- -y";
}

FileSystemTestResult::ErrorType ExtFsSpecific::GetFsckReturn(
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

string ExtFsSpecific::GetFsTypeString() {
  return string(Ext4FsSpecific::kFsType);
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
  // 0, `btrfs check` did not find anything out of the ordinary. For 1,
  // `btrfs check` found something. The tests in the btrfs-progs repo seem to
  // imply that it won't automatically fix things for you so we return
  // FileSystemTestResult::kCheckUnfixed.
  if (return_code == 0) {
    return FileSystemTestResult::kFixed;
  }
  return FileSystemTestResult::kCheckUnfixed;
}

string BtrfsFsSpecific::GetFsTypeString() {
  return string(BtrfsFsSpecific::kFsType);
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
  return string(kFsckCommand) + kFsType + " " + fs_path + " -- -y";
}

FileSystemTestResult::ErrorType F2fsFsSpecific::GetFsckReturn(
    int return_code) {
  // The following is taken from the specification in man(8) fsck.f2fs.
  // `fsck.f2fs` is much less expressive in its return codes than fsck.ext4.
  // Here all we get is 0/-1 corresponding to success/failure respectively. For
  // 0, FileSystemTestResult::kFixed will be assumed as it appears that 0 is the
  // return when fsck has completed running (the function that runs fsck is void
  // in the source code so there is no way to tell what it did easily). For -1,
  // FileSystemTestResult::kCheck will be assumed.
  // TODO(ashmrtn): Update with better values based on string parsing.
  if (return_code == 0) {
    return FileSystemTestResult::kFixed;
  }
  return FileSystemTestResult::kCheck;
}

string F2fsFsSpecific::GetFsTypeString() {
  return string(F2fsFsSpecific::kFsType);
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
  if (return_code == 0) {
    // Will always return 0 when running without the dry-run flag. Things have
    // been fixed though.
    return FileSystemTestResult::kFixed;
  }
  return FileSystemTestResult::kCheck;
}

string XfsFsSpecific::GetFsTypeString() {
  return string(XfsFsSpecific::kFsType);
}

}  // namespace fs_testing
