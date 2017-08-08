#include "DataTestError.h"

std::ostream& operator<<(std::ostream& os, DataTestError::ErrorType err) {
  switch (err) {
    case fs_testing::tests::DataTestError::kOldFilePersisted:
      os << "old file persisted";
      break;
    case fs_testing::tests::DataTestError::kFileMissing:
      os << "file missing";
      break;
    case fs_testing::tests::DataTestError::kFileDataCorrupted:
      os << "file data corrupted";
      break;
    case fs_testing::tests::DataTestError::kFileMetadataCorrupted:
      os << "file metadata corrupted";
      break;
    case fs_testing::tests::DataTestError::kOther:
      os << "other error";
      break;
    default:
      os.setstate(std::ios_base::failbit);
  }
  return os;
}
