#include <errno.h>
#include <sys/mman.h>

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

using ::testing::Return;



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
};

class TestCmFsOps : public CmFsOps {
 public:
  TestCmFsOps(FsFns *functions) : CmFsOps(functions) { }

  vector<DiskMod> * GetMods() {
    return &mods_;
  }

  unordered_map<int, pair<string, unsigned int>> * GetFdMap() {
    return &fd_map_;
  }

  unordered_map<int, string> * GetMmapMap() {
    return &mmap_map_;
  }

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
  EXPECT_CALL(mock, FnStat(pathname, ::testing::_));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd).first, pathname);
  EXPECT_EQ(fd_map->at(fd).second, 0);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::DATA_METADATA_MOD);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::TRUNCATE);
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

  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
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
  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd).first, pathname);
  EXPECT_EQ(fd_map->at(fd).second, 0);

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
  EXPECT_CALL(mock, FnStat(pathname, ::testing::_));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd).first, pathname);
  EXPECT_EQ(fd_map->at(fd).second, 0);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::CREATE);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::NONE);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that opening a file that does not exist with the O_CREAT and O_TRUNC
 * flags does result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod of type CREATE to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatTruncNew) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_TRUNC | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(false));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, ::testing::_));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd).first, pathname);
  EXPECT_EQ(fd_map->at(fd).second, 0);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::CREATE);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::NONE);
  EXPECT_EQ(mods->at(0).path, pathname);
}

/*
 * Test that opening a file that exists with the O_CREAT and O_TRUNC flags does
 * result in
 *    - the file path and returned descriptor to be added to the map
 *    - a DiskMod of type DATA_METADATA_MOD to be placed in the list of mods.
 */
TEST(CmFsOps, OpenCreatTrunc) {
  const string pathname = "/mnt/snapshot/bleh";
  const unsigned int expected_fd = 1;
  const int flags = O_CREAT | O_TRUNC | O_RDWR;

  MockFsFns mock;
  EXPECT_CALL(mock, FnPathExists(pathname.c_str())).WillOnce(Return(true));
  EXPECT_CALL(mock, FnOpen(pathname, flags)).WillOnce(Return(expected_fd));
  EXPECT_CALL(mock, FnStat(pathname, ::testing::_));

  TestCmFsOps ops(&mock);

  const int fd = ops.CmOpen(pathname, flags);
  EXPECT_EQ(fd, expected_fd);

  unordered_map<int, pair<string, unsigned int>> *fd_map = ops.GetFdMap();
  EXPECT_EQ(fd_map->size(), 1);
  EXPECT_EQ(fd_map->at(fd).first, pathname);
  EXPECT_EQ(fd_map->at(fd).second, 0);

  vector<DiskMod> *mods = ops.GetMods();
  EXPECT_EQ(mods->size(), 1);
  EXPECT_EQ(mods->at(0).mod_type, DiskMod::DATA_METADATA_MOD);
  EXPECT_EQ(mods->at(0).mod_opts, DiskMod::TRUNCATE);
  EXPECT_EQ(mods->at(0).path, pathname);
}


}  // namespace test
}  // namespace fs_testing

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
