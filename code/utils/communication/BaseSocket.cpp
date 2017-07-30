#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>

#include "BaseSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

int BaseSocket::ReadIntFromSocket(int socket, int* data) {
  int bytes_read = 0;
  int32_t d;
  do {
    int res = recv(socket, (char*) &d + bytes_read, sizeof(d), 0);
    if (res < 0) {
      return -1;
    }
    bytes_read += res;
  } while(bytes_read < sizeof(d));

  // Correct the byte order of the command.
  *data = ntohl(d);
  return 0;
}

int BaseSocket::WriteIntToSocket(int socket, int data) {
  int32_t d = htonl(data);
  int bytes_written = 0;
  do {
    int res = send(socket, (char*) &d + bytes_written, sizeof(d), 0);
    if (res < 0) {
      return -1;
    }
    bytes_written += res;
  } while (bytes_written < sizeof(d));
  return 0;
}

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing
