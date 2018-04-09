#include "BaseTestCase.h"

namespace fs_testing {
namespace tests {

using std::string;

using fs_testing::user_tools::api::CmFsOps;
using fs_testing::user_tools::api::DefaultFsFns;

int BaseTestCase::init_values(string mount_dir, long filesys_size) {
  mnt_dir_ = mount_dir;
  filesys_size_ = filesys_size;
  return 0;
}

int BaseTestCase::Run(const int change_fd) {
  DefaultFsFns default_fns;
  CmFsOps cm(&default_fns);
  cm_ = &cm;
  int res = run();
  if (res < 0) {
    return res;
  }

  res = cm_->Serialize(change_fd);
  return res;
}

}  // namespace tests
}  // namespace fs_testing
