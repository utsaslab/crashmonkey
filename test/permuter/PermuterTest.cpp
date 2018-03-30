#include <iterator>
#include <vector>

#include "../../code/disk_wrapper_ioctl.h"
#include "../../code/permuter/Permuter.h"
#include "../../code/results/PermuteTestResult.h"
#include "../../code/utils/utils.h"
#include "gtest/gtest.h"

namespace fs_testing {
namespace test {
using std::vector;

using fs_testing::permuter::epoch;
using fs_testing::permuter::epoch_op;
using fs_testing::permuter::EpochOpSector;
using fs_testing::permuter::Permuter;
using fs_testing::utils::disk_write;

class TestPermuter : public Permuter {
 public:
  TestPermuter() {};
  TestPermuter(std::vector<fs_testing::utils::disk_write> *data) {};
  void init_data(vector<epoch> *data) {};
  bool gen_one_state(std::vector<epoch_op>& res,
      PermuteTestResult &log_data) {
    return false;
  }
  bool gen_one_sector_state(std::vector<EpochOpSector>& res,
      PermuteTestResult &log_data) {
    return false;
  }

  vector<EpochOpSector> Coalesce(vector<EpochOpSector> &sectors) {
    return CoalesceSectors(sectors);
  }

  vector<epoch>* GetInternalEpochs() {
    return GetEpochs();
  };
};

/*
 * Good for simple comparisons on the result. Goes through the result epoch and
 * checks that each operation is equal to the corresponding operation found
 * between expected_start and expected_end. This function assumes that the
 * number of operations between expected_start and expected_end is equal to the
 * number of operations in result. Start index is the starting abs_index of the
 * first operation in the epoch. It is assumed that each operation has an index
 * of one higher than the previous operation.
 *
 * This function calls EXPECT_xxxx where necessary to print results out as
 * needed.
 */
static void VerifyEpoch(epoch &result,
    const vector<disk_write>::iterator &expected_start,
    const vector<disk_write>::iterator &expected_end,
    const unsigned int start_index) {
  ASSERT_EQ(result.ops.size(), std::distance(expected_start, expected_end));

  auto current_expected = expected_start;
  for (unsigned int i = 0; i < result.ops.size(); ++i) {
    EXPECT_EQ(result.ops.at(i).abs_index, start_index + i);
    EXPECT_EQ(result.ops.at(i).op.metadata.write_sector,
        current_expected->metadata.write_sector);
    EXPECT_EQ(result.ops.at(i).op.metadata.size,
        current_expected->metadata.size);
    EXPECT_EQ(result.ops.at(i).op.metadata.bi_rw,
        current_expected->metadata.bi_rw);
    ++current_expected;
  }
}

/*
 * Basic test that checks flush operations terminate an epoch and that other
 * operations are properly represented in the epoch vector (i.e. checks size,
 * sector, flags, etc).
 */
TEST(Permuter, InitDataVectorSingleEpochFlushTerminated) {
  const unsigned int num_regular_writes = 9;
  const unsigned int write_size = 1024;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * i;
    write.metadata.size = write_size;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_SYNC_FLAG;
    }
    test_epoch.push_back(write);
  }

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 0;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 1);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Basic test that checks fua operations terminate an epoch and that other
 * operations are properly represented in the epoch vector (i.e. checks size,
 * sector, flags, etc).
 */
TEST(Permuter, InitDataVectorSingleEpochFuaTerminated) {
  const unsigned int num_regular_writes = 9;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * i;
    write.metadata.size = 4096;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_SYNC_FLAG;
    }
    test_epoch.push_back(write);
  }

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FUA_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 0;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 1);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Basic test that makes sure the `has_barrier` flag is only set on epochs that
 * actually have a barrier terminating them.
 */
TEST(Permuter, InitDataVectorSingleEpochUnTerminated) {
  const unsigned int num_regular_writes = 9;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * i;
    write.metadata.size = 4096;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_SYNC_FLAG;
    }
    test_epoch.push_back(write);
  }

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_FALSE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Test that ensure the `overlaps` flag on an epoch is set when a write is
 * contained within another write in the same epoch.
 */
