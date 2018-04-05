#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../code/user_tools/api/wrapper.h"
#include "../../code/utils/DiskMod.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace fs_testing {
namespace test {

using std::pair;
using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;

using fs_testing::user_tools::api::CmFsOps;
using fs_testing::user_tools::api::FsFns;
using fs_testing::utils::DiskMod;

using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;


namespace {

static constexpr char kTestData[] = "abcdefghijklmnopqrstuvwxyz012345";
static const unsigned int kTestDataSize = 32;  // Tied to length of above.

}  // namespace


class FakeFsFns : public FsFns {
 public:
  FakeFsFns() : call_num_(0) { }

  virtual int FnMknod(const std::string &pathname, mode_t mode,
      dev_t dev) override {
    return 0;
	}
  virtual int FnMkdir(const std::string &pathname, mode_t mode) override {
    return 0;
	}
  virtual int FnOpen(const std::string &pathname, int flags) override {
    return 0;
	}
  virtual int FnOpen2(const std::string &pathname, int flags,
      mode_t mode) override {
    return 0;
	}
  virtual off_t FnLseek(int fd, off_t offset, int whence) override {
    return 0;
	}
  virtual ssize_t FnWrite(int fd, const void *buf, size_t count) override {
    return 0;
	}
  virtual ssize_t FnPwrite(int fd, const void *buf, size_t count,
      off_t offset) override {
    return 0;
	}
  virtual void * FnMmap(void *addr, size_t length, int prot, int flags, int fd,
      off_t offset) override {
    return nullptr;
	}
  virtual int FnMsync(void *addr, size_t length, int flags) override {
    return 0;
	}
  virtual int FnMunmap(void *addr, size_t length) override {
    return 0;
	}
  virtual int FnFallocate(int fd, int mode, off_t offset, off_t len) override {
    return 0;
	}
  virtual int FnClose(int fd) override {
    return 0;
	}
  virtual int FnUnlink(const std::string &pathname) override {
    return 0;
	}
  virtual int FnRemove(const std::string &pathname) override {
    return 0;
	}

  virtual int FnStat(const string &pathname, struct stat *buf) override {
    // Clear buffer since the user may have left junk in it.
    memset(buf, 0, sizeof(struct stat));

    // If we haven't already used all the results we were given, output those.
    if (call_num_ < file_sizes.size()) {
      buf->st_size = file_sizes.at(call_num_);
      ++call_num_;
    }

    return 0;
  }

  virtual bool FnPathExists(const std::string &pathname) override {
    return 0;
	}

  virtual int CmCheckpoint() override {
    return 0;
	}

  // Used to restore values that should be placed in the struct stat. Used as a
  // FIFO queue.
  vector<unsigned int> file_sizes;

 private:
  // Used to index into vector(s) to get the proper result values.
  unsigned int call_num_;
};

class MockFsFns : public FsFns {
 public:
  MOCK_METHOD3(FnMknod, int(const std::string &pathname, mode_t mode,
        dev_t dev));
  MOCK_METHOD2(FnMkdir, int(const std::string &pathname, mode_t mode));
  MOCK_METHOD2(FnOpen, int(const std::string &pathname, int flags));
  MOCK_METHOD3(FnOpen2, int(const std::string &pathname, int flags,
        mode_t mode));
  MOCK_METHOD3(FnLseek, off_t(int fd, off_t offset, int whence));
  MOCK_METHOD3(FnWrite, ssize_t(int fd, const void *buf, size_t count));
  MOCK_METHOD4(FnPwrite, ssize_t(int fd, const void *buf, size_t count,
      off_t offset));
  MOCK_METHOD6(FnMmap, void *(void *addr, size_t length, int prot, int flags,
        int fd, off_t offset));
  MOCK_METHOD3(FnMsync, int(void *addr, size_t length, int flags));
  MOCK_METHOD2(FnMunmap, int(void *addr, size_t length));
  MOCK_METHOD4(FnFallocate, int(int fd, int mode, off_t offset, off_t len));
  MOCK_METHOD1(FnClose, int(int fd));
  MOCK_METHOD1(FnUnlink, int(const std::string &pathname));
  MOCK_METHOD1(FnRemove, int(const std::string &pathname));

  MOCK_METHOD2(FnStat, int(const std::string &pathname, struct stat *buf));
  MOCK_METHOD1(FnPathExists, bool(const std::string &pathname));

  MOCK_METHOD0(CmCheckpoint, int());

  void DelegateToFake() {
    ON_CALL(*this, FnStat(::testing::_, NotNull()))
      .WillByDefault(Invoke(&fake, &FakeFsFns::FnStat));
  }

  FakeFsFns fake;
};

class TestCmFsOps : public CmFsOps {
 public:
  TestCmFsOps(FsFns *functions) : CmFsOps(functions) { }

  vector<DiskMod> * GetMods() {
    return &mods_;
  }

  unordered_map<int, string> * GetFdMap() {
    return &fd_map_;
  }

