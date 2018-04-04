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
    CREATE,             // File or directory created.
    DATA_METADATA_MOD,  // Both data and metadata changed (ex. extending file).
    METADATA_MOD,       // Only file metadata changed.
    DATA_MOD,           // Only file data changed.
    DATA_MOD_MMAP,      // Only file data changed via mmap.
    REMOVE,             // File or directory removed.
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
  std::shared_ptr<char> file_data;
  struct dirent directory_data;
};

}  // namespace utils
}  // namespace fs_testing
#endif  // UTILS_DISK_MOD_H
