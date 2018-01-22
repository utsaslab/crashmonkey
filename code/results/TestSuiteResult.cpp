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

void TestSuiteResult::AddCompletedTest(const SingleTestInfo& done) {
  completed_.push_back(done);
}

unsigned int TestSuiteResult::GetCompleted() const {
  return completed_.size();
}

bool TestSuiteResult::SingleTestResult(const SingleTestInfo& test_info) {
  if (test_info.fs_test.GetError() == FileSystemTestResult::kClean
      && test_info.data_test.GetError() == DataTestResult::kClean) {
    return true;
  }
  else if (test_info.fs_test.GetError() == FileSystemTestResult::kFixed
      && test_info.data_test.GetError() == DataTestResult::kClean) {
    return true;
  }
  else if (test_info.fs_test.GetError() == FileSystemTestResult::kKernelMount
    && test_info.data_test.GetError() == DataTestResult::kClean) {
    return true;
  }
  else {
    return false;
  }
}

void TestSuiteResult::RetrieveFailedTests(std::vector<int>& failed_tests) {
  for (int i = 0; i < completed_.size(); i++) {
    bool ret = SingleTestResult(completed_[i]);
    if (!ret) {
      failed_tests.push_back(i);
    }
  }
}

void TestSuiteResult::PrintResults(ostream& os, bool is_log) const {
  unsigned int num_failed = 0;
  unsigned int num_passed_fixed = 0;
  unsigned int num_passed = 0;
  unsigned int total_tests = 0;

  unsigned int fsck_required = 0;
  unsigned int old_file_persisted = 0;
  unsigned int file_missing = 0;
  unsigned int file_data_corrupted = 0;
  unsigned int file_metadata_corrupted = 0;
  unsigned int incorrect_block_count = 0;
  unsigned int other = 0;

  for (const auto& result : completed_) {

    string test_status("");
    string des("");
    total_tests++;
    if (result.fs_test.GetError() == FileSystemTestResult::kClean
        && result.data_test.GetError() == DataTestResult::kClean) {
      ++num_passed;
      test_status = "PASSED";
    } else if (result.fs_test.GetError() == FileSystemTestResult::kFixed
        && result.data_test.GetError() == DataTestResult::kClean) {
      ++num_passed_fixed;
      test_status = "FSCK_FIXED";
    } else if (result.fs_test.GetError() ==
        FileSystemTestResult::kKernelMount &&
        result.data_test.GetError() == DataTestResult::kClean) {
      ++fsck_required;
    } else {
      test_status = "FAILED";
      ++num_failed;
      switch (result.data_test.GetError()) {
        case DataTestResult::kOldFilePersisted:
          des += "old file persisted: ";
          ++old_file_persisted;
          break;
        case DataTestResult::kFileMissing:
          des += "file missing: ";
          ++file_missing;
          break;
        case DataTestResult::kFileDataCorrupted:
          des += "file data corrupted: ";
          ++file_data_corrupted;
          break;
        case DataTestResult::kFileMetadataCorrupted:
          des += "file metadata corrupted: ";
          ++file_metadata_corrupted;
          break;
        case DataTestResult::kIncorrectBlockCount:
          des += "incorrect block count: ";
          ++incorrect_block_count;
          break;    
        case DataTestResult::kOther:
          des += "other: ";
          ++other;
          break;
      }
    }
    if (is_log) {
      os << "Test #" << total_tests << ": " << test_status << ": " << des
        << result.data_test.error_description << endl;
      os << "\tlast checkpoint: " << result.permute_data.last_checkpoint
        << endl;
      os << "\tcrash state: ";
      result.permute_data.PrintCrashState(os) << endl;
    }
  }

  os << "Ran " << num_failed + num_passed_fixed + num_passed << " tests with"
    << "\n\tpassed cleanly: " << num_passed
    << "\n\tpassed fixed: " << num_passed_fixed + fsck_required
    << "\n\t\tfsck required: " << fsck_required
    << "\n\t\tfsck fixed: " << num_passed_fixed
    << "\n\tfailed: " << num_failed
    << "\n\t\told file persisted: " << old_file_persisted
    << "\n\t\tfile missing: " << file_missing
    << "\n\t\tfile data corrupted: " << file_data_corrupted
    << "\n\t\tfile metadata corrupted: " << file_metadata_corrupted
    << "\n\t\tincorrect block count: " << incorrect_block_count    
    << "\n\t\tother: " << other << endl;
}

}  // namespace fs_testing
