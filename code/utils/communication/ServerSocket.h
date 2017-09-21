#ifndef UTILS_COMMUNICATION_SERVER_SOCKET_H
#define UTILS_COMMUNICATION_SERVER_SOCKET_H

#include <string>
#include <vector>

#include "BaseSocket.h"
#include "SocketUtils.h"

namespace fs_testing {
namespace utils {
namespace communication {

// Simple class that acts as a server for a socket by receiving and replying to
// messages.
// *** This is not a thread-safe class. ***
class ServerSocket {
 public:
  ServerSocket(std::string address);
  ~ServerSocket();
  int Init(unsigned int queue_depth);
  // Shorthand for SendMessage with the proper options.
  SocketError SendCommand(SocketMessage::CmCommand c);
  SocketError SendMessage(SocketMessage &m);
  SocketError WaitForMessage(SocketMessage *m);
  SocketError TryForMessage(SocketMessage *m);
  void CloseClient();
  void CloseServer();
 private:
  int server_socket = -1;
  int client_socket = -1;
  const std::string socket_address;
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_SERVER_SOCKET_H
