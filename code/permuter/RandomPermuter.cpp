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

namespace {

struct range {
  unsigned int start;
  unsigned int end;
};

}  // namespace

void RandomPermuter::init_data_vector(vector<disk_write> *data) {
  epochs.clear();
  log_length = data->size();
  unsigned int index = 0;
  list<range> overlaps;
  while (index < data->size()) {
    struct epoch current_epoch;
    current_epoch.has_barrier = false;
    current_epoch.overlaps = false;
    current_epoch.length = 0;

    // Get all ops in this epoch and add them to either the sync_op or async_op
    // lists.
    while (index < data->size() && !(data->at(index)).is_barrier_write()) {
      disk_write curr = data->at(index);
      // Find overlapping ranges.
      unsigned int start = curr.metadata.write_sector;
      unsigned int end = start + curr.metadata.size;
      for (auto range_iter = overlaps.begin(); range_iter != overlaps.end();
          range_iter++) {
        range r = *range_iter;
        if ((r.start <= start && r.end >= start)
            || (r.start <= end && r.end >= end)) {
          if (r.start > start) {
            r.start = start;
          }
          if (r.end < end) {
            r.end = end;
          }
          current_epoch.overlaps = true;
          break;
        } else if (r.start > end) {
          // Since this is an ordered list, if the next spot is past where we're
          // looking now then we won't find anything past here. We may as well
          // insert an item here.
          range new_range = {start, end};
          overlaps.insert(range_iter, new_range);
        }
      }

      ++current_epoch.length;
      epoch_op curr_op = {index, data->at(index)};
      current_epoch.ops.push_back(curr_op);
      current_epoch.num_meta += data->at(index).is_meta();
      ++index;
    }

    // Check is the op at the current index is a "barrier." If it is then add it
    // to the special spot in the epoch, otherwise just push the current epoch
    // onto the list and move to the next segment of the log.
    if (index < data->size() && (data->at(index)).is_barrier_write()) {
      ++current_epoch.length;
      epoch_op curr_op = {index, data->at(index)};
      current_epoch.ops.push_back(curr_op);
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

void RandomPermuter::init_data(vector<disk_write> *data) {
  init_data_vector(data);
}

bool RandomPermuter::gen_one_state(vector<disk_write>& res) {
  vector<unsigned int> permuted_epoch(0);
  do {
    unsigned int total_elements = 0;
    // Find how many elements we will be returning (randomly determined).
    uniform_int_distribution<unsigned int> permute_epochs(1, epochs.size());
    unsigned int num_epochs = permute_epochs(rand);
    // Don't subtract 1 from this size so that we can send a complete epoch if we
    // want.
    uniform_int_distribution<unsigned int> permute_requests(1,
        epochs.at(num_epochs - 1).length);
    unsigned int num_requests = permute_requests(rand);
    for (unsigned int i = 0; i < num_epochs - 1; ++i) {
      total_elements += epochs.at(i).length;
    }
    total_elements += num_requests;
    res.resize(total_elements);
    // Add all of the elements as some of the epochs could have been reordered
    // if they overlapped.
    permuted_epoch.resize(total_elements);

    auto curr_iter = res.begin();
    auto permute_iter = permuted_epoch.begin();
    for (unsigned int i = 0; i < num_epochs; ++i) {
      if (epochs.at(i).overlaps || i == num_epochs - 1) {
        unsigned int size =
          (i != num_epochs - 1) ? epochs.at(i).length : num_requests;
        auto res_end = curr_iter + size;
        permute_epoch(curr_iter, res_end, permute_iter, epochs.at(i));

        curr_iter = res_end;
        advance(permute_iter, size);
      } else {
        // Use a for loop since vector::insert inserts new elements and we
        // resized above to the exact size we will have.
        // We will only ever be placing the full epoch here because the above if
        // will catch the case where we place only part of an epoch.
        for (auto epoch_iter = epochs.at(i).ops.begin();
            epoch_iter != epochs.at(i).ops.end(); ++epoch_iter) {
          *curr_iter = epoch_iter->op;
          ++curr_iter;
          *permute_iter = epoch_iter->abs_index;
          ++permute_iter;
        }
      }
    }
  } while (completed_permutations.count(permuted_epoch));
  completed_permutations.insert(permuted_epoch);

  return true;
}

void RandomPermuter::permute_epoch(
      vector<disk_write>::iterator& res_start,
      vector<disk_write>::iterator& res_end,
      vector<unsigned int>::iterator& order_start,
      epoch& epoch) {
  assert(distance(res_start, res_end) <= epoch.length);

  // Even if the number of bios we're placing is less than the number in the
  // epoch, allow any bio but the barrier (if present) to be picked.
  unsigned int slots = (epoch.has_barrier) ? epoch.length - 1 : epoch.length;

  // Fill the list with the empty slots, either [0, epoch.length - 1] or
  // [0, epoch.length - 2]. Prefer a list so that removals are fast. We have
  // this so that each time we pick a number we can find a bio which we haven't
  // already placed.
  list<unsigned int> empty_slots(slots);
  iota(empty_slots.begin(), empty_slots.end(), 0);

  // First case is when we are placing a subset of the bios, the second is when
  // we are placing all the bios but a barrier operation is present.
  while (res_start != res_end && !empty_slots.empty()) {
    // Uniform distribution includes both ends, so we need to subtract 1 from
    // the size.
    uniform_int_distribution<unsigned int> uid(0, empty_slots.size() - 1);
    auto shift = empty_slots.begin();
    advance(shift, uid(rand));
    *res_start = epoch.ops.at(*shift).op;
    ++res_start;
    *order_start = epoch.ops.at(*shift).abs_index;
    ++order_start;
    empty_slots.erase(shift);
  }

  // We are only placing part of an epoch so we need to return here.
  if (res_start == res_end) {
    return;
  }

  assert(epoch.has_barrier);

  // Place the barrier operation if it exists since the entire vector already
  // exists (i.e. we won't cause extra shifting when adding the other elements).
  // Decrement out count of empty slots since we have filled one.
  *res_start = epoch.ops.back().op;
  *order_start = epoch.ops.back().abs_index;
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
