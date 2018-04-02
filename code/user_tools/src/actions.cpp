#include "../api/actions.h"
#include "../api/actions_c.h"
#include "../../utils/communication/ClientCommandSender.h"

namespace fs_testing {
namespace user_tools {
namespace api {

using fs_testing::utils::communication::ClientCommandSender;
using fs_testing::utils::communication::kSocketNameOutbound;
using fs_testing::utils::communication::SocketMessage;

extern "C" {
int Checkpoint() {
  ClientCommandSender c(kSocketNameOutbound, SocketMessage::kCheckpoint,
      SocketMessage::kCheckpointDone);
  return c.Run();
}
}

} // fs_testing
} // user_tools
} // api
