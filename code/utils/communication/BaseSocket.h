#ifndef UTILS_COMMUNICATION_BASE_SOCKET_H
#define UTILS_COMMUNICATION_BASE_SOCKET_H

#include "SocketUtils.h"

namespace fs_testing {
namespace utils {
namespace communication {

class BaseSocket {
 public:
  static int ReadMessageFromSocket(int socket, SocketMessage *data);
  static int WriteMessageToSocket(int socket, SocketMessage &data);

 private:
  static int GobbleData(int socket, unsigned int len);
  static int ReadIntFromSocket(int socket, int* data);
  static int WriteIntToSocket(int socket, int data);
  static int ReadStringFromSocket(int socket, unsigned int len,
      std::string *data);
  static int WriteStringToSocket(int socket, std::string &data);
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_BASE_SOCKET_H