TEST(Permuter, InitDataVectorSingleEpochOverlap) {
  const unsigned int num_regular_writes = 9;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * i;
    write.metadata.size = 4096;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_SYNC_FLAG;
    }
    test_epoch.push_back(write);
  }

  disk_write overlap;
  overlap.metadata.bi_rw = HWM_WRITE_FLAG;
  overlap.metadata.write_sector = 0;
  overlap.metadata.size = 128;
  test_epoch.push_back(overlap);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FUA_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 0;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_TRUE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 2);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Another test for setting the `overlaps` flag on an epoch, this time with one
 * operation only partially overlapping another operation within the same epoch.
 */
TEST(Permuter, InitDataVectorSingleEpochOverlap2) {
  const unsigned int num_regular_writes = 9;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * 10;
    write.metadata.size = 1024;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_SYNC_FLAG;
    }
    test_epoch.push_back(write);
  }

  disk_write overlap;
  overlap.metadata.bi_rw = HWM_WRITE_FLAG;
  overlap.metadata.write_sector = 512;
  overlap.metadata.size = 1024;
  test_epoch.push_back(overlap);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FUA_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 0;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_TRUE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 2);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Test that ensures the `overlap` flag is only set when operations within a
 * single epoch overlap, not when operations from one epoch overlap another
 * epoch. This test creates two flush terminated epochs, both of which write to
 * the same sectors. Neither epoch should have the `overlaps` flag set in this
 * case.
 */
TEST(Permuter, InitDataVectorTwoEpochsFlushTerminatedNoOverlap) {
  const unsigned int num_epochs = 2;
  const unsigned int num_regular_writes = 9;
  vector<disk_write> test_epochs;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epochs.push_back(checkpoint);

  for (unsigned int j = 0; j < num_epochs; ++j) {
    for (unsigned int i = 0; i < num_regular_writes; ++i) {
      disk_write write;
      write.metadata.write_sector = 4096 * i;
      write.metadata.size = 4096;
      write.metadata.bi_rw = HWM_WRITE_FLAG;

      // Make a sync operation.
      if (i % 3 == 0) {
        write.metadata.bi_rw |= HWM_SYNC_FLAG;
      }
      test_epochs.push_back(write);
    }

    disk_write barrier;
    barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
    barrier.metadata.write_sector = 0;
    barrier.metadata.size = 0;
    test_epochs.push_back(barrier);
  }

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epochs);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  for (const auto &epoch : *internal) {
    EXPECT_EQ(epoch.num_meta, 0);
    EXPECT_EQ(epoch.checkpoint_epoch, 0);
    EXPECT_TRUE(epoch.has_barrier);
    EXPECT_FALSE(epoch.overlaps);
    EXPECT_EQ(epoch.ops.size(), num_regular_writes + 1);
  }
  VerifyEpoch(internal->front(), test_epochs.begin() + 1,
      test_epochs.begin() + 11, 1);
  VerifyEpoch(internal->back(), test_epochs.begin() + 11,
      test_epochs.end(), 11);
}

/*
 * Test to ensure that flush operations that also contain data are split into
 * two separate operations across two different epochs. The first operation
 * should consist of only the flags and have no data. This operation will also
 * terminate the previous epoch. The second operation should consist of the
 * flags excluding the flush flag and have the data found in the original
 * operation. This operation will be the first operation in a new epoch.
 *
 * This behavior is based on the definition of the flush flag which only
 * specifies that previous data is persisted, thus making no gaurantees about
 * data within the flush request itself. If the FUA flag is present, then the
 * operation should not be split across two epochs. Likewise, if no data is
 * contained in the operations (i.e. size = 0) then the operation should not be
 * split across two epochs.
 */
