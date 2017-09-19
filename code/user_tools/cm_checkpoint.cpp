#include "../utils/communication/ClientCommandSender.h"
#include "../utils/communication/SocketUtils.h"

using fs_testing::utils::communication::ClientCommandSender;
using fs_testing::utils::communication::kSocketNameOutbound;
using fs_testing::utils::communication::SocketMessage;

int main(int argc, char** argv) {
  return ClientCommandSender(kSocketNameOutbound, SocketMessage::kCheckpoint,
      SocketMessage::kCheckpointDone).Run();
}
