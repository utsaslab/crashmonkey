#include "PermuteTestResult.h"

namespace fs_testing {

using std::ostream;
using std::string;
using std::to_string;

ostream& PermuteTestResult::PrintCrashState(ostream& os) const {
  for (unsigned int i = 0; i < crash_state.size() - 1; ++i) {
    os << to_string(crash_state.at(i)) << ", ";
  }
  os << to_string(crash_state.back());
  return os;
}

}  // namespace fs_testing
