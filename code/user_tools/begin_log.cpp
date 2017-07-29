#include "../utils/communication/communication.h"
#include "../utils/communication/helper.h"

int main(int argc, char** argv) {
  return command_sender(SOCKET_NAME_OUTBOUND, HARNESS_BEGIN_LOG,
      HARNESS_LOG_READY);
}
