#include "DataTestResult.h"

namespace fs_testing {
namespace tests {

using std::initializer_list;
using std::ostream;

void DataTestSummary::ResetError() {
  error_summary_ = 0;
}

void DataTestResult::SetError(initializer_list<ErrorType> errors) {
  for (const auto& err : errors) {
    error_summary_ |= err;
  }
}

ostream& PrintErrors(ostream& os) {
  unsigned int noted_errors = error_summary_;
  unsigned int shift = 0;
  while (noted_errors != 0) {
    if (noted_errors & 1) {
      os << (ErrorType) (1 << shift);
    }
    ++shift;
    noted_errors >>= 1;
  }

  return os;
}

ostream& operator<<(ostream& os, ErrorType err) {
  switch (err) {
    case fs_testing::tests::DataTestResult::kOldFilePersisted:
      os << "old_file_persisted";
      break;
    case fs_testing::tests::DataTestResult::kFileMissing:
      os << "file_missing";
      break;
    case fs_testing::tests::DataTestResult::kFileDataCorrupted:
      os << "file_data_corrupted";
      break;
    case fs_testing::tests::DataTestResult::kFileMetadataCorrupted:
      os << "file_metadata_corrupted";
      break;
    case fs_testing::tests::DataTestResult::kOther:
      os << "other_error";
      break;
    default:
      os.setstate(std::ios_base::failbit);
  }
  return os;
}

}  // namespace tests
}  // namespace fs_testing
