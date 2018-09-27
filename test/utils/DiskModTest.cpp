#include <endian.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "../../code/utils/DiskMod.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"


#include <iostream>
#include <ios>

namespace fs_testing {
namespace test {

using std::shared_ptr;
using std::string;

using fs_testing::utils::DiskMod;

namespace {

static constexpr char kTestData[] = "abcdefghijklmnopqrstuvwxyz012345";
static const unsigned int kTestDataSize = 32;  // Tied to length of above.

}  // namespace

// For parameterized tests.
class TestDiskModParameterized : public ::testing::TestWithParam<string> {
};

class TestDiskModParameterizedOpts :
  public ::testing::TestWithParam<DiskMod::ModOpts> {
};

class TestDiskModParameterizedFalloc :
  public ::testing::TestWithParam<std::pair<DiskMod::ModType, DiskMod::ModOpts>>
{
};

/*
 * Test that serializing a kCheckpointMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kCheckpointMod
 *      DiskMod
 */
TEST(DiskMod, SerializeDeserializeCheckpoint) {
  DiskMod start;

  start.mod_type = DiskMod::kCheckpointMod;

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, sizeof(uint64_t) + (2 * sizeof(uint16_t)));
  EXPECT_EQ(mod_type2, DiskMod::kCheckpointMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);
  EXPECT_EQ(new_mod.mod_type, DiskMod::kCheckpointMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_TRUE(new_mod.path.empty());
}

/*
 * Test that serializing a kFsyncMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kFsyncMod
 *      DiskMod
 */
TEST(DiskMod, SerializeDeserializeFsync) {
  const string path = "/mnt/snapshot/bleh";
  DiskMod start;

  start.mod_type = DiskMod::kFsyncMod;
  start.path = path;

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, sizeof(uint64_t) + (2 * sizeof(uint16_t)) + path.size() + 2);
  EXPECT_EQ(mod_type2, DiskMod::kFsyncMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);
  EXPECT_EQ(new_mod.mod_type, DiskMod::kFsyncMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), path.c_str());
}

/*
 * Test that serializing a kSyncMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kSyncMod
 *      DiskMod
 */
TEST(DiskMod, SerializeDeserializeSync) {
  DiskMod start;

  start.mod_type = DiskMod::kSyncMod;

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, sizeof(uint64_t) + (2 * sizeof(uint16_t)));
  EXPECT_EQ(mod_type2, DiskMod::kSyncMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);
  EXPECT_EQ(new_mod.mod_type, DiskMod::kSyncMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_TRUE(new_mod.path.empty());
}

/*
 * Test that serializing a kSyncFileRangeMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kSyncFileRangeMod
 *      DiskMod
 */
TEST(DiskMod, SerializeDeserializeSyncFileRange) {
  const string path = "/mnt/snapshot/bleh";
  DiskMod start;

  start.mod_type = DiskMod::kSyncFileRangeMod;
  start.path = path;

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, (3 * sizeof(uint64_t)) + (2 * sizeof(uint16_t)) +
      path.size() + 2);
  EXPECT_EQ(mod_type2, DiskMod::kSyncFileRangeMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);
  EXPECT_EQ(new_mod.mod_type, DiskMod::kSyncFileRangeMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), path.c_str());
}

/*
 * Test that serializing a kCreateMod DiskMod that is for a directory
 * (directory_mod == true) results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kCreateMod
 *      DiskMod for a directory
 */
TEST(DiskMod, SerializeDeserializeCreateDirectory) {
  const string path = "/mnt/snapshot/bleh";
  DiskMod start;

  start.mod_type = DiskMod::kCreateMod;
  start.mod_opts = DiskMod::kNoneOpt;
  start.path = path;
  start.directory_mod = true;

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, sizeof(uint64_t) + (2 * sizeof(uint16_t)) + path.size() + 2);
  EXPECT_EQ(mod_type2, DiskMod::kCreateMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);
  EXPECT_EQ(new_mod.mod_type, DiskMod::kCreateMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_TRUE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), path.c_str());
}

/*
 * Test that serializing a kDataMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kDataMod
 *      DiskMod
 */
TEST_P(TestDiskModParameterized, SerializeDeserializeFileDataMod) {
  const string mod_path = GetParam();
  DiskMod start;

  start.mod_type = DiskMod::kDataMod;
  start.path = mod_path;
  start.file_mod_len = kTestDataSize;
  start.file_mod_location = 0;
  start.file_mod_data.reset(new char[kTestDataSize], [](char *c) {delete[] c;});
  memcpy(start.file_mod_data.get(), kTestData, kTestDataSize);

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, (3 * sizeof(uint64_t)) + (2 * sizeof(uint16_t)) +
      kTestDataSize + 1 + mod_path.size() + 1);
  EXPECT_EQ(mod_type2, DiskMod::kDataMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);

  EXPECT_EQ(new_mod.mod_type, DiskMod::kDataMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, kTestDataSize);
  EXPECT_EQ(strncmp(new_mod.file_mod_data.get(), kTestData, kTestDataSize), 0);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), mod_path.c_str());
}

/*
 * Test that serializing a kDataMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kDataMod
 *      DiskMod
 */
TEST_P(TestDiskModParameterized, SerializeDeserializeFileDataModNoData) {
  const string mod_path = GetParam();
  DiskMod start;

  start.mod_type = DiskMod::kDataMod;
  start.path = mod_path;
  start.file_mod_len = 0;
  start.file_mod_location = 0;
  start.file_mod_data.reset();

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, (3 * sizeof(uint64_t)) + (2 * sizeof(uint16_t)) + 1 +
      mod_path.size() + 1);
  EXPECT_EQ(mod_type2, DiskMod::kDataMod);
  EXPECT_EQ(mod_opts2, DiskMod::kNoneOpt);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);

  EXPECT_EQ(new_mod.mod_type, DiskMod::kDataMod);
  EXPECT_EQ(new_mod.mod_opts, DiskMod::kNoneOpt);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), mod_path.c_str());
}

