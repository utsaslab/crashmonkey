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
};


/*
 * The below will need to be accessed via downcasting because it's not possible
 * to have different return values for override functions.
 */

/*
 * DiskMod that pertains only to changes that happen to files.
 */
class FileDiskMod : public DiskMod {
 public:
  unsigned int len;

  // Data passed to the write function.
  std::shared_ptr<char> data;
};

/*
 * DiskMod that pertains only to changes that happen to directories.
 */
class DirectoryDiskMod : public DiskMod {
 public:
  // Reflects any changes that may happen to the directory entries. This really
  // only needs to contain the diff of the checkpoint directory contents and the
  // new directory contents, meaning it really only needs to hold one change at
  // a time.
  struct dirent data;
};

}  // namespace utils
}  // namespace fs_testing
#endif  // UTILS_DISK_MOD_H
