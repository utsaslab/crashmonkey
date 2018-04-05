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
  enum ModType {
    // Changes to directories are implicitly tracked by noting which mods are
    // kCreateMod mods. Since kCreateMod means a new file or directory was made,
    // it also implies that the parent directory of the newly created item was
    // modified. Therefore, the change to the directory itself is not
    // represented by a mod since it would just be repeating information already
    // available (the parent directory can be deduced by examining the path in
    // the mod of type kCreateMod).
    kCreateMod,         // File or directory created.
    kDataMetadataMod,   // Both data and metadata changed (ex. extending file).
    kMetadataMod,       // Only file metadata changed.
    kDataMod,           // Only file data changed.
    kDataMmapMod,       // Only file data changed via mmap.
    kRemoveMod,         // File or directory removed.
    kCheckpointMod,     // CrashMonkey Checkpoint() marker.
  };

  // TODO(ashmrtn): Figure out how to handle permissions.
  enum ModOpts {
    kNoneOpt,           // No special flags given.
    kTruncateOpt,       // ex. truncate on open.
    // Below flags are fallocate specific.
    kPunchHoleOpt,
    kCollapseRangeOpt,
    kZeroRangeOpt,
    kInsertRangeOpt,
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
  off_t file_mod_location;
  unsigned int file_mod_len;
  struct dirent directory_mod_data;
};

}  // namespace utils
}  // namespace fs_testing
#endif  // UTILS_DISK_MOD_H
