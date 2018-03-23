#include "PermuteTestResult.h"

namespace fs_testing {

using std::ostream;
using std::to_string;

ostream& PermuteTestResult::PrintCrashStateSize(ostream& os) const {
  if (crash_state.empty() && sector_crash_state.empty()) {
    os << "0 bios";
  } else if (!crash_state.empty()) {
    os << to_string(crash_state.size()) << " bios";
  } else {
    os << to_string(sector_crash_state.size()) << " sectors";
  }
  return os;
}

ostream& PermuteTestResult::PrintCrashState(ostream& os) const {
  if (crash_state.empty() && sector_crash_state.empty()) {
    return os;
  }
  if (!crash_state.empty()) {
    return PrintFullBioCrashState(os);
  } else {
    return PrintSectorCrashState(os);
  }
}

ostream& PermuteTestResult::PrintFullBioCrashState(ostream &os) const {
  for (unsigned int i = 0; i < crash_state.size() - 1; ++i) {
    os << to_string(crash_state.at(i)) << ", ";
  }
  os << to_string(crash_state.back());
  return os;
}

ostream& PermuteTestResult::PrintSectorCrashState(ostream &os) const {
  for (unsigned int i = 0; i < sector_crash_state.size() - 1; ++i) {
    std::pair<unsigned int, unsigned int> sector = sector_crash_state.at(i);
    os << "(" << to_string(sector.first) << ", " << to_string(sector.second) <<
      "), ";
  }
  std::pair<unsigned int, unsigned int> sector = sector_crash_state.back();
  os << "(" << to_string(sector.first) << ", " << sector.second << ")";

  return os;
}

}  // namespace fs_testing
