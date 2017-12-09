#include "BaseTestCase.h"

namespace fs_testing {
namespace tests {

using std::string;

int BaseTestCase::init_values(string mount_dir, long filesys_size) {
  mnt_dir_ = mount_dir;
  filesys_size_ = filesys_size;
  return 0;
}

}  // namespace tests
}  // namespace fs_testing
