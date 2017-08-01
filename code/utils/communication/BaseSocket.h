#ifndef UTILS_COMMUNICATION_BASE_SOCKET_H
#define UTILS_COMMUNICATION_BASE_SOCKET_H

namespace fs_testing {
namespace utils {
namespace communication {

class BaseSocket {
 protected:
  static int ReadIntFromSocket(int socket, int* data);
  static int WriteIntToSocket(int socket, int data);
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_BASE_SOCKET_H
