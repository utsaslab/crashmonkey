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
    // CREATE mods. Since CREATE means a new file or directory was made, it also
    // implies that the parent directory of the newly created item was modified.
    // Therefore, the change to the directory itself is not represented by a mod
    // since it would just be repeating information already available (the
    // parent directory can be deduced by examining the path in the mod of type
    // CREATE).
    CREATE,             // File or directory created.
    DATA_METADATA_MOD,  // Both data and metadata changed (ex. extending file).
    METADATA_MOD,       // Only file metadata changed.
    DATA_MOD,           // Only file data changed.
    DATA_MOD_MMAP,      // Only file data changed via mmap.
    REMOVE,             // File or directory removed.
    CHECKPOINT,         // CrashMonkey Checkpoint() marker.
  };

  // TODO(ashmrtn): Figure out how to handle permissions.
  enum ModOpts {
    NONE,               // No special flags given.
    TRUNCATE,           // ex. truncate on open.
    // Below flags are fallocate specific.
    PUNCH_HOLE,
    COLLAPSE_RANGE,
    ZERO_RANGE,
    INSERT_RANGE,
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
