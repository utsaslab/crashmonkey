#ifndef UTILS_DISK_MOD_H
#define UTILS_DISK_MOD_H

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

namespace fs_testing {
namespace utils {

class DiskMod {
 public:
  /*
   * Serialize a single DiskMod. Returns a shared_ptr to a non-NULL value on
   * success, else a NULL shared_ptr.
   */
  static std::shared_ptr<char> Serialize(DiskMod &dm, unsigned long long *size);

  /*
   * Deserialize a single DiskMod. Returns 0 on success, a value < 0 on failure.
   * On success, the DiskMod res is also populated with the deserialized values.
   */
  static int Deserialize(std::shared_ptr<char> data, DiskMod &res);

  enum ModType {
    // Changes to directories are implicitly tracked by noting which mods are
    // kCreateMod mods. Since kCreateMod means a new file or directory was made,
    // it also implies that the parent directory of the newly created item was
    // modified. Therefore, the change to the directory itself is not
    // represented by a mod since it would just be repeating information already
    // available (the parent directory can be deduced by examining the path in
    // the mod of type kCreateMod).
    kCreateMod = 0,     // File or directory created.
    kDataMetadataMod,   // Both data and metadata changed (ex. extending file).
    kMetadataMod,       // Only file metadata changed.
    kDataMod,           // Only file data changed.
    kDataMmapMod,       // Only file data changed via mmap.
    kRemoveMod,         // File or directory removed.
    kCheckpointMod,     // CrashMonkey Checkpoint() marker.
    kFsyncMod,          // For fsync/fdatasync that take an argument
    kSyncMod,           // sync, flushes all the contents
  };

  // TODO(ashmrtn): Figure out how to handle permissions.
  enum ModOpts {
    kNoneOpt = 0,           // No special flags given.
    kTruncateOpt,           // ex. truncate on open.
    // Below flags are fallocate specific.
    kFallocateOpt,          // For regular fallocate.
    kFallocateKeepSizeOpt,  // For regular fallocate with keep size.
    kPunchHoleKeepSizeOpt,  // Implies keep_size.
    kCollapseRangeOpt,      // Cannot have keep_size.
    kZeroRangeOpt,          // Does not have keep_size.
    kZeroRangeKeepSizeOpt,  // Does have keep_size.
    kInsertRangeOpt,        // Cannot have keep_size.
  };

  std::string path;
  ModType mod_type;
  ModOpts mod_opts;
  struct stat post_mod_stats;
  // This DiskMod represents a change to a directory. This field *could* be
  // removed if we *always* filled out the post_mod_stats struct and users of
  // the code just call IS_DIR on the stat struct.
  bool directory_mod;
  std::shared_ptr<char> file_mod_data;
  uint64_t file_mod_location;
  uint64_t file_mod_len;
  std::string directory_added_entry;

  DiskMod();

  /*
   * Zeros out all fields in the DiskMod.
   */
  void Reset();

 private:
  /*
   * Returns the number of bytes in the DiskMod in serialized form.
   */
  uint64_t GetSerializeSize();

  /*
   * Serialize various parts of a DiskMod. The SerializeHeader method only
   * serializes the mod_type and mod_opts fields as that is the only thing
   * common to all types (ex. kCheckpointMod type doesn't need anything but
   * that).
   */
  static int SerializeHeader(char *buf, const unsigned int buf_offset,
      DiskMod &dm);
  /*
   * Serializes stuff that is presesnt in file and directory operations.
   */
  static int SerializeChangeHeader(char *buf,
      const unsigned int buf_offset, DiskMod &dm);
  static int SerializeFileMod(char *buf, const unsigned int buf_offset,
      DiskMod &dm);
  static int SerializeDirectoryMod(char *buf, const unsigned int len,
      DiskMod &dm);
};

}  // namespace utils
}  // namespace fs_testing
#endif  // UTILS_DISK_MOD_H
