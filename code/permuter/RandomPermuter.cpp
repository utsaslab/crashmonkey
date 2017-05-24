#include <iterator>
#include <list>
#include <numeric>
#include <vector>

#include <cassert>

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

void RandomPermuter::init_data_vector(vector<disk_write> *data) {
  epochs.clear();
  log_length = data->size();
  unsigned int index = 0;
  while (index < data->size()) {
    struct epoch current_epoch;
    current_epoch.has_barrier = false;
    current_epoch.length = 0;

    // Get all ops in this epoch and add them to either the sync_op or async_op
    // lists.
    while (index < data->size() && !(data->at(index)).is_barrier_write()) {
      ++current_epoch.length;
      current_epoch.ops.push_back(data->at(index));
      current_epoch.num_meta += data->at(index).is_meta();
      ++index;
    }

    // Check is the op at the current index is a "barrier." If it is then add it
    // to the special spot in the epoch, otherwise just push the current epoch
    // onto the list and move to the next segment of the log.
    if (index < data->size() && (data->at(index)).is_barrier_write()) {
      ++current_epoch.length;
      current_epoch.ops.push_back(data->at(index));
      current_epoch.num_meta += data->at(index).is_meta();
      current_epoch.has_barrier = true;
      ++index;
    }
    epochs.push_back(current_epoch);
  }
}

RandomPermuter::RandomPermuter() {
  log_length = 0;
  rand = mt19937(42);
}

RandomPermuter::RandomPermuter(vector<disk_write> *data) {
  init_data_vector(data);
  // TODO(ashmrtn): Make a flag to make it random or not.
  rand = mt19937(42);
}

void RandomPermuter::set_data(vector<disk_write> *data) {
  init_data_vector(data);
}

bool RandomPermuter::permute(vector<disk_write>& res) {
  unsigned int total_elements = 0;
  // Find how many elements we will be returning (randomly determined).
  uniform_int_distribution<unsigned int> permute_epochs(1, epochs.size());
  unsigned int num_epochs = permute_epochs(rand);
  // Don't subtract 1 from this size so that we can send a complete epoch if we
  // want.
  uniform_int_distribution<unsigned int> permute_requests(0,
      epochs.at(num_epochs - 1).length);
  unsigned int num_requests = permute_requests(rand);
  // TODO(ashmrtn): Optimize so that we resize to exactly the number of elements
  // we will have, not the number of elements in all the epochs.
  for (unsigned int i = 0; i < num_epochs; ++i) {
    total_elements += epochs.at(i).length;
  }
  res.resize(total_elements);

  int current_index = 0;
  for (unsigned int i = 0; i < num_epochs; ++i) {
    if (epochs.at(i).overlaps) {
      permute_epoch(res, current_index, epochs.at(i));
    } else {
      auto ins = res.begin();
      advance(ins, current_index);
      res.insert(ins, epochs.at(i).ops.begin(), epochs.at(i).ops.end());
    }
    current_index += epochs.at(i).length;
  }
  // Trim the last epoch to a random number of requests.
  auto cutoff = res.begin();
  // Move the iterator to the start of the epoch we're trimming.
  advance(cutoff, current_index - epochs.at(num_epochs - 1).length);
  advance(cutoff, num_requests);
  // Trim off all requests at or after the one we selected above.
  res.erase(cutoff, res.end());

  return true;
}

void RandomPermuter::permute_epoch(vector<disk_write>& res,
    const int start_index, epoch& epoch) {
  unsigned int slots = epoch.length;

  // Place the barrier operation if it exists since the entire vector already
  // exists (i.e. we won't cause extra shifting when adding the other elements).
  // Decrement out count of empty slots since we have filled one.
  if (epoch.has_barrier) {
    res.at(start_index + slots - 1) = epoch.ops.back();
    --slots;
  }

  // Fill the list with the empty slots, either [0, epoch.length - 1) or
  // [0, epoch.length - 2).
  list<unsigned int> empty_slots(slots);
  iota(empty_slots.begin(), empty_slots.end(), 0);

  for (int i = 0; i < slots; ++i) {
    // Uniform distribution includes both ends, so we need to subtract 1 from
    // the size.
    uniform_int_distribution<unsigned int> uid(0, empty_slots.size() - 1);
    auto shift = empty_slots.begin();
    advance(shift, uid(rand));
    res.at(start_index + *shift) = epoch.ops.at(i);
    empty_slots.erase(shift);
  }

  assert(empty_slots.empty());
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
