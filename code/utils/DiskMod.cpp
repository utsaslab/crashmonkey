#include "DiskMod.h"

#include <assert.h>
#include <endian.h>
#include <string.h>

namespace fs_testing {
namespace utils {

using std::shared_ptr;
using std::vector;

unsigned long long int DiskMod::GetSerializeSize() {
  // mod_type, mod_opts, and a uint64_t for the size of the serialized mod.
  unsigned long long int res = 2 * sizeof(uint16_t) + sizeof(uint64_t);

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
int DiskMod::Serialize(const int fd,
    const vector<DiskMod> &to_serialize) {
  // Move back to the start of the file.
  int res = lseek(fd, 0, SEEK_SET);
  if (res < 0) {
    return res;
  }

  for (const auto &dm : to_serialize) {
    const int res = DiskMod::Serialize(fd, dm);
    if (res < 0) {
      return res;
    }
  }

  return 0;
}

int DiskMod::Deserialize(const int fd, vector<DiskMod> &res) {
  res.clear();
  // Move back to the start of the file.
  int res = lseek(fd, 0, SEEK_SET);
  if (res < 0) {
    return res;
  }
}
*/

/*
 * Make things miserable on myself, and only store either the directory or the
 * file fields based on the value of directory_mod.
 *
 * Convert everything to big endian for the sake of reading dumps in a
 * consistent manner if need be.
 */
shared_ptr<char> DiskMod::Serialize(DiskMod &dm) {
  // Standard code to serialize the front part of the DiskMod.
  // Get a block large enough for this DiskMod.
  const unsigned long long int mod_size = dm.GetSerializeSize();
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
  uint16_t mod_type = htobe16((uint16_t) dm.mod_type);
  uint16_t mod_opts = htobe16((uint16_t) dm.mod_type);
  memcpy(buf, &mod_type, sizeof(uint16_t));
  buf += sizeof(uint16_t);
  memcpy(buf, &mod_type, sizeof(uint16_t));

  return 2 * sizeof(uint16_t);
}

int DiskMod::SerializeChangeHeader(char *buf,
    const unsigned int buf_offset, DiskMod &dm) {
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

int DiskMod::Deserialize(const int fd, DiskMod &res) {
  return 0;
}

}  // namespace utils
}  // namespace fs_testing
