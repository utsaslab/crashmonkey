#ifndef FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H
#define FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H

#include <string>

#include "communication.h"
#include "ClientSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

class ClientCommandSender {
 public:
  ClientCommandSender(std::string socket_addr, int send, int recv);
  int Run();

 private:
  const std::string socket_address;
  const int send_command;
  const int return_command;
  ClientSocket conn;
};

}  // namesapce communication
}  // namesapce utils
}  // namesapce fs_testing

#endif  // FS_TESTING_UTILS_COMMUNICATION_CLIENT_COMMAND_SENDER_H
