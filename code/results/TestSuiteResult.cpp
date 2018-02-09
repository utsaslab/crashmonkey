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

void TestSuiteResult::TallyResult(SingleTestInfo &done, ResultSet &set) {
  switch (done.GetTestResult()) {
    case SingleTestInfo::kPassed:
      ++set.num_passed;
      break;
    case SingleTestInfo::kFsckFixed:
      ++set.num_passed_fixed;
      break;
    case SingleTestInfo::kFsckRequired:
      ++set.fsck_required;
      break;
    case SingleTestInfo::kFailed:
      ++set.num_failed;

      switch (done.data_test.GetError()) {
        case DataTestResult::kOldFilePersisted:
          ++set.old_file_persisted;
          break;
        case DataTestResult::kFileMissing:
          ++set.file_missing;
          break;
        case DataTestResult::kFileDataCorrupted:
          ++set.file_data_corrupted;
          break;
        case DataTestResult::kFileMetadataCorrupted:
          ++set.file_metadata_corrupted;
          break;
        case DataTestResult::kIncorrectBlockCount:
          ++set.incorrect_block_count;
          break;    
        case DataTestResult::kOther:
          ++set.other;
          break;
      }
      break;
  }
}

void TestSuiteResult::TallyReorderingResult(SingleTestInfo &r) {
  TallyResult(r, reordering_results_);
}
void TestSuiteResult::TallyTimingResult(SingleTestInfo &r) {
  TallyResult(r, timing_results_);
}

unsigned int TestSuiteResult::GetReorderingCompleted() const {
  return reordering_results_.num_passed + reordering_results_.num_passed_fixed +
    reordering_results_.fsck_required + reordering_results_.num_failed;
}

unsigned int TestSuiteResult::GetTimingCompleted() const {
  return timing_results_.num_passed + timing_results_.num_passed_fixed +
    timing_results_.fsck_required + timing_results_.num_failed;
}

unsigned int TestSuiteResult::GetCompleted() const {
  return GetTimingCompleted() + GetReorderingCompleted();
}

void TestSuiteResult::PrintResults(ostream& os) const {
  os << "Reordering tests ran " << GetReorderingCompleted() << " tests with" <<
    "\n\tpassed cleanly: " << reordering_results_.num_passed <<
    "\n\tpassed fixed: " << reordering_results_.num_passed_fixed <<
    "\n\tfsck required: " << reordering_results_.fsck_required <<
    "\n\tfailed: " << reordering_results_.num_failed <<
    "\n\t\told file persisted: " << reordering_results_.old_file_persisted <<
    "\n\t\tfile missing: " << reordering_results_.file_missing <<
    "\n\t\tfile data corrupted: " << reordering_results_.file_data_corrupted <<
    "\n\t\tfile metadata corrupted: " <<
      reordering_results_.file_metadata_corrupted <<
    "\n\t\tincorrect block count: " <<
      reordering_results_.incorrect_block_count <<
    "\n\t\tother: " << reordering_results_.other << endl << endl;

  os << "Timing tests ran " << GetTimingCompleted() << " tests with" <<
    "\n\tpassed cleanly: " << timing_results_.num_passed <<
    "\n\tpassed fixed: " << timing_results_.num_passed_fixed <<
    "\n\tfsck required: " << timing_results_.fsck_required <<
    "\n\tfailed: " << timing_results_.num_failed <<
    "\n\t\told file persisted: " << timing_results_.old_file_persisted <<
    "\n\t\tfile missing: " << timing_results_.file_missing <<
    "\n\t\tfile data corrupted: " << timing_results_.file_data_corrupted <<
    "\n\t\tfile metadata corrupted: " <<
      timing_results_.file_metadata_corrupted <<
    "\n\t\tincorrect block count: " <<
      timing_results_.incorrect_block_count <<
    "\n\t\tother: " << timing_results_.other << endl << endl;
}

}  // namespace fs_testing
