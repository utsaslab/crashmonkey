#ifndef HARNESS_FILE_SYSTEM_TEST_RESULT_H
#define HARNESS_FILE_SYSTEM_TEST_RESULT_H

#include <iostream>

namespace fs_testing {

namespace {
  static const unsigned int kUnmountable_ = 0;
  static const unsigned int kFixed_ = 1;
  static const unsigned int kCheck_ = 2;
  static const unsigned int kSnapshotRestore_ = 3;
  static const unsigned int kBioWrite_ = 4;
  static const unsigned int kOther_ = 5;
  static const unsigned int kKernelMount_ = 6;
}  // namespace

class FileSystemTestResult {
 public:
  enum ErrorType {
    kClean = 0,
    kUnmountable = (1 << kUnmountable_),
    kFixed = (1 << kFixed_),
    kCheck = (1 << kCheck_),
    kSnapshotRestore = (1 << kSnapshotRestore_),
    kBioWrite = (1 << kBioWrite_),
    kOther = (1 << kOther_),
    kKernelMount = (1 << kKernelMount_),
  };

  FileSystemTestResult();
  void ResetError();
  void SetError(ErrorType errors);
  ErrorType GetError() const;
  std::ostream& PrintErrors(std::ostream& os);
  std::string error_description;
  std::string fsck_result;

  int fs_check_return;

 private:
  ErrorType error_summary_;
};

std::ostream& operator<<(std::ostream& os, FileSystemTestResult::ErrorType err);

};  // namespace fs_testing

#endif  // HARNESS_FILE_SYSTEM_TEST_RESULT_H
