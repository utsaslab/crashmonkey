#include <cassert>

#include <algorithm>
#include <iterator>
#include <list>
#include <numeric>
#include <vector>

#include "Permuter.h"
#include "RandomPermuter.h"

namespace fs_testing {
namespace permuter {
using std::advance;
using std::iota;
using std::list;
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

using fs_testing::utils::disk_write;
using fs_testing::utils::DiskWriteData;

GenRandom::GenRandom() : rand(mt19937(42)) { }

int GenRandom::operator()(int max) {
  uniform_int_distribution<unsigned int> uid(0, max - 1);
  return uid(rand);
}

RandomPermuter::RandomPermuter(vector<disk_write> *data) {
  // TODO(ashmrtn): Make a flag to make it random or not.
  rand = mt19937(42);
}

void RandomPermuter::init_data(vector<epoch> *data) {
}

bool RandomPermuter::gen_one_state(vector<epoch_op>& res,
    PermuteTestResult &log_data) {
  // Return if there are no ops to permute and generate a crash state
  if (GetEpochs()->size() == 0) {
    return false;
  }
  unsigned int total_elements = 0;
  // Find how many elements we will be returning (randomly determined).
  uniform_int_distribution<unsigned int> permute_epochs(1, GetEpochs()->size());
  unsigned int num_epochs = permute_epochs(rand);
  // Don't subtract 1 from this size so that we can send a complete epoch if we
  // want.
  uniform_int_distribution<unsigned int> permute_requests(1,
      GetEpochs()->at(num_epochs - 1).ops.size());
  unsigned int num_requests = permute_requests(rand);
  // If the last epoch has zero ops, we set num_requests to zero instead of a 
  // garbage value returned by permute_requests(rand)
  if (GetEpochs()->at(num_epochs - 1).ops.size() == 0) {
    num_requests = 0;
  }
  for (unsigned int i = 0; i < num_epochs - 1; ++i) {
    total_elements += GetEpochs()->at(i).ops.size();
  }
  total_elements += num_requests;
  res.resize(total_elements);

  // Tell CrashMonkey the most recently seen checkpoint for the crash state
  // we're generating. We can't just pull the last epoch because it could be the
  // case that there's a checkpoint at the end of this disk write epoch.
  // Therefore, we should determine 1. if we are writing out this entire epoch,
  // and 2. if the checkpoint epoch for this disk write epoch is different than
  // the checkpoint epoch of the disk write epoch before this one (indicating
  // that this disk write epoch changes checkpoint epochs).
  epoch *target = &GetEpochs()->at(num_epochs - 1);
  epoch *prev = NULL;
  if (num_epochs > 1) {
    prev = &GetEpochs()->at(num_epochs - 2);
  }
  if (num_requests != target->ops.size()) {
    log_data.last_checkpoint = (prev) ? prev->checkpoint_epoch : 0;
  } else {
    log_data.last_checkpoint = target->checkpoint_epoch;
  }

  auto curr_iter = res.begin();
  for (unsigned int i = 0; i < num_epochs; ++i) {

    // We donot want to modify epochs prior to the one we are crashing at.
    // We will drop a subset of bios in the epoch we are currently crashing at
    // Only if num_req < ops in the target epoch, we need to pick a subset, else 
    // we'll just copy all the bios in this epoch
    if (i == num_epochs - 1 && num_requests < target->ops.size()) {

      unsigned int size = num_requests;
      auto res_end = curr_iter + size;

      //This should drop a subset of bios instead of permuting them
      subset_epoch(curr_iter, res_end, GetEpochs()->at(i));

      curr_iter = res_end;
    } else {
      // Use a for loop since vector::insert inserts new elements and we
      // resized above to the exact size we will have.
      // We will only ever be placing the full epoch here because the above if
      // will catch the case where we place only part of an epoch.
      for (auto epoch_iter = GetEpochs()->at(i).ops.begin();
          epoch_iter != GetEpochs()->at(i).ops.end(); ++epoch_iter) {
        *curr_iter = *epoch_iter;
        ++curr_iter;
      }
    }
  }

  return true;
}

bool RandomPermuter::gen_one_sector_state(vector<DiskWriteData> &res,
    PermuteTestResult &log_data) {
  res.clear();
  // Return if there are no ops to work with.
  if (GetEpochs()->size() == 0) {
    return false;
  }

  vector<epoch> *epochs = GetEpochs();

  // Pick the point in the sequence we will crash at.
  // Find how many elements we will be returning (randomly determined).
  uniform_int_distribution<unsigned int> permute_epochs(1, epochs->size());
  unsigned int num_epochs = permute_epochs(rand);
  unsigned int num_requests = 0;
  if (!epochs->at(num_epochs - 1).ops.empty()) {
    // It is not valid to call the random number generator with arguments (1, 0)
    // (you get a large number if you do), so skip this if the epoch we crash in
    // has no ops in it.

    // Don't subtract 1 from this size so that we can send a complete epoch if
    // we want.
    uniform_int_distribution<unsigned int> permute_requests(1,
        epochs->at(num_epochs - 1).ops.size());
    num_requests = permute_requests(rand);
  }

  // Tell CrashMonkey the most recently seen checkpoint for the crash state
  // we're generating. We can't just pull the last epoch because it could be the
  // case that there's a checkpoint at the end of this disk write epoch.
  // Therefore, we should determine 1. if we are writing out this entire epoch,
  // and 2. if the checkpoint epoch for this disk write epoch is different than
  // the checkpoint epoch of the disk write epoch before this one (indicating
  // that this disk write epoch changes checkpoint epochs).
  epoch *target = &(epochs->at(num_epochs - 1));
  epoch *prev = NULL;
  if (num_epochs > 1) {
    prev = &epochs->at(num_epochs - 2);
  }
  if (num_requests != target->ops.size()) {
    log_data.last_checkpoint = (prev) ? prev->checkpoint_epoch : 0;
  } else {
    log_data.last_checkpoint = target->checkpoint_epoch;
  }

  // Get and coalesce the sectors of the final epoch we crashed in. We do this
  // here so that we can determine the size of the resulting crash state and
  // allocate that many slots in `res` right off the bat, thus skipping later
  // reallocations as the vector grows.
  vector<EpochOpSector> final_epoch;
  for (unsigned int i = 0; i < num_requests; ++i) {
    vector<EpochOpSector> sectors =
      epochs->at(num_epochs - 1).ops.at(i).ToSectors(sector_size_);
    final_epoch.insert(final_epoch.end(), sectors.begin(), sectors.end());
  }

  unsigned int total_elements = 0;
  for (unsigned int i = 0; i < num_epochs - 1; ++i) {
    total_elements += epochs->at(i).ops.size();
  }

  if (final_epoch.empty()) {
    // No sectors to drop in the final epoch.
    res.resize(total_elements);
    auto epoch_end_iterator = epochs->begin() + num_epochs - 1;
    AddEpochs(res.begin(), res.end(), epochs->begin(), epoch_end_iterator);
    return true;
  } else if (num_requests == epochs->at(num_epochs - 1).ops.size() &&
      epochs->at(num_epochs - 1).ops.back().op.is_barrier()) {
    // We picked the entire epoch and it has a barrier, so we can't rearrange
    // anything.
    res.resize(total_elements + epochs->at(num_epochs - 1).ops.size());
    // + 1 because we also add the full epoch we "crash" in.
    auto epoch_end_iterator = epochs->begin() + num_epochs;
    AddEpochs(res.begin(), res.end(), epochs->begin(), epoch_end_iterator);
    return true;
  }

  // For this branch of execution, we are dropping some sectors from the final
  // epoch we are "crashing" in.

  // Pick a number of sectors to keep. final_epoch.size() > 0 due to if block
  // above, so no need to worry about getting an invalid range.
  uniform_int_distribution<unsigned int> rand_num_sectors(1,
      final_epoch.size());
  const unsigned int num_sectors = rand_num_sectors(rand);

  final_epoch = CoalesceSectors(final_epoch);

  // Result size is now a known quantity.
  res.resize(total_elements + num_sectors);
  // Add the requests not in the final epoch to the result.
  auto epoch_end_iterator = epochs->begin() + (num_epochs - 1);
  auto res_end = res.begin() + total_elements;
  AddEpochs(res.begin(), res_end, epochs->begin(), epoch_end_iterator);

  // Randomly drop some sectors.
  vector<unsigned int> indices(final_epoch.size());
  iota(indices.begin(), indices.end(), 0);
  // Use a known random generator function for repeatability.
  std::random_shuffle(indices.begin(), indices.end(), subset_random_);

  // Populate the bitmap to set req_set number of bios. This is required to keep
  // sectors in temporal order when we generate the crash state.
  vector<unsigned char> sector_bitmap(final_epoch.size());
  for (int i = 0; i < num_sectors; ++i) {
    sector_bitmap[indices[i]] = 1;
  }

  // Add the sectors corresponding to bitmap indexes to the result.
  auto next_index = res.begin() + total_elements;
  for (unsigned int i = 0; i < sector_bitmap.size(); ++i) {
    if (next_index == res.end()) {
      break;
    }
    if (sector_bitmap[i] == 1) {
      *next_index = final_epoch.at(i).ToWriteData();
      ++next_index;
    }
  }

  return true;
}

void RandomPermuter::subset_epoch(
      vector<epoch_op>::iterator &res_start,
      vector<epoch_op>::iterator &res_end,
      epoch &epoch) {

  unsigned int req_size = distance(res_start, res_end);
  assert(req_size <= epoch.ops.size());

  // Even if the number of bios we're placing is less than the number in the
  // epoch, allow any bio but the barrier (if present) to be picked.
  unsigned int slots = epoch.ops.size();
  if (epoch.has_barrier) {
    --slots;
  }

  // Bitmap to indicate which bios in the epoch we want to pick for the crash
  // state.
  vector<unsigned int> epoch_op_bitmap(epoch.ops.size());

  // Fill the list with the empty slots, either [0, epoch.size() - 1] or
  // [0, epoch.size() - 2]. Prefer a list so that removals are fast. We have
  // this so that each time we shuffle the indexes of bios in the epoch, and
  // pick the first req_size number of bios.

  vector<unsigned int> indices(slots);
  iota(indices.begin(), indices.end(), 0);
  // Use a known random generator function for repeatability.
  std::random_shuffle(indices.begin(), indices.end(), subset_random_);

  // Populate the bitmap to set req_set number of bios.
  for (int i = 0; i < req_size; i++) {
    epoch_op_bitmap[indices[i]] = 1;
  }

  // Return the bios corresponding to bitmap indexes.
  for (unsigned int filled = 0;
      filled < epoch_op_bitmap.size() && res_start != res_end; filled++) {
    if (epoch_op_bitmap[filled] == 1) {
      *res_start = epoch.ops.at(filled);
      ++res_start;
    }
  }

  // We are only placing part of an epoch so we need to return here.
  if (res_start == res_end) {
    return;
  }

  assert(epoch.has_barrier);

  // Place the barrier operation if it exists since the entire vector already
  // exists (i.e. we won't cause extra shifting when adding the other elements).
  // Decrement out count of empty slots since we have filled one.
  *res_start = epoch.ops.back();
}

void RandomPermuter::AddEpochs(const vector<DiskWriteData>::iterator &res_start,
    const vector<DiskWriteData>::iterator &res_end,
    const vector<epoch>::iterator &start,
    const vector<epoch>::iterator &end) {
  auto current_res = res_start;
  auto current_epoch = start;

  while (current_epoch != end) {
    for (auto &op : current_epoch->ops) {
      // Check before we do the assignment so that this won't be run on the
      // final time through the loop causing us to fail for no reason since we
      // already incremented res_start to the end.
      assert(current_res != res_end);
      *current_res = op.ToWriteData();
      ++current_res;
    }
    ++current_epoch;
  }

  // We should fill up the entire range that we were given.
  assert(current_res == res_end);
}

}  // namespace permuter
}  // namespace fs_testing

extern "C" fs_testing::permuter::Permuter* permuter_get_instance(
    std::vector<fs_testing::utils::disk_write> *data) {
  return new fs_testing::permuter::RandomPermuter(data);
}

extern "C" void permuter_delete_instance(fs_testing::permuter::Permuter* p) {
  delete p;
}
