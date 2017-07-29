#include "../utils/communication/ClientCommandSender.h"
#include "../utils/communication/communication.h"

using fs_testing::utils::communication::ClientCommandSender;

int main(int argc, char** argv) {
  return ClientCommandSender(SOCKET_NAME_OUTBOUND, HARNESS_BEGIN_LOG,
      HARNESS_LOG_READY).Run();
}