/*
 * Test that serializing a kDataMod DiskMod results in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid kDataMod
 *      DiskMod
 */
TEST_P(TestDiskModParameterizedOpts,
    SerializeDeserializeOpts) {
  const string mod_path = "/mnt/snapshot/blah";
  const DiskMod::ModOpts opts = GetParam();
  DiskMod start;

  start.mod_type = DiskMod::kDataMod;
  start.mod_opts = opts;
  start.path = mod_path;
  start.file_mod_len = 0;
  start.file_mod_location = 0;
  start.file_mod_data.reset();

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, (3 * sizeof(uint64_t)) + (2 * sizeof(uint16_t)) + 1 +
      mod_path.size() + 1);
  EXPECT_EQ(mod_type2, DiskMod::kDataMod);
  EXPECT_EQ(mod_opts2, opts);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);

  EXPECT_EQ(new_mod.mod_type, DiskMod::kDataMod);
  EXPECT_EQ(new_mod.mod_opts, opts);
  EXPECT_EQ(new_mod.file_mod_location, 0);
  EXPECT_EQ(new_mod.file_mod_len, 0);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), mod_path.c_str());
}

/*
 * Test that serializing DiskMods that represent fallocate(2) calls result in
 *    - the proper serialized buffer
 *    - the serialized buffer can be turned back into a valid DiskMod
 *      representing a fallocate(2) call
 */
TEST_P(TestDiskModParameterizedFalloc, SerializeDeserializeFalloc) {
  const string mod_path = "/mnt/snapshot/bleh";
  const DiskMod::ModType type = GetParam().first;
  const DiskMod::ModOpts opts = GetParam().second;
  const unsigned int mod_offset = 0;
  const unsigned int mod_len = 4096;
  DiskMod start;

  start.mod_type = type;
  start.mod_opts = opts;
  start.path = mod_path;
  start.file_mod_len = mod_len;
  start.file_mod_location = mod_offset;
  start.file_mod_data.reset();

  shared_ptr<char> serialized = DiskMod::Serialize(start, nullptr);
  char *buf = serialized.get();

  uint64_t size;
  memcpy(&size, buf, sizeof(uint64_t));
  const uint64_t size2 = be64toh(size);
  buf += sizeof(uint64_t);

  uint16_t mod_type;
  memcpy(&mod_type, buf, sizeof(uint16_t));
  const uint16_t mod_type2 = be16toh(mod_type);
  buf += sizeof(uint16_t);

  uint16_t mod_opts;
  memcpy(&mod_opts, buf, sizeof(uint16_t));
  const uint16_t mod_opts2 = be16toh(mod_opts);

  EXPECT_EQ(size2, (3 * sizeof(uint64_t)) + (2 * sizeof(uint16_t)) + 1 +
      mod_path.size() + 1);
  EXPECT_EQ(mod_type2, type);
  EXPECT_EQ(mod_opts2, opts);

  DiskMod new_mod;

  DiskMod::Deserialize(serialized, new_mod);

  EXPECT_EQ(new_mod.mod_type, type);
  EXPECT_EQ(new_mod.mod_opts, opts);
  EXPECT_EQ(new_mod.file_mod_location, mod_offset);
  EXPECT_EQ(new_mod.file_mod_len, mod_len);
  EXPECT_EQ(new_mod.file_mod_data.get(), nullptr);
  EXPECT_FALSE(new_mod.directory_mod);
  EXPECT_TRUE(new_mod.directory_added_entry.empty());
  EXPECT_STREQ(new_mod.path.c_str(), mod_path.c_str());
}

// Test with a file path that is larger than the tmp buffer used in
// DiskMod::Deserialize.
INSTANTIATE_TEST_CASE_P(PathNames, TestDiskModParameterized,
    ::testing::Values(
      "/mnt/snapshot/bleh",
      "/mnt/snapshot/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
          "/mnt/snapshot/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
          "/mnt/snapshot/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));

INSTANTIATE_TEST_CASE_P(PathNames, TestDiskModParameterizedOpts,
    ::testing::Values(
      DiskMod::kNoneOpt,
      DiskMod::kTruncateOpt,
      DiskMod::kFallocateOpt,
      DiskMod::kFallocateKeepSizeOpt,
      DiskMod::kPunchHoleKeepSizeOpt,
      DiskMod::kCollapseRangeOpt,
      DiskMod::kZeroRangeOpt,
      DiskMod::kZeroRangeKeepSizeOpt,
      DiskMod::kInsertRangeOpt));

INSTANTIATE_TEST_CASE_P(Falloc, TestDiskModParameterizedFalloc,
    ::testing::Values(
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kFallocateOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kFallocateKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kPunchHoleKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kCollapseRangeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kZeroRangeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kZeroRangeKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMod, DiskMod::kInsertRangeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMetadataMod, DiskMod::kFallocateOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMetadataMod, DiskMod::kFallocateKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
        DiskMod::kDataMetadataMod, DiskMod::kPunchHoleKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
          DiskMod::kDataMetadataMod, DiskMod::kCollapseRangeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
          DiskMod::kDataMetadataMod, DiskMod::kZeroRangeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
          DiskMod::kDataMetadataMod, DiskMod::kZeroRangeKeepSizeOpt),
      std::pair<DiskMod::ModType, DiskMod::ModOpts>(
          DiskMod::kDataMetadataMod, DiskMod::kInsertRangeOpt)));

}  // namespace test
}  // namespace fs_testing
