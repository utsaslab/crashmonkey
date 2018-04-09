#include "PermuteTestResult.h"

namespace fs_testing {

using std::ostream;
using std::to_string;

ostream& PermuteTestResult::PrintCrashStateSize(ostream& os) const {
  if (crash_state.empty()) {
    os << "0 bios/sectors";
  } else {
    os << to_string(crash_state.size()) << " bios/sectors";
  }
  return os;
}

ostream& PermuteTestResult::PrintCrashState(ostream& os) const {
  if (crash_state.empty()) {
    return os;
  }

  for (unsigned int i = 0; i < crash_state.size() - 1; ++i) {
    os << "(" << to_string(crash_state.at(i).bio_index);
    if (!crash_state.at(i).full_bio) {
      os << ", " << to_string(crash_state.at(i).bio_sector_index);
    }
    os << "), ";
  }

  os << "(" << to_string(crash_state.back().bio_index);
  if (!crash_state.back().full_bio) {
    os << ", " << to_string(crash_state.back().bio_sector_index);
  }
  os << ")";

  return os;
}

}  // namespace fs_testing
