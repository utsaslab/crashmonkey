#ifndef USER_TOOLS_API_WORKLOAD_H
#define USER_TOOLS_API_WORKLOAD_H

namespace fs_testing {
namespace user_tools {
namespace api {

// Write size bytes of known data at offset in the file specified by fd. Returns
// 0 on success and -1 on error.
int WriteData(int fd, unsigned int offset, unsigned int size);

// Use mmap and msync to write size bytes of known data at offset in the file
// specified by fd. Returns 0 on success and -1 on error.
int WriteDataMmap(int fd, unsigned int offset, unsigned int size);

} // fs_testing
} // user_tools
} // api

#endif // USER_TOOLS_API_WORKLOAD_H
