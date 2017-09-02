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
  socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
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
  return 0;
}

SocketError ClientSocket::SendCommand(SocketMessage::CmCommand c) {
  SocketMessage m;
  m.type = c;
  m.size = 0;
  return SendMessage(m);
}

SocketError ClientSocket::SendMessage(SocketMessage &m) {
  if (socket_fd < 0) {
    return SocketError::kNotConnected;
  }

  if (BaseSocket::WriteMessageToSocket(socket_fd, m) < 0) {
    return SocketError::kSyscall;
  }
  return SocketError::kNone;
}

SocketError ClientSocket::WaitForMessage(SocketMessage *m) {
  if (BaseSocket::ReadMessageFromSocket(socket_fd, m) < 0) {
    return SocketError::kSyscall;
  }

  return SocketError::kNone;
}

void ClientSocket::CloseClient() {
  close(socket_fd);
  socket_fd = -1;
}

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing
