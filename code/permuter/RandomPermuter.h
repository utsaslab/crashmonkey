#ifndef RANDOM_PERMUTER_H
#define RANDOM_PERMUTER_H

#include <random>
#include <vector>

#include "Permuter.h"
#include "../utils/utils.h"
#include "../results/PermuteTestResult.h"

namespace fs_testing {
namespace permuter {

using fs_testing::PermuteTestResult;

class GenRandom {
 public:
  GenRandom();
  int operator()(int max);

 private:
  std::mt19937 rand;
};

class RandomPermuter : public Permuter {
 public:
  RandomPermuter();
  RandomPermuter(std::vector<fs_testing::utils::disk_write> *data);

 private:
  virtual void init_data(std::vector<epoch> *data);
  virtual bool gen_one_state(std::vector<epoch_op>& res,
      PermuteTestResult &log_data);
  virtual bool gen_one_sector_state(
      std::vector<fs_testing::utils::DiskWriteData> &res,
      PermuteTestResult &log_data) override;

  void subset_epoch(
      std::vector<epoch_op>::iterator &res_start,
      std::vector<epoch_op>::iterator &res_end, epoch &epoch);
  /*
   * Add the operations in the epoch_ops contained in the epochs [start, end).
   */
  void AddEpochs(
      const std::vector<fs_testing::utils::DiskWriteData>::iterator &res_start,
      const std::vector<fs_testing::utils::DiskWriteData>::iterator &res_end,
      const std::vector<epoch>::iterator &start,
      const std::vector<epoch>::iterator &end);

  std::mt19937 rand;
  GenRandom subset_random_;
};

}  // namespace permuter
}  // namespace fs_testing

#endif
