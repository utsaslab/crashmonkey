#ifndef PERMUTER_H
#define PERMUTER_H

#include <list>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../utils/utils.h"
#include "../results/PermuteTestResult.h"

namespace fs_testing {
namespace permuter {

using fs_testing::PermuteTestResult;

struct BioVectorHash {
  std::size_t operator() (const std::vector<unsigned int>& permutation) const;
};

struct BioVectorEqual {
  bool operator() (const std::vector<unsigned int>& a,
      const std::vector<unsigned int>& b) const;
};

struct epoch_op {
  unsigned int abs_index;
  fs_testing::utils::disk_write op;
};

struct epoch {
  unsigned int num_meta;
  unsigned int checkpoint_epoch;
  bool has_barrier;
  bool overlaps;
  std::vector<struct epoch_op> ops;
};


class Permuter {
 public:
  virtual ~Permuter() {};
  void InitDataVector(std::vector<fs_testing::utils::disk_write> &data);
  bool GenerateCrashState(std::vector<fs_testing::utils::disk_write>& res,
      PermuteTestResult &log_data);
  
 protected:
  std::vector<epoch>* GetEpochs();

 private:
  virtual void init_data(std::vector<epoch> *data) = 0;
  virtual bool gen_one_state(std::vector<epoch_op>& res,
      PermuteTestResult &log_data) = 0;
  bool FindOverlapsAndInsert(fs_testing::utils::disk_write &dw,
      std::list<std::pair<unsigned int, unsigned int>> &ranges) const;

  std::vector<epoch> epochs_;
  std::unordered_set<std::vector<unsigned int>, BioVectorHash, BioVectorEqual>
    completed_permutations_;
};

typedef Permuter *permuter_create_t();
typedef void permuter_destroy_t(Permuter *instance);

}  // namespace permuter
}  // namespace fs_testing

#endif  // PERMUTER_H
