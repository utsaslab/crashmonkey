#ifndef PERMUTER_FULL_EPOCH_PERMUTER_H
#define PERMUTER_FULL_EPOCH_PERMUTER_H

#include <random>
#include <vector>

#include "Permuter.h"
#include "../utils/utils.h"
#include "../results/PermuteTestResult.h"

namespace fs_testing {
namespace permuter {

using fs_testing::PermuteTestResult;

class FullEpochPermuter : public Permuter {
 public:
  FullEpochPermuter();
  FullEpochPermuter(std::vector<fs_testing::utils::disk_write> *data);

 private:
  virtual void init_data(std::vector<epoch> *data);
  virtual bool gen_one_state(std::vector<epoch_op>& res,
      PermuteTestResult &log_data);
  void permute_epoch(
      std::vector<epoch_op>::iterator& res_start,
      std::vector<epoch_op>::iterator& res_end, epoch& epoch);

  std::mt19937 rand;
};

}  // namespace permuter
}  // namespace fs_testing

#endif  // PERMUTER_FULL_EPOCH_PERMUTER
