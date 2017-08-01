#ifndef UTILS_COMMUNICATION_SERVER_SOCKET_H
#define UTILS_COMMUNICATION_SERVER_SOCKET_H

#include <string>
#include <vector>

#include "BaseSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

class ServerSocket: public BaseSocket {
 public:
  ServerSocket(std::string address);
  ~ServerSocket();
  int Init();
  int WaitForInt(int* data);
  int SendInt(int data);
  int WaitAndSendInt(int data);
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
