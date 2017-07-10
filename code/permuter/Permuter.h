#ifndef PERMUTER_H
#define PERMUTER_H

#include <vector>

#include "../utils/utils.h"

namespace fs_testing {
namespace permuter {

class Permuter {
 public:
  virtual ~Permuter() {};
  virtual void init_data(std::vector<fs_testing::utils::disk_write> *data) = 0;
  virtual bool gen_one_state(std::vector<fs_testing::utils::disk_write>& res) = 0;
};

typedef Permuter *permuter_create_t();
typedef void permuter_destroy_t(Permuter *instance);

}  // namespace permuter
}  // namespace fs_testing

#endif

