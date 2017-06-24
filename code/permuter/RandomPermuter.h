#ifndef RANDOM_PERMUTER_H
#define RANDOM_PERMUTER_H

#include <random>
#include <unordered_set>
#include <vector>

#include "Permuter.h"
#include "../utils/utils.h"

namespace fs_testing {
namespace permuter {

struct BioVectorHash {
  std::size_t operator() (const std::vector<unsigned int>& permutation) const {
    unsigned int seed = permutation.size();
    for (const auto& bio_pos : permutation) {
      seed ^= bio_pos + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

struct BioVectorEqual {
  bool operator() (const std::vector<unsigned int>& a,
      const std::vector<unsigned int>& b) const {
    if (a.size() != b.size()) {
      return false;
    }
    for (unsigned int i = 0; i < a.size(); ++i) {
      if (a.at(i) != b.at(i)) {
        return false;
      }
    }
    return true;
  }
};

struct epoch_op {
  unsigned int abs_index;
  fs_testing::utils::disk_write op;
};

struct epoch {
  unsigned int length;
  unsigned int num_meta;
  bool has_barrier;
  bool overlaps;
  std::vector<struct epoch_op> ops;
};

class RandomPermuter : public Permuter {
 public:
  RandomPermuter();
  RandomPermuter(std::vector<fs_testing::utils::disk_write> *data);
  virtual void set_data(std::vector<fs_testing::utils::disk_write> *data);
  virtual bool permute(std::vector<fs_testing::utils::disk_write>& res);

 private:
  void init_data_vector(std::vector<fs_testing::utils::disk_write> *data);
  void permute_epoch(
      std::vector<fs_testing::utils::disk_write>::iterator& res_start,
      std::vector<fs_testing::utils::disk_write>::iterator& res_end,
      std::vector<unsigned int>::iterator& order_start,
      epoch& epoch);
  unsigned int log_length;
  std::vector<struct epoch> epochs;
  std::mt19937 rand;
  std::unordered_set<std::vector<unsigned int>, BioVectorHash, BioVectorEqual>
    completed_permutations;
};

}  // namespace permuter
}  // namespace fs_testing

#endif