TEST(Permuter, InitDataVectorSplitFlush) {
  vector<disk_write> test_epoch;
  const unsigned int write_sector = 512;
  const unsigned int write_size = 1024;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = write_sector;
  barrier.metadata.size = write_size;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);
  EXPECT_EQ(internal->front().ops.front().abs_index, 1);
  EXPECT_EQ(internal->front().ops.front().op.metadata.size, 0);
  EXPECT_EQ(internal->front().ops.front().op.metadata.write_sector,
      write_sector);
  EXPECT_EQ(internal->front().ops.front().op.metadata.bi_rw,
      HWM_FLUSH_FLAG | HWM_WRITE_FLAG);

  EXPECT_EQ(internal->back().num_meta, 0);
  EXPECT_EQ(internal->back().checkpoint_epoch, 0);
  EXPECT_FALSE(internal->back().has_barrier);
  EXPECT_FALSE(internal->back().overlaps);
  EXPECT_EQ(internal->back().ops.size(), 1);
  EXPECT_EQ(internal->back().ops.front().abs_index, 1);
  EXPECT_EQ(internal->back().ops.front().op.metadata.size, write_size);
  EXPECT_EQ(internal->back().ops.front().op.metadata.write_sector,
      write_sector);
  EXPECT_EQ(internal->back().ops.front().op.metadata.bi_rw, HWM_WRITE_FLAG);
}

/*
 * Test to ensure that an operation bearing all of the flush, fua, and write
 * flags (with non-zero size) is not split across two epochs (see comment on
 * InitDataVectorSplitFlush test case for why this split might happen).
 */
TEST(Permuter, InitDataVectorNoSplitFlush) {
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_FUA_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 512;
  barrier.metadata.size = 1024;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Test to ensure that an operation bearing the flush and write flags with a
 * size of zero is not split across two epochs (see comment on
 * InitDataVectorSplitFlush test case for why this split might happen).
 */
TEST(Permuter, InitDataVectorNoSplitFlush2) {
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 512;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);

  VerifyEpoch(internal->front(), test_epoch.begin() + 1, test_epoch.end(), 1);
}

/*
 * Test that ensures the most recently seen checkpoint is updated properly,
 * even if two checkpoints are seen in a row. In the case where multiple
 * checkpoints are seen in a row (or within a single epoch), the checkpoint of
 * that epoch is recorded as the last checkpoint, potentially causing a skip in
 * the checkpoint numbers when looking at all the epochs together.
 */
TEST(Permuter, InitDataVectorCheckpointSkip) {
  const unsigned int write_sector = 512;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  // Another random checkpoint.
  disk_write testing;
  testing.metadata.write_sector = 1;
  testing.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  testing.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  testing.metadata.size = 0;
  testing.metadata.time_ns = 0;
  test_epoch.push_back(testing);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = write_sector;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 1);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);

  EXPECT_EQ(internal->front().ops.front().abs_index, 2);
  EXPECT_EQ(internal->front().ops.front().op.metadata.size, 0);
  EXPECT_EQ(internal->front().ops.front().op.metadata.write_sector,
      write_sector);
  EXPECT_EQ(internal->front().ops.front().op.metadata.bi_rw,
      HWM_FLUSH_FLAG | HWM_WRITE_FLAG);
}

/*
 * Test that nothing bad happens if you have a barrier operation immediately
 * followed by a checkpoint at the very end of the log (i.e. the checkpoint is
 * the last operation in the log). In this case, the checkpoint will be the last
 * thing seen and the final epoch will be empty in the Permuter, but will have
 * an incremented checkpoint number.
 */
TEST(Permuter, InitDataVectorEmptyEndEpoch) {
  const unsigned int write_sector = 512;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = write_sector;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  // Another random checkpoint.
  disk_write testing;
  testing.metadata.write_sector = 1;
  testing.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  testing.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  testing.metadata.size = 0;
  testing.metadata.time_ns = 0;
  test_epoch.push_back(testing);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);

  EXPECT_EQ(internal->front().ops.front().abs_index, 1);
  EXPECT_EQ(internal->front().ops.front().op.metadata.size, 0);
  EXPECT_EQ(internal->front().ops.front().op.metadata.write_sector,
      write_sector);
  EXPECT_EQ(internal->front().ops.front().op.metadata.bi_rw,
      HWM_FLUSH_FLAG | HWM_WRITE_FLAG);

  EXPECT_EQ(internal->back().num_meta, 0);
  EXPECT_EQ(internal->back().checkpoint_epoch, 1);
  EXPECT_FALSE(internal->back().has_barrier);
  EXPECT_FALSE(internal->back().overlaps);
  EXPECT_EQ(internal->back().ops.size(), 0);
}

