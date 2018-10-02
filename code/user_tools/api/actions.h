#ifndef USER_TOOLS_API_ACTIONS_H
#define USER_TOOLS_API_ACTIONS_H
#include <linux/falloc.h>
#include <fcntl.h>
namespace fs_testing {
namespace user_tools {
namespace api {

int Checkpoint();

} // fs_testing
} // user_tools
} // api

#endif // USER_TOOLS_API_ACTIONS_H
