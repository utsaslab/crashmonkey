#include "FileSystemTestResult.h"

namespace fs_testing {

using std::ostream;

FileSystemTestResult::FileSystemTestResult() {
  error_summary_ = FileSystemTestResult::kClean;
}

void FileSystemTestResult::ResetError() {
  error_summary_ = FileSystemTestResult::kClean;
}

void FileSystemTestResult::SetError(ErrorType err) {
  error_summary_ = err;
}

FileSystemTestResult::ErrorType FileSystemTestResult::GetError() const {
  return error_summary_;
}

ostream& FileSystemTestResult::PrintErrors(ostream& os) {
  unsigned int noted_errors = error_summary_;
  unsigned int shift = 0;
  while (noted_errors != 0) {
    if (noted_errors & 1) {
      os << (FileSystemTestResult::ErrorType) (1 << shift);
    }
    ++shift;
    noted_errors >>= 1;
  }

  return os;
}

ostream& operator<<(ostream& os, FileSystemTestResult::ErrorType err) {
  switch (err) {
    case fs_testing::FileSystemTestResult::kUnmountable:
      os << "unmountable_file_system";
      break;
    case fs_testing::FileSystemTestResult::kCheck:
      os << "file_system_checker";
      break;
    case fs_testing::FileSystemTestResult::kFixed:
      os << "file_system_fixed";
      break;
    case fs_testing::FileSystemTestResult::kSnapshotRestore:
      os << "restoring_snapshot";
      break;
    case fs_testing::FileSystemTestResult::kBioWrite:
      os << "writing_crash_state";
      break;
    case fs_testing::FileSystemTestResult::kOther:
      os << "other_error";
      break;
    case fs_testing::FileSystemTestResult::kKernelMount:
      os << "kernel_mount";
      break;
    default:
      os.setstate(std::ios_base::failbit);
  }
  return os;
}


}  // namespace fs_testing