/*
 * Test to ensure per epoch metadata counts are properly maintained. Operations
 * with the meta flag should be added to the count of metadata operations in an
 * epoch. This includes barrier operations that are split across two epochs,
 * normal barrier operations, and regular wrties.
 */
TEST(Permuter, InitDataVectorCountMetadata) {
  const unsigned int num_regular_writes = 9;
  const unsigned int write_sector = 512;
  const unsigned int write_size = 128;
  vector<disk_write> test_epoch;

  // Create a Checkpoint in the log since all logs start with one.
  disk_write checkpoint;
  checkpoint.metadata.write_sector = 0;
  checkpoint.metadata.bi_flags = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.bi_rw = HWM_CHECKPOINT_FLAG;
  checkpoint.metadata.size = 0;
  checkpoint.metadata.time_ns = 0;
  test_epoch.push_back(checkpoint);

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 4096 * i;
    write.metadata.size = 1024;
    write.metadata.bi_rw = HWM_WRITE_FLAG;

    // Make a meta operation.
    if (i % 3 == 0) {
      write.metadata.bi_rw |= HWM_META_FLAG;
    }
    test_epoch.push_back(write);
  }

  disk_write barrier;
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG | HWM_META_FLAG;
  barrier.metadata.write_sector = write_sector;
  barrier.metadata.size = write_size;
  test_epoch.push_back(barrier);

  disk_write barrier2;
  barrier2.metadata.bi_rw = HWM_FUA_FLAG | HWM_WRITE_FLAG | HWM_META_FLAG;
  barrier2.metadata.write_sector = 0;
  barrier2.metadata.size = 0;
  test_epoch.push_back(barrier2);

  TestPermuter tp;
  const unsigned int sector_size = 512;
  tp.InitDataVector(sector_size, test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  EXPECT_EQ(internal->front().num_meta, 4);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 1);

  EXPECT_EQ(internal->back().num_meta, 2);
  EXPECT_EQ(internal->back().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->back().has_barrier);
  EXPECT_FALSE(internal->back().overlaps);
  EXPECT_EQ(internal->back().ops.size(), 2);
}


/*
 * Test to ensure CoalesceSector when no sectors overlap returns a vector with
 * the original vector's sectors.
 */
TEST(Permuter, CoalesceSectorsNoChange) {
  const unsigned int sector_size = 512;
  vector<disk_write> test_epoch;

  TestPermuter tp;
  tp.InitDataVector(sector_size, test_epoch);

  // CoalesceSector only needs the disk_offset field.
  vector<EpochOpSector> sectors;

  for (unsigned int i = 0; i < 10; ++i) {
    EpochOpSector s(NULL, i, i * sector_size, sector_size, sector_size);
    sectors.push_back(s);
  }

  vector<EpochOpSector> coalesced = tp.Coalesce(sectors);

  EXPECT_EQ(coalesced.size(), sectors.size());
  for (unsigned int i = 0; i < sectors.size(); ++i) {
    EXPECT_EQ(coalesced.at(i), sectors.at(i));
  }
}

/*
 * Test that CoalesceSectors() correctly drops EpochOpSectors that are earlier
 * in the vector if they refer to the same disk_offset as an EpochOpSector later
 * in the vector.
 */
