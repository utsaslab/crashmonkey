#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <vector>

#include "hellow_ioctl.h"

namespace fs_testing {

class disk_write {
 public:
  disk_write();
  disk_write(const struct disk_write_op_meta& m, const void* d);
  disk_write(const disk_write& other);
  ~disk_write();

  struct disk_write_op_meta metadata;
  void* data;

  disk_write& operator=(const disk_write& other);
};

bool operator==(const disk_write& a, const disk_write& b);
bool operator!=(const disk_write& a, const disk_write& b);
std::ostream& operator<<(std::ostream& os, const disk_write& dw);

std::vector<disk_write> permute(const std::vector<disk_write>* data,
    const std::vector<disk_write>* original,
    const std::vector<disk_write>* ordering);

}  // namespace fs_testing
#endif
