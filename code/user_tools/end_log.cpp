#include "communication.h"
#include "helper.h"

int main(int argc, char** argv) {
  return command_sender(SOCKET_NAME_OUTBOUND, HARNESS_END_LOG,
      HARNESS_LOG_DONE);
}
