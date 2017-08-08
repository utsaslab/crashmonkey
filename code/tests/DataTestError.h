#ifndef TESTS_DATA_TEST_ERROR_H
#define TESTS_DATA_TEST_ERROR_H

#include <string>

namespace fs_testing {
namespace tests {

class DataTestError {
 public:
  enum ErrorType {
    kOldFilePersisted,
    kFileMissing,
    kFileDataCorrupted,
    kFileMetadataCorrupted,
    kOther,
  };

  ErrorType error_summary;
  std::string* error_description;
}

std::ostream& operator<<(std::ostream& os, DataTestError::ErrorType err);

}  // namespace tests
}  // namespace fs_testing

#endif  // TESTS_DATA_TEST_ERROR_H
