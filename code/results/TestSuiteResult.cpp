#include "DataTestResult.h"
#include "FileSystemTestResult.h"
#include "TestSuiteResult.h"

namespace fs_testing {

using std::endl;
using std::ostream;
using std::string;
using std::vector;

using fs_testing::tests::DataTestResult;
using fs_testing::FileSystemTestResult;
using fs_testing::SingleTestInfo;

void TestSuiteResult::TallyResult(SingleTestInfo& done) {
  switch (done.GetTestResult()) {
    case SingleTestInfo::kPassed:
      ++num_passed_;
      break;
    case SingleTestInfo::kFsckFixed:
      ++num_passed_fixed_;
      break;
    case SingleTestInfo::kFsckRequired:
      ++fsck_required_;
      break;
    case SingleTestInfo::kFailed:
      ++num_failed_;

      switch (done.data_test.GetError()) {
        case DataTestResult::kOldFilePersisted:
          ++old_file_persisted_;
          break;
        case DataTestResult::kFileMissing:
          ++file_missing_;
          break;
        case DataTestResult::kFileDataCorrupted:
          ++file_data_corrupted_;
          break;
        case DataTestResult::kFileMetadataCorrupted:
          ++file_metadata_corrupted_;
          break;
        case DataTestResult::kIncorrectBlockCount:
          ++incorrect_block_count_;
          break;    
        case DataTestResult::kOther:
          ++other_;
          break;
      }
      break;
  }
}

unsigned int TestSuiteResult::GetCompleted() const {
  return num_passed_ + num_passed_fixed_ + fsck_required_ + num_failed_;
}

void TestSuiteResult::PrintResults(ostream& os) const {
  os << "Ran " <<
    num_failed_ + num_passed_fixed_ + num_passed_ + fsck_required_ <<
    " tests with" <<
    "\n\tpassed cleanly: " << num_passed_ <<
    "\n\tpassed fixed: " << num_passed_fixed_ <<
    "\n\tfsck required: " << fsck_required_ <<
    "\n\tfailed: " << num_failed_ <<
    "\n\t\told file persisted: " << old_file_persisted_ <<
    "\n\t\tfile missing: " << file_missing_ <<
    "\n\t\tfile data corrupted: " << file_data_corrupted_ <<
    "\n\t\tfile metadata corrupted: " << file_metadata_corrupted_ <<
    "\n\t\tincorrect block count: " << incorrect_block_count_ <<
    "\n\t\tother: " << other_ << endl;
}

}  // namespace fs_testing
