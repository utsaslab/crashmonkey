#include <endian.h>
#include <string.h>

#include <memory>
#include <string>

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
      DiskMod::kPunchHoleOpt,
      DiskMod::kCollapseRangeOpt,
      DiskMod::kZeroRangeOpt,
      DiskMod::kInsertRangeOpt));

}  // namespace test
}  // namespace fs_testing
