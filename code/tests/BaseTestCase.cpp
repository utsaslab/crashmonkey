#include "BaseTestCase.h"

namespace fs_testing {
namespace tests {

using std::string;

int BaseTestCase::pass(string mount_dir, string filesys_size) {
  mnt_dir_ = mount_dir;
  filesys_size_ = filesys_size;
  return 0;
}

}  // namespace tests
}  // namespace fs_testing