TEST(Permuter, CoalesceSectorDropSector) {
  const unsigned int num_sectors = 20;
  const unsigned int sector_size = 512;
  vector<disk_write> test_epoch;

  TestPermuter tp;
  tp.InitDataVector(sector_size, test_epoch);

  // CoalesceSector only needs the disk_offset field.
  vector<EpochOpSector> sectors(num_sectors);

  for (unsigned int i = 0; i < (num_sectors >> 1); ++i) {
    EpochOpSector s(NULL, i, i * sector_size, sector_size, sector_size);
    EpochOpSector s2(NULL, (num_sectors - 1 - i), i * sector_size, sector_size,
        sector_size);
    sectors.at(i) = s;
    sectors.at(num_sectors - 1 - i) = s2;
  }

  vector<EpochOpSector> coalesced = tp.Coalesce(sectors);

  EXPECT_EQ(coalesced.size(), num_sectors >> 1);
  for (unsigned int i = 0; i < num_sectors >> 1; ++i) {
    EXPECT_EQ(coalesced.at(i), sectors.at((num_sectors >> 1) + i));
  }
}

/*
 * Test that CoalesceSectors() correctly drops EpochOpSectors that are earlier
 * in the vector *only* if they refer to the same disk_offset as an
 * EpochOpSector later in the vector.
 */
TEST(Permuter, CoalesceSectorDropSector2) {
  const unsigned int num_sectors = 20;
  const unsigned int sector_size = 512;
  vector<disk_write> test_epoch;

  TestPermuter tp;
  tp.InitDataVector(sector_size, test_epoch);

  // CoalesceSector only needs the disk_offset field.
  vector<EpochOpSector> sectors(num_sectors + 1);

  EpochOpSector s(NULL, 0, (num_sectors + 5) * sector_size, sector_size,
      sector_size);
  sectors.at(0) = s;
  for (unsigned int i = 0; i < (num_sectors >> 1); ++i) {
    EpochOpSector s(NULL, i + 1, i * sector_size, sector_size, sector_size);
    EpochOpSector s2(NULL, (num_sectors - i), i * sector_size, sector_size,
        sector_size);
    // Skip the first slot.
    sectors.at(i + 1) = s;
    sectors.at(num_sectors - i) = s2;
  }

  vector<EpochOpSector> coalesced = tp.Coalesce(sectors);

  // The 0th element of both vectors should be an EpochOpSector for offset
  // sector_size * (num_sectors + 5). The next (num_sectors / 2) elements in the
  // coalesced vector should be the last (num_sectors / 2) elements of the
  // original vector.
  EXPECT_EQ(coalesced.size(), (num_sectors >> 1) + 1);
  for (unsigned int i = 0; i < num_sectors >> 1; ++i) {
    EXPECT_EQ(coalesced.at(i + 1), sectors.at(((num_sectors >> 1) + 1) + i));
  }
  EXPECT_EQ(coalesced.at(0), sectors.at(0));
}

/*
 * Test that ToSectors() generates the correct number of sectors.
 */
TEST(EpochOp, ToSectorSimple) {
  const unsigned int sector_size = 512;
  const unsigned int num_sectors = 3;

  epoch_op op;
  op.abs_index = 0;
  op.op.metadata.write_sector = 0;
  op.op.metadata.size = num_sectors * sector_size;

  vector<EpochOpSector> sectors = op.ToSectors(sector_size);

  EXPECT_EQ(sectors.size(), num_sectors);
  for (unsigned int i = 0; i < sectors.size(); ++i) {
    EXPECT_EQ(sectors.at(i).parent, &op);
    EXPECT_EQ(sectors.at(i).parent_sector_index, i);
    EXPECT_EQ(sectors.at(i).disk_offset, i * sector_size);
    EXPECT_EQ(sectors.at(i).max_sector_size, sector_size);
    EXPECT_EQ(sectors.at(i).size, sector_size);
  }
}

/*
 * Test that ToSectors() generates the correct number of sectors at the right
 * disk_offsets.
 */
