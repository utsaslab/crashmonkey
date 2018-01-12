#include <iterator>
#include <list>
#include <numeric>
#include <vector>

#include <cassert>

#include "Permuter.h"
#include "RandomSubsetPermuter.h"
#include "../utils/utils.h"

namespace fs_testing {
namespace permuter {
using std::advance;
using std::iota;
using std::list;
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

using fs_testing::utils::disk_write;

RandomSubsetPermuter::RandomSubsetPermuter() {
  rand = mt19937(42);
}

RandomSubsetPermuter::RandomSubsetPermuter(vector<disk_write> *data) {
  // TODO(ashmrtn): Make a flag to make it random or not.
  rand = mt19937(42);
}

void RandomSubsetPermuter::init_data(vector<epoch> *data) {
}


bool RandomSubsetPermuter::gen_one_state(vector<epoch_op>& res,
    PermuteTestResult &log_data) {
  //std::cout << "In gen_one_state" << std::endl;
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
  log_data.crash_state.resize(total_elements);
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
    if (GetEpochs()->at(i).overlaps || i == num_epochs - 1) {
      unsigned int size =
        (i != num_epochs - 1) ? GetEpochs()->at(i).ops.size() : num_requests;
      auto res_end = curr_iter + size;
      //std::cout << "Size selected = " << size<< "\tNum req = "<< num_requests << "\t epoch size = " << GetEpochs()->at(i).ops.size() << std::endl;
      auto res_end_old = res_end;
      permute_epoch(curr_iter, res_end, GetEpochs()->at(i));
      //std::cout << "New distance of end= " << distance(res_end_old, res_end) << std::endl;

      //we have dropped this epoch
      if(distance(res_end_old, res_end) < 0){//distance(res_end, res_end_old)
        res.resize(total_elements - distance(res_end, res_end_old));
        log_data.crash_state.resize(total_elements - distance(res_end, res_end_old));
        //std::cout << "Resized to " << total_elements - distance(res_end, res_end_old) << std::endl;
      }

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

  // Messy bit to add everything to the logging data struct.
  for (unsigned int i = 0; i < res.size(); ++i) {
    log_data.crash_state.at(i) = res.at(i).abs_index;
  }
  return true;
}

void RandomSubsetPermuter::permute_epoch(
      vector<epoch_op>::iterator& res_start,
      vector<epoch_op>::iterator& res_end,
      epoch& epoch) {
  int probable_drop = 0;
  int journal_start_bio = 0;
  int bios_to_exclude = 0;
  int required_size = distance(res_start, res_end);
  int size_to_fill = required_size;
  //std::cout << "size_to_fill ="<< size_to_fill <<std::endl;
  assert(required_size <= epoch.ops.size());

  // Even if the number of bios we're placing is less than the number in the
  // epoch, allow any bio but the barrier (if present) to be picked.
  unsigned int slots = epoch.ops.size();
  if (epoch.has_barrier) {
    //if there is a barrier - flush and if its journal, don't reorder
    for(int i = 0; i < slots ; i++){
        if(epoch.ops[i].op.is_journal()){
        probable_drop = 1;
        journal_start_bio = i;
        break; 
      }
    }

    if(probable_drop == 0)
      goto reduce_slot;

    if(required_size < slots){
      //std::cout << "Drop full journal bio" << std::endl;
      bios_to_exclude = required_size - journal_start_bio;
      //std::cout << "j start = "<< journal_start_bio << " exclude = "<< bios_to_exclude << std::endl;
      if(bios_to_exclude > 0)
        res_end = res_end - bios_to_exclude;

      size_to_fill = (bios_to_exclude > 0)? journal_start_bio : required_size;
    }

    else{
      //std::cout << "No permute journal epoch. Write " << slots <<" bios." << std::endl;
      for(int i = journal_start_bio; i < required_size; i++){
        *(res_start+i) = epoch.ops.at(i);
        //++res_start;
      }
      size_to_fill = journal_start_bio;
    }
    slots = journal_start_bio;
    goto next;

    reduce_slot:
      --slots;
      --size_to_fill;
      //std::cout << "Slot reduced" << std::endl;
  }


  

  // Fill the list with the empty slots, either [0, epoch.size() - 1] or
  // [0, epoch.size() - 2]. Prefer a list so that removals are fast. We have
  // this so that each time we pick a number we can find a bio which we haven't
  // already placed.
  next:
  list<unsigned int> empty_slots(slots);

  //make this of required size = either dist(res_end, res_sstrat) or journal_start or required_size
  list<unsigned int> bios_to_pick;
  //std::cout << "size to fill = " << size_to_fill << std::endl;

  //std::cout <<"bios to pick = " << bios_to_pick.size() << std::endl;
  iota(empty_slots.begin(), empty_slots.end(), 0);
  //iota(bios_to_pick.begin(), bios_to_pick.end(), 0);

  // First case is when we are placing a subset of the bios, the second is when
  // we are placing all the bios but a barrier operation is present.
  while(size_to_fill != 0){
    uniform_int_distribution<unsigned int> uid(0, empty_slots.size() - 1 );
    auto shift = empty_slots.begin(); 
    unsigned int val = uid(rand);
    advance(shift, val);
    bios_to_pick.push_back(*shift);
    empty_slots.erase(shift);
    size_to_fill--;
  }
  //std::cout << "DOne filling bios" << std::endl;
  bios_to_pick.sort();

  //for(auto it = bios_to_pick.begin(); it!= bios_to_pick.end(); it++)
   // std::cout << *it << std::endl;
  //std::cout << "bios to pick now =" << bios_to_pick.size()<<std::endl;

  while (res_start != res_end && !bios_to_pick.empty()) {
    // Uniform distribution includes both ends, so we need to subtract 1 from
    // the size.
    auto iter = bios_to_pick.begin();
    *res_start = epoch.ops.at(bios_to_pick.front());
    //std::cout << "Bio filled = " << bios_to_pick.front() << "value =" << epoch.ops.at(bios_to_pick.front()).abs_index<< std::endl ;
    ++res_start;
    bios_to_pick.pop_front();
    //if(probable_drop && bios_to_exclude < 0 && (slots - bios_to_pick.size() == required_size))
    //  break;
  }

  // We are only placing part of an epoch so we need to return here.
  if (res_start == res_end) {
    return;
  }

  assert(epoch.has_barrier);

  // Place the barrier operation if it exists since the entire vector already
  // exists (i.e. we won't cause extra shifting when adding the other elements).
  // Decrement out count of empty slots since we have filled one.
  if(!probable_drop)
    *res_start = epoch.ops.back();
}




}  // namespace permuter
}  // namespace fs_testing

extern "C" fs_testing::permuter::Permuter* permuter_get_instance(
    std::vector<fs_testing::utils::disk_write> *data) {
  return new fs_testing::permuter::RandomSubsetPermuter(data);
}

extern "C" void permuter_delete_instance(fs_testing::permuter::Permuter* p) {
  delete p;
}
