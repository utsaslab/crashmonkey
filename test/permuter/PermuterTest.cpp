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
using fs_testing::permuter::Permuter;
using fs_testing::utils::disk_write;

class TestPermuter : public Permuter {
 public:
  TestPermuter() {};
  TestPermuter(std::vector<fs_testing::utils::disk_write> *data) {};
  void init_data(vector<epoch> *data) {};
  bool gen_one_state(std::vector<epoch_op>& res,
      PermuteTestResult &log_data) {
    return GetEpochs();
  }

  vector<epoch>* GetInternalEpochs() {
    return GetEpochs();
  };
};

TEST(Permuter, InitDataVectorSingleEpochFlushTerminated) {
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
  barrier.metadata.bi_rw = HWM_FLUSH_FLAG | HWM_WRITE_FLAG;
  barrier.metadata.write_sector = 0;
  barrier.metadata.size = 0;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 1);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 1);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_FALSE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_TRUE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 2);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_TRUE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), num_regular_writes + 2);
}

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
  tp.InitDataVector(test_epochs);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  for (const auto &epoch : *internal) {
    EXPECT_EQ(epoch.num_meta, 0);
    EXPECT_EQ(epoch.checkpoint_epoch, 0);
    EXPECT_TRUE(epoch.has_barrier);
    EXPECT_FALSE(epoch.overlaps);
    EXPECT_EQ(epoch.ops.size(), num_regular_writes + 1);
  }
}

TEST(Permuter, InitDataVectorSplitFlush) {
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
  barrier.metadata.size = 1024;
  test_epoch.push_back(barrier);

  TestPermuter tp;
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 2);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);

  EXPECT_EQ(internal->back().num_meta, 0);
  EXPECT_EQ(internal->back().checkpoint_epoch, 0);
  EXPECT_FALSE(internal->back().has_barrier);
  EXPECT_FALSE(internal->back().overlaps);
  EXPECT_EQ(internal->back().ops.size(), 1);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);
}

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
  tp.InitDataVector(test_epoch);
  vector<epoch> *internal = tp.GetInternalEpochs();

  EXPECT_EQ(internal->size(), 1);

  EXPECT_EQ(internal->front().num_meta, 0);
  EXPECT_EQ(internal->front().checkpoint_epoch, 0);
  EXPECT_TRUE(internal->front().has_barrier);
  EXPECT_FALSE(internal->front().overlaps);
  EXPECT_EQ(internal->front().ops.size(), 1);
}
}  // namespace test
}  // namespace fs_testing
