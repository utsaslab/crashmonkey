#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "ServerSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

using std::string;

namespace {
  const unsigned int kNonBlockPollTimeout = 25;
}

ServerSocket::ServerSocket(string address): socket_address(address) {};

ServerSocket::~ServerSocket() {
  // If we try to close an invalid file descriptor then oh well, nothing bad
  // should happen (famous last words...).
  close(client_socket);
  close(server_socket);
  unlink(socket_address.c_str());
}

int ServerSocket::Init(unsigned int queue_depth) {
  server_socket =
    socket(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
  if (server_socket < 0) {
    return -1;
  }
  struct sockaddr_un comm;
  comm.sun_family = AF_LOCAL;
  strcpy(comm.sun_path, socket_address.c_str());
  if (bind(server_socket, (const struct sockaddr*) &comm, sizeof(comm)) < 0) {
    return -1;
  }

  if (listen(server_socket, queue_depth) < 0) {
    return -1;
  }
  return 0;
}

SocketError ServerSocket::SendCommand(SocketMessage::CmCommand c) {
  SocketMessage m;
  m.type = c;
  m.size = 0;
  return SendMessage(m);
}

SocketError ServerSocket::SendMessage(SocketMessage &m) {
  if (server_socket < 0) {
    return SocketError::kNotConnected;
  }

  if (BaseSocket::WriteMessageToSocket(client_socket, m) < 0) {
    return SocketError::kSyscall;
  }
  return SocketError::kNone;
}

SocketError ServerSocket::WaitForMessage(SocketMessage *m) {
  // We're already connected to something.
  if (client_socket >= 0) {
    return SocketError::kAlreadyConnected;
  }
  // Block until we can call accept without blocking.
  struct pollfd pfd;
  pfd.fd = server_socket;
  pfd.events = POLLIN;

  int res = poll(&pfd, 1, -1);

  if (res == -1) {
    return SocketError::kSyscall;
  } else if (res == 0) {
    return SocketError::kTimeout;
  } else if (!(pfd.revents & POLLIN)) {
    return SocketError::kOther;
  }

  // For now, don't care about getting the client address.
  client_socket = accept(server_socket, NULL, NULL);
  if (client_socket < 0) {
    return SocketError::kSyscall;
  }

  if (BaseSocket::ReadMessageFromSocket(client_socket, m)) {
    return SocketError::kSyscall;
  }
  return SocketError::kNone;
}

SocketError ServerSocket::TryForMessage(SocketMessage *m) {
  // We're already connected to something.
  if (client_socket >= 0) {
    return SocketError::kAlreadyConnected;
  }
  // Block until we can call accept without blocking.
  struct pollfd pfd;
  pfd.fd = server_socket;
  pfd.events = POLLIN;

  int res = poll(&pfd, 1, kNonBlockPollTimeout);

  if (res == -1) {
    return SocketError::kSyscall;
  } else if (res == 0) {
    return SocketError::kTimeout;
  } else if (!(pfd.revents & POLLIN)) {
    return SocketError::kOther;
  }

  // For now, don't care about getting the client address.
  client_socket = accept(server_socket, NULL, NULL);
  if (client_socket < 0) {
    return SocketError::kSyscall;
  }

  if (BaseSocket::ReadMessageFromSocket(client_socket, m)) {
    return SocketError::kSyscall;
  }
  return SocketError::kNone;
}

void ServerSocket::CloseClient() {
  close(client_socket);
  client_socket = -1;
}

void ServerSocket::CloseServer() {
  close(client_socket);
  client_socket = -1;
  close(server_socket);
  server_socket = -1;
}

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing
