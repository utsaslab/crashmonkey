#ifndef UTILS_H
#define UTILS_H

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "../disk_wrapper_ioctl.h"

namespace fs_testing {
namespace utils {

class disk_write {
 public:
  disk_write() = default;
  disk_write(const struct disk_write_op_meta& m, const char *d);
  disk_write(const disk_write& other);

  struct disk_write_op_meta metadata;

  void operator=(const disk_write& other);
  friend bool operator==(const disk_write& a, const disk_write& b);
  friend bool operator!=(const disk_write& a, const disk_write& b);
  friend std::ofstream& operator<<(std::ofstream& os, const disk_write& dw);

  bool has_write_flag();
  bool is_barrier_write();
  bool is_async_write();

  static void serialize(std::ofstream& fs, const disk_write& dw);
  static disk_write deserialize(std::ifstream& is);

  // Returns a pointer to the data which was assigned or NULL if data could not
  // be assigned. Pointer is valid only as long as the object exists. The user
  // should not call free on this pointer or otherwise attempt memory management
  // of it.
  std::shared_ptr<char> set_data(const char *data);
  // Returns a pointer to the data field or NULL if data has not been assigned.
  // Pointer is valid only as long as the object exists or otherwise attempt
  // memory management of it.
  std::shared_ptr<char> get_data();

 private:
  std::shared_ptr<char> data;
};


bool operator==(const disk_write& a, const disk_write& b);
bool operator!=(const disk_write& a, const disk_write& b);
std::ostream& operator<<(std::ostream& os, const disk_write& dw);

}  // namespace utils
}  // namespace fs_testing
#endif
