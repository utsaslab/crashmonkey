#include "communication.h"
#include "helper.h"

int main(int argc, char** argv) {
  return command_sender(SOCKET_NAME_OUTBOUND, HARNESS_RUN_TESTS,
      HARNESS_TESTS_DONE);
}