  unordered_map<int, string> * GetMmapMap() {
    return &mmap_map_;
  }

  /*
   * Add a fake fd->pathname mapping so the code can use this for things like
   * stat operations.
   */
  void AddFdMapping(const int fd, const string &pathname) {
    fd_map_.insert({fd, pathname});
  }
};

// For parameterized tests.
class TestCmFsOpsParameterized : public ::testing::TestWithParam<unsigned int> {
};



/*
 * Test that opening a file that exists with the truncate flag causes
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenTrunc) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_TRUNC | O_RDONLY;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(true));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, NotNull()));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd), pathname);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMetadataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kTruncateOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that opening a file that does not exist with the truncate flag does not
 * result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenTruncNoFile) {
  const string pathname = "/mnt/snapshot/bleh";
  const int flags = O_TRUNC | O_RDONLY;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(false));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(-1));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, -1);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_TRUE(fd_map->empty());

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_TRUE(mods->empty());
}

/*
 * Test that opening a file that exists with the O_CREAT flag does result in
 *    - the file path and returned descriptor to be added to the map
 *
 * but does not result in
 *    - a DiskMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatExists) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(true));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);
  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd), pathname);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_TRUE(mods->empty());
}

/*
 * Test that opening a file that does not exist with the O_CREAT flag does
 * result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatNew) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(false));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, NotNull()));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd), pathname);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kCreateMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that opening a file that does not exist with the O_CREAT and O_TRUNC
 * flags does result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod of type kCreateMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatTruncNew) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_TRUNC | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(false));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, NotNull()));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd), pathname);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kCreateMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that opening a file that exists with the O_CREAT and O_TRUNC flags does
 * result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod of type kDataMetadataMod to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatTrunc) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_TRUNC | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(true));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, NotNull()));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd), pathname);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMetadataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kTruncateOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that closing a valid file that was previously opened results in
 *    - the file path and returned descriptor being removed from the map
 */
TEST(CmFsOps, CloseGoodFd) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;

  MockFsFns mock;
  TestCmFsOps ops(&mock);
  ops.AddFdMapping(expected_fd, pathname);

  EXPECT_CALL(mock, FnClose(expected_fd)).WillOnce(Return(0));

  const int close_res = ops.CmClose(expected_fd);
  EXPECT_EQ(close_res, 0);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_TRUE(fd_map->empty());

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_TRUE(mods->empty());
}

/*
 * Test that closing an invalid file does not result in
 *    - a segfault or other crash
 */
TEST(CmFsOps, CloseBadFd) {
  const unsigned int expected_fd = 1;

  MockFsFns mock;
  TestCmFsOps ops(&mock);

  EXPECT_CALL(mock, FnClose(expected_fd)).WillOnce(Return(-1));

  const int close_res = ops.CmClose(expected_fd);
  EXPECT_EQ(close_res, -1);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_TRUE(fd_map->empty());

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_TRUE(mods->empty());
}

/*
 * Test that running a checkpoint that succeeds results in
 *    - a DiskMod of type kCheckpointMod to be placed in the list of mods
 */
TEST(CmFsOps, CheckpointGood) {
  MockFsFns mock;
  TestCmFsOps ops(&mock);

  EXPECT_CALL(mock, CmCheckpoint()).WillOnce(Return(0));

  const int checkpoint_res = ops.CmCheckpoint();
  EXPECT_EQ(checkpoint_res, 0);

  unordered_map<int, string> *fd_map = ops.GetFdMap();
  EXPECT_TRUE(fd_map->empty());

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kCheckpointMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
}

/*
 * Test that writing to a file where the write extends the file size results in
 *    - a DiskMod of type kDataMetadataMod to be placed in the list of mods
 *    - the right offset and length in the DiskMod
 *    - the right data in the disk mod
 */
TEST_P(TestCmFsOpsParameterized, WriteFileExtendZeroOffset) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const unsigned int write_size = GetParam();

  MockFsFns mock;
  mock.DelegateToFake();

  mock.fake.file_sizes.emplace_back(0);
  mock.fake.file_sizes.emplace_back(write_size);

  EXPECT_CALL(mock, FnLseek(expected_fd, 0, SEEK_CUR)).WillOnce(Return(0));
  EXPECT_CALL(mock, FnWrite(expected_fd, kTestData, kTestDataSize))
    .WillOnce(Return(write_size));
  EXPECT_CALL(mock, FnStat(pathname, NotNull())).Times(2);

  TestCmFsOps ops(&mock);
  ops.AddFdMapping(expected_fd, pathname);

  ops.CmWrite(expected_fd, kTestData, kTestDataSize);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMetadataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
  EXPECT_FALSE(mods->at(0).directory_mod);
  EXPECT_EQ(mods->at(0).file_mod_location, 0);
  EXPECT_EQ(mods->at(0).file_mod_len, write_size);
  EXPECT_FALSE(strncmp(mods->at(0).file_mod_data.get(), kTestData, write_size));
}

