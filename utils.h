#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "hellow_ioctl.h"

namespace fs_testing {
namespace utils {

class disk_write {
 public:
  disk_write() = default;
  disk_write(const struct disk_write_op_meta& m, const void* d);
  disk_write(const disk_write& other);

  struct disk_write_op_meta metadata;

  disk_write& operator=(const disk_write& other);
  friend bool operator==(const disk_write& a, const disk_write& b);
  friend bool operator!=(const disk_write& a, const disk_write& b);

  bool has_write_flag();
  bool is_barrier_write();
  bool is_async_write();
  // Returns a pointer to the data which was assigned or NULL if data could not
  // be assigned. Pointer is valid only as long as the object exists. The user
  // should not call free on this pointer or otherwise attempt memory management
  // of it.
  std::shared_ptr<void> set_data(const void* data);
  // Returns a pointer to the data field or NULL if data has not been assigned.
  // Pointer is valid only as long as the object exists or otherwise attempt
  // memory management of it.
  std::shared_ptr<void> get_data();

 private:
  std::shared_ptr<void> data;
};

bool operator==(const disk_write& a, const disk_write& b);
bool operator!=(const disk_write& a, const disk_write& b);
std::ostream& operator<<(std::ostream& os, const disk_write& dw);

}  // namespace utils
}  // namespace fs_testing
#endif
