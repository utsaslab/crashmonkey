#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "ClientSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

using std::string;

ClientSocket::ClientSocket(string address): socket_address(address) {};

ClientSocket::~ClientSocket() {
  close(socket_fd);
}

int ClientSocket::Init() {
  int socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    return -1;
  }
  struct sockaddr_un socket_info;
  socket_info.sun_family = AF_LOCAL;
  strcpy(socket_info.sun_path, socket_address.c_str());

  if (connect(socket_fd, (struct sockaddr*) &socket_info,
        sizeof(socket_info)) < 0) {
    return -1;
  }
}

int ClientSocket::WaitForInt(int* data) {
  return ClientSocket::ReadIntFromSocket(socket_fd, data);
}

int ClientSocket::SendInt(int data) {
  if (socket_fd < 0) {
    return -1;
  }
  return ClientSocket::WriteIntToSocket(socket_fd, data);
}

void ClientSocket::CloseClient() {
  close(socket_fd);
  socket_fd = -1;
}

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing
