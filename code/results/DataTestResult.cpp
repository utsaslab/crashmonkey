#include "DataTestResult.h"

namespace fs_testing {
namespace tests {

using std::ostream;

DataTestResult::DataTestResult() {
  error_summary_ = DataTestResult::kClean;
}

void DataTestResult::ResetError() {
  error_summary_ = DataTestResult::kClean;
}

void DataTestResult::SetError(ErrorType err) {
  error_summary_ = err;
}

DataTestResult::ErrorType DataTestResult::GetError() const {
  return error_summary_;
}

ostream& DataTestResult::PrintErrors(ostream& os) const {
  unsigned int noted_errors = error_summary_;
  unsigned int shift = 0;
  while (noted_errors != 0) {
    if (noted_errors & 1) {
      os << (DataTestResult::ErrorType) (1 << shift);
    }
    ++shift;
    noted_errors >>= 1;
  }

  return os;
}

ostream& operator<<(ostream& os, DataTestResult::ErrorType err) {
  switch (err) {
    case DataTestResult::kClean:
      break;
    case DataTestResult::kOldFilePersisted:
      os << "old_file_persisted";
      break;
    case DataTestResult::kFileMissing:
      os << "file_missing";
      break;
    case DataTestResult::kFileDataCorrupted:
      os << "file_data_corrupted";
      break;
    case DataTestResult::kFileMetadataCorrupted:
      os << "file_metadata_corrupted";
      break;
    case DataTestResult::kIncorrectBlockCount:
      os << "incorrect_block_count";
      break;      
    case DataTestResult::kOther:
      os << "other_error";
      break;
    case DataTestResult::kAutoCheckFailed:
      os << "auto_check_test_failed";
      break;
    default:
      os.setstate(std::ios_base::failbit);
  }
  return os;
}

}  // namespace tests
}  // namespace fs_testing
