#include <string>

#include "communication.h"
#include "ClientCommandSender.h"
#include "ClientSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

ClientCommandSender::ClientCommandSender(std::string socket_addr, int send,
    int recv) : socket_address(socket_addr), send_command(send),
    return_command(recv), conn(ClientSocket(socket_address)) {}

int ClientCommandSender::Run() {
  if (conn.Init() < 0) {
    return -1;
  }

  if (conn.SendInt(send_command) < 0) {
    return -2;
  }

  int ret;
  if (conn.WaitForInt(&ret) < 0) {
    return -3;
  }
  return ret == return_command;
}

}  // namesapce communication
}  // namesapce utils
}  // namesapce fs_testing
