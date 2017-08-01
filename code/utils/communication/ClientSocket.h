#ifndef UTILS_COMMUNICATION_CLIENT_SOCKET_H
#define UTILS_COMMUNICATION_CLIENT_SOCKET_H

#include <string>
#include <vector>

#include "BaseSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

class ClientSocket: public BaseSocket {
 public:
  ClientSocket(std::string address);
  ~ClientSocket();
  int Init();
  int WaitForInt(int* data);
  int SendInt(int data);
  void CloseClient();
 private:
  int socket_fd = -1;
  const std::string socket_address;
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_CLIENT_SOCKET_H
