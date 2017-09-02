#ifndef UTILS_COMMUNICATION_CLIENT_SOCKET_H
#define UTILS_COMMUNICATION_CLIENT_SOCKET_H

#include <string>
#include <vector>

#include "BaseSocket.h"
#include "SocketUtils.h"

namespace fs_testing {
namespace utils {
namespace communication {

// Simple class that acts as a client for a socket by sending and sometimes
// receiving messages.
// *** This is not a thread-safe class. ***
class ClientSocket {
 public:
  ClientSocket(std::string address);
  ~ClientSocket();
  int Init();
  SocketError SendCommand(SocketMessage::CmCommand c);
  SocketError SendMessage(SocketMessage &m);
  SocketError WaitForMessage(SocketMessage *m);
  void CloseClient();
 private:
  int socket_fd = -1;
  const std::string socket_address;
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_CLIENT_SOCKET_H
