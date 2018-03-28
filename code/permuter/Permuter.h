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

// Declare just so that we can reference it in a function below.
struct EpochOpSector;

struct BioVectorHash {
  std::size_t operator() (const std::vector<unsigned int>& permutation) const;
};

struct BioVectorEqual {
  bool operator() (const std::vector<unsigned int>& a,
      const std::vector<unsigned int>& b) const;
};

struct epoch_op {
  std::vector<EpochOpSector> ToSectors(unsigned int sector_size);
  fs_testing::utils::DiskWriteData ToWriteData();

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

/*
 * Assumes that the starting sector of the epoch_op containing these sectors is
 * aligned to max_sector_size.
 */
struct EpochOpSector {
 public:
  EpochOpSector();
  EpochOpSector(epoch_op *parent, unsigned int parent_sector_index,
      unsigned int disk_offset, unsigned int size,
      unsigned int max_sector_size);
  bool operator==(const EpochOpSector &other) const;
  bool operator!=(const EpochOpSector &other) const;
  void * GetData();
  fs_testing::utils::DiskWriteData ToWriteData();

  epoch_op *parent;
  unsigned int parent_sector_index;
  unsigned int disk_offset;
  unsigned int max_sector_size;
  // Note that this could be less than the given sector size if the sector is
  // the last one for the bio and the sector size is not a multiple of the bio
  // size.
  unsigned int size;
};

class Permuter {
 public:
  virtual ~Permuter() {};
  void InitDataVector(unsigned int sector_size,
      std::vector<fs_testing::utils::disk_write> &data);
  bool GenerateCrashState(std::vector<fs_testing::utils::DiskWriteData> &res,
      fs_testing::PermuteTestResult &log_data);
  bool GenerateSectorCrashState(
      std::vector<fs_testing::utils::DiskWriteData> &res,
      fs_testing::PermuteTestResult &log_data);

 protected:
  std::vector<epoch>* GetEpochs();
  /*
   * Given a vector of sectors ordered in time (i.e. the submission time of a
   * sector at a higher index in the vector is later than the submission time of
   * a sector at a lower index in the vector), remove earlier sectors that write
   * to the same disk address as later sectors.
   */
  std::vector<EpochOpSector> CoalesceSectors(
      std::vector<EpochOpSector> &sector_list);

  unsigned int sector_size_;

 private:
  virtual void init_data(std::vector<epoch> *data) = 0;
  virtual bool gen_one_state(std::vector<epoch_op>& res,
      fs_testing::PermuteTestResult &log_data) = 0;
  virtual bool gen_one_sector_state(
      std::vector<fs_testing::utils::DiskWriteData> &res,
      fs_testing::PermuteTestResult &log_data) = 0;

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
