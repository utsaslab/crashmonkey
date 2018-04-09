#include "DiskMod.h"

#include <assert.h>
#include <endian.h>
#include <string.h>

namespace fs_testing {
namespace utils {

using std::shared_ptr;
using std::vector;

uint64_t DiskMod::GetSerializeSize() {
  // mod_type, mod_opts, and a uint64_t for the size of the serialized mod.
  uint64_t res = (2 * sizeof(uint16_t)) + sizeof(uint64_t);

  if (mod_type == DiskMod::kCheckpointMod) {
    return res;
  }

  res += path.size() + 1;  // size() doesn't include null terminator.
  res += sizeof(bool);  // directory_mod.
  if (directory_mod) {
    res += directory_added_entry.size() + 1;  // Path changed in directory.
  } else {
    // Data changed, location of change, length of change.
    res += 2 * sizeof(uint64_t) + file_mod_len;
  }

  return res;
}

/*
 * The basic serialized format is as follows:
 *    * uint64_t size of following region in bytes
 *    * uint16_t mod_type
 *    * uint16_t mod_opts
 *    ~~~~~~~~~~~~~~~~~~~~    <-- End of entry if kCheckpointMod.
 *    * null-terminated string for path the mod refers to (ex. file path)
 *    * 1-byte directory_mod boolean
 *    ~~~~~~~~~~~~~~~~~~~~    <-- End of ChangeHeader function data.
 *    * uint64_t file_mod_location
 *    * uint64_t file_mod_len
 *    * <file_mod_len>-bytes of file mod data
 *
 * The final three lines of this layout are specific only to modifications on
 * files. Modifications to directories are not yet supported, though there are
 * some structures that may be used to help track them.
 *
 * All multi-byte data fields in the serialized format use big endian encoding.
 */

/*
 * Make things miserable on myself, and only store either the directory or the
 * file fields based on the value of directory_mod.
 *
 * Convert everything to big endian for the sake of reading dumps in a
 * consistent manner if need be.
 */
shared_ptr<char> DiskMod::Serialize(DiskMod &dm, unsigned long long *size) {
  // Standard code to serialize the front part of the DiskMod.
  // Get a block large enough for this DiskMod.
  const uint64_t mod_size = dm.GetSerializeSize();
  if (size != nullptr) {
    *size = mod_size;
  }
  // TODO(ashmrtn): May want to split this if it is very large.
  shared_ptr<char> res_ptr(new (std::nothrow) char[mod_size],
      [](char *c) {delete[] c;});
  char *buf = res_ptr.get();
  if (buf == nullptr) {
    return res_ptr;
  }

  const uint64_t mod_size_be = htobe64(mod_size);
  memcpy(buf, &mod_size_be, sizeof(uint64_t));
  unsigned int buf_offset = sizeof(uint64_t);

  int res = SerializeHeader(buf, buf_offset, dm);
  if (res < 0) {
    return shared_ptr<char>(nullptr);
  }
  buf_offset += res;

  // kCheckpointMod doesn't need anything done after the type.
  if (dm.mod_type != DiskMod::kCheckpointMod) {
    res = SerializeChangeHeader(buf, buf_offset, dm);
    if (res < 0) {
      return shared_ptr<char>(nullptr);
    }

    buf_offset += res;
    if (dm.directory_mod) {
      // We changed a directory, only put that down.
      res = SerializeDirectoryMod(buf, buf_offset, dm);
      if (res < 0) {
        return shared_ptr<char>(nullptr);
      }
      buf_offset += res;
    } else {
      // We changed a file, only put that down.
      res = SerializeFileMod(buf, buf_offset, dm);
      if (res < 0) {
        return shared_ptr<char>(nullptr);
      }
      buf_offset += res;
    }
  }

  return res_ptr;
}

int DiskMod::SerializeHeader(char *buf, const unsigned int buf_offset,
    DiskMod &dm) {
  buf = buf + buf_offset;
  uint16_t mod_type = htobe16((uint16_t) dm.mod_type);
  uint16_t mod_opts = htobe16((uint16_t) dm.mod_opts);
  memcpy(buf, &mod_type, sizeof(uint16_t));
  buf += sizeof(uint16_t);
  memcpy(buf, &mod_opts, sizeof(uint16_t));

  return 2 * sizeof(uint16_t);
}

int DiskMod::SerializeChangeHeader(char *buf,
    const unsigned int buf_offset, DiskMod &dm) {
  buf += buf_offset;
  unsigned int size = dm.path.size() + 1;
  // Add the path that was changed to the buffer.
  // size() doesn't include null-terminator.
  // TODO(ashmrtn): The below assumes 1 character per byte encoding.
  memcpy(buf, dm.path.c_str(), size);
  buf += size;

  // Add directory_mod to buffer.
  uint8_t mod_directory_mod = dm.directory_mod;
  memcpy(buf, &mod_directory_mod, sizeof(uint8_t));

  return size + sizeof(uint8_t);
}

int DiskMod::SerializeFileMod(char *buf, const unsigned int buf_offset,
    DiskMod &dm) {
  buf += buf_offset;
  // Add file_mod_location.
  uint64_t file_mod_location = htobe64(dm.file_mod_location);
  memcpy(buf, &file_mod_location, sizeof(uint64_t));
  buf += sizeof(uint64_t);

  // Add file_mod_len.
  uint64_t file_mod_len = htobe64(dm.file_mod_len);
  memcpy(buf, &file_mod_len, sizeof(uint64_t));
  buf += sizeof(uint64_t);

  // Add file_mod_data (non-null terminated).
  memcpy(buf, dm.file_mod_data.get(), dm.file_mod_len);

  return (2 * sizeof(uint64_t)) + dm.file_mod_len;
}

int DiskMod::SerializeDirectoryMod(char *buf, const unsigned int buf_offset,
    DiskMod &dm) {
  assert(0 && "Not implemented");
}

int DiskMod::Deserialize(shared_ptr<char> data, DiskMod &res) {
  res.Reset();

  // Skip the first uint64 which is the size of this region. This is a blind
  // deserialization of the object!
  char *data_ptr = data.get();
  data_ptr += sizeof(uint64_t);

  uint16_t mod_type;
  uint16_t mod_opts;
  memcpy(&mod_type, data_ptr, sizeof(uint16_t));
  data_ptr += sizeof(uint16_t);
  res.mod_type = (DiskMod::ModType) be16toh(mod_type);

  memcpy(&mod_opts, data_ptr, sizeof(uint16_t));
  data_ptr += sizeof(uint16_t);
  res.mod_opts = (DiskMod::ModOpts) be16toh(mod_opts);

  if (res.mod_type == DiskMod::kCheckpointMod) {
    // No more left to do here.
    return 0;
  }

  // Small buffer to read characters into so we aren't adding to a string one
  // character at a time until the end of the string.
  const unsigned int tmp_size = 128;
  char tmp[tmp_size];
  memset(tmp, 0, tmp_size);
  unsigned int chars_read = 0;
  while (data_ptr[0] != '\0') {
    // We still haven't seen a null terminator, so read another character.
    tmp[chars_read] = data_ptr[0];
    ++chars_read;
    if (chars_read == tmp_size - 1) {
      // Fall into this at one character short so that we have an automatic null
      // terminator
      res.path += tmp;
      chars_read = 0;
      // Required because we just add the char[] to the string and we don't want
      // extra junk. An alternative would be to make sure you always had a null
      // terminator the character after the one that was just assigned.
      memset(tmp, 0, tmp_size);
    }
    ++data_ptr;
  }
  // Add the remaining data that is in tmp.
  res.path += tmp;
  // Move past the null terminating character.
  ++data_ptr;

  uint8_t dir_mod = data_ptr[0];
  ++data_ptr;
  res.directory_mod = (bool) dir_mod;

  uint64_t file_mod_location;
  uint64_t file_mod_len;
  memcpy(&file_mod_location, data_ptr, sizeof(uint64_t));
  data_ptr += sizeof(uint64_t);
  file_mod_location = be64toh(file_mod_location);
  res.file_mod_location = file_mod_location;

  memcpy(&file_mod_len, data_ptr, sizeof(uint64_t));
  data_ptr += sizeof(uint64_t);
  file_mod_len = be64toh(file_mod_len);
  res.file_mod_len = file_mod_len;

  if (res.file_mod_len > 0) {
    // Read the data for this mod.
    res.file_mod_data.reset(new (std::nothrow) char[res.file_mod_len],
        [](char *c) {delete[] c;});
    if (res.file_mod_data.get() == nullptr) {
      return -1;
    }
    memcpy(res.file_mod_data.get(), data_ptr, res.file_mod_len);
  }

  return 0;
}

DiskMod::DiskMod() {
  Reset();
}

void DiskMod::Reset() {
  path.clear();
  mod_type = kCreateMod;
  mod_opts = kNoneOpt;
  memset(&post_mod_stats, 0, sizeof(struct stat));
  directory_mod = false;
  file_mod_data.reset();
  file_mod_location = 0;
  file_mod_len = 0;
  directory_added_entry.clear();
}

}  // namespace utils
}  // namespace fs_testing