TEST(EpochOp, ToSectorSimple2) {
  const unsigned int sector_size = 512;
  const unsigned int num_sectors = 3;

  epoch_op op;
  op.abs_index = 0;
  op.op.metadata.write_sector = 3;
  op.op.metadata.size = num_sectors * sector_size;

  vector<EpochOpSector> sectors = op.ToSectors(sector_size);

  EXPECT_EQ(sectors.size(), num_sectors);
  for (unsigned int i = 0; i < sectors.size(); ++i) {
    EXPECT_EQ(sectors.at(i).parent, &op);
    EXPECT_EQ(sectors.at(i).parent_sector_index, i);
    EXPECT_EQ(sectors.at(i).max_sector_size, sector_size);
    EXPECT_EQ(sectors.at(i).size, sector_size);
  }
  // To avoid writing the same code that is in Permuter.cpp already, manually
  // check all the disk_offset fields. The offset should be (i + 3) * 512 where
  // i is the index into the vector. Multiply by 512 because both the kernel
  // sector size and EpochOpSector max_sector_size are set to 512.
  EXPECT_EQ(sectors.at(0).disk_offset, 1536);
  EXPECT_EQ(sectors.at(1).disk_offset, 2048);
  EXPECT_EQ(sectors.at(2).disk_offset, 2560);
}

/*
 * Test that ToSectors() generates the correct number of sectors at the right
 * disk_offsets, even if the kernel sector size does not match the
 * max_sector_size for the EpochOpSectors.
 */
TEST(EpochOp, ToSectorUnequalSectorSizes) {
  const unsigned int sector_size = 256;
  const unsigned int num_sectors = 3;

  epoch_op op;
  op.abs_index = 0;
  op.op.metadata.write_sector = 3;
  op.op.metadata.size = num_sectors * sector_size;

  vector<EpochOpSector> sectors = op.ToSectors(sector_size);

  EXPECT_EQ(sectors.size(), num_sectors);
  for (unsigned int i = 0; i < sectors.size(); ++i) {
    EXPECT_EQ(sectors.at(i).parent, &op);
    EXPECT_EQ(sectors.at(i).parent_sector_index, i);
    // The 3 is the offset the sector the bio "went" to times the sector size
    // used in the kernel code.
    EXPECT_EQ(sectors.at(i).max_sector_size, sector_size);
    EXPECT_EQ(sectors.at(i).size, sector_size);
  }
  // To avoid writing the same code that is in Permuter.cpp already, manually
  // check all the disk_offset fields. The offset should be
  // (3 * 512) + (i * 256) where i is the index into the vector. Multiply by 512
  // because the kernel sector size is set to 512.
  EXPECT_EQ(sectors.at(0).disk_offset, 1536);
  EXPECT_EQ(sectors.at(1).disk_offset, 1792);
  EXPECT_EQ(sectors.at(2).disk_offset, 2048);
}

/*
 * Test that even when someone is really annoying and uses a max_sector_size
 * that isn't a factor of the bio size ToSectors() generates the correct number
 * of sectors at the right disk_offsets and they have the proper sizes.
 */
TEST(EpochOp, ToSectorNonMultipleSize) {
  const unsigned int data_size = 10;
  const unsigned int sector_size = 3;

  epoch_op op;
  op.abs_index = 0;
  op.op.metadata.write_sector = 0;
  op.op.metadata.size = data_size;

  vector<EpochOpSector> sectors = op.ToSectors(sector_size);

  EXPECT_EQ(sectors.size(), 4);
  for (unsigned int i = 0; i < sectors.size(); ++i) {
    EXPECT_EQ(sectors.at(i).parent, &op);
    EXPECT_EQ(sectors.at(i).parent_sector_index, i);
    // The 3 is the offset the sector the bio "went" to times the sector size
    // used in the kernel code.
    EXPECT_EQ(sectors.at(i).max_sector_size, sector_size);
  }
  // To avoid writing the same code that is in Permuter.cpp already, manually
  // check all the disk_offset fields. The offset should be i * 3 where i is the
  // index into the vector.
  EXPECT_EQ(sectors.at(0).disk_offset, 0);
  EXPECT_EQ(sectors.at(0).size, 3);
  EXPECT_EQ(sectors.at(1).disk_offset, 3);
  EXPECT_EQ(sectors.at(1).size, 3);
  EXPECT_EQ(sectors.at(2).disk_offset, 6);
  EXPECT_EQ(sectors.at(2).size, 3);
  EXPECT_EQ(sectors.at(3).disk_offset, 9);
  EXPECT_EQ(sectors.at(3).size, 1);
}

}  // namespace test
}  // namespace fs_testing
