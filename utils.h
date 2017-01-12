#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <random>
#include <utility>
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

/*
namespace {
struct permute_info;
}  // namespace
*/

struct permute_info {
  disk_write write;
  int shift;
};

class permuter {
 public:
  permuter() = default;
  permuter(const std::vector<disk_write>* data);
  permuter& operator=(const permuter& other);
  bool permute(std::vector<disk_write>* res);
  bool permute_nth(std::vector<disk_write>* res, int n);
  void permute_random(std::vector<disk_write>* res);

 private:
  std::vector<disk_write> original;
  std::vector<disk_write> current;
  std::vector<struct permute_info> ordering;
  std::mt19937 rand;
  //std::unordered_set<vector<disk_write>> done;
};

}  // namespace fs_testing
#endif
