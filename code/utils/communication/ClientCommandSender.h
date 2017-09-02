#ifndef FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H
#define FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H

#include <string>

#include "ClientSocket.h"
#include "SocketUtils.h"

namespace fs_testing {
namespace utils {
namespace communication {

class ClientCommandSender {
 public:
  ClientCommandSender(std::string socket_addr, SocketMessage::CmCommand send,
      SocketMessage::CmCommand recv);
  int Run();

 private:
  const std::string socket_address;
  const SocketMessage::CmCommand send_command;
  const SocketMessage::CmCommand return_command;
  ClientSocket conn;
};

}  // namesapce communication
}  // namesapce utils
}  // namesapce fs_testing

#endif  // FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H