/*
 * Test that writing to a file where the write extends the file size results in
 *    - a DiskMod of type kDataMod to be placed in the list of mods
 *    - the right offset and length in the DiskMod
 *    - the right data in the disk mod
 */
TEST_P(TestCmFsOpsParameterized, WriteFileNoExtendZeroOffset) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const unsigned int write_size = GetParam();

  MockFsFns mock;
  mock.DelegateToFake();

  mock.fake.file_sizes.emplace_back(kTestDataSize + (kTestDataSize >> 1));
  mock.fake.file_sizes.emplace_back(kTestDataSize + (kTestDataSize >> 1));

  EXPECT_CALL(mock, FnLseek(expected_fd, 0, SEEK_CUR)).WillOnce(Return(0));
  EXPECT_CALL(mock, FnWrite(expected_fd, kTestData, kTestDataSize))
    .WillOnce(Return(write_size));
  EXPECT_CALL(mock, FnStat(pathname, NotNull())).Times(2);

  TestCmFsOps ops(&mock);
  ops.AddFdMapping(expected_fd, pathname);

  ops.CmWrite(expected_fd, kTestData, kTestDataSize);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
  EXPECT_FALSE(mods->at(0).directory_mod);
  EXPECT_EQ(mods->at(0).file_mod_location, 0);
  EXPECT_EQ(mods->at(0).file_mod_len, write_size);
  EXPECT_FALSE(strncmp(mods->at(0).file_mod_data.get(), kTestData, write_size));
}

/*
 * Test that writing to a file where the write extends the file size results in
 *    - a DiskMod of type kDataMetadataMod to be placed in the list of mods
 *    - the right offset and length in the DiskMod
 *    - the right data in the disk mod
 */
TEST_P(TestCmFsOpsParameterized, WriteFileExtendNonZeroOffset) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const unsigned int write_size = GetParam();
  const unsigned int start_offset = kTestDataSize + (kTestDataSize >> 1) - 1;

  MockFsFns mock;
  mock.DelegateToFake();

  mock.fake.file_sizes.emplace_back(start_offset);
  mock.fake.file_sizes.emplace_back(start_offset + write_size);

  EXPECT_CALL(mock, FnLseek(expected_fd, 0, SEEK_CUR))
    .WillOnce(Return(start_offset));
  EXPECT_CALL(mock, FnWrite(expected_fd, kTestData, kTestDataSize))
    .WillOnce(Return(write_size));
  EXPECT_CALL(mock, FnStat(pathname, NotNull())).Times(2);

  TestCmFsOps ops(&mock);
  ops.AddFdMapping(expected_fd, pathname);

  ops.CmWrite(expected_fd, kTestData, kTestDataSize);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMetadataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
  EXPECT_FALSE(mods->at(0).directory_mod);
  EXPECT_EQ(mods->at(0).file_mod_location, start_offset);
  EXPECT_EQ(mods->at(0).file_mod_len, write_size);
  EXPECT_FALSE(strncmp(mods->at(0).file_mod_data.get(), kTestData, write_size));
}

/*
 * Test that writing to a file where the write extends the file size results in
 *    - a DiskMod of type kDataMod to be placed in the list of mods
 *    - the right offset and length in the DiskMod
 *    - the right data in the disk mod
 */
TEST_P(TestCmFsOpsParameterized, WriteFileNoExtendNonZeroOffset) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const unsigned int write_size = GetParam();
  const unsigned int start_offset = kTestDataSize + (kTestDataSize >> 1) - 1;

  MockFsFns mock;
  mock.DelegateToFake();

  mock.fake.file_sizes.emplace_back(kTestDataSize << 1);
  mock.fake.file_sizes.emplace_back(kTestDataSize << 1);

  EXPECT_CALL(mock, FnLseek(expected_fd, 0, SEEK_CUR))
    .WillOnce(Return(start_offset));
  EXPECT_CALL(mock, FnWrite(expected_fd, kTestData, kTestDataSize))
    .WillOnce(Return(write_size));
  EXPECT_CALL(mock, FnStat(pathname, NotNull())).Times(2);

  TestCmFsOps ops(&mock);
  ops.AddFdMapping(expected_fd, pathname);

  ops.CmWrite(expected_fd, kTestData, kTestDataSize);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::kDataMod);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(mods->at(0).path, pathname);
  EXPECT_FALSE(mods->at(0).directory_mod);
  EXPECT_EQ(mods->at(0).file_mod_location, start_offset);
  EXPECT_EQ(mods->at(0).file_mod_len, write_size);
  EXPECT_FALSE(strncmp(mods->at(0).file_mod_data.get(), kTestData, write_size));
}

INSTANTIATE_TEST_CASE_P(WriteSizes, TestCmFsOpsParameterized,
    ::testing::Values(
      kTestDataSize,
      kTestDataSize >> 1,
      (kTestDataSize >> 1) - 1));

}  // namespace test
}  // namespace fs_testing

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
