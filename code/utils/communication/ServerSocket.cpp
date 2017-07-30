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

ServerSocket::ServerSocket(string address): socket_address(address) {};

ServerSocket::~ServerSocket() {
  // If we try to close an invalid file descriptor then oh well, nothing bad
  // should happen.
  close(client_socket);
  close(server_socket);
  unlink(socket_address.c_str());
}

int ServerSocket::Init() {
  server_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (server_socket < -1) {
    return -1;
  }
  struct sockaddr_un comm;
  comm.sun_family = AF_LOCAL;
  strcpy(comm.sun_path, socket_address.c_str());
  if (bind(server_socket, (const struct sockaddr*) &comm, sizeof(comm)) < 0) {
    return -1;
  }

  if (listen(server_socket, 1) < 0) {
    return -1;
  }
  return 0;
}

int ServerSocket::WaitForInt(int* data) {
  if (client_socket >= 0) {
    return -2;
  }
  // For now, don't care about getting the client address.
  client_socket = accept(server_socket, NULL, NULL);
  if (client_socket < 0) {
    return -1;
  }

  return ServerSocket::ReadIntFromSocket(client_socket, data);
}

int ServerSocket::SendInt(int data) {
  if (server_socket < 0) {
    return -1;
  }
  return ServerSocket::WriteIntToSocket(client_socket, data);
}

int ServerSocket::WaitAndSendInt(int data) {
  if (client_socket >= 0) {
    return -2;
  }
  // For now, don't care about getting the client address.
  client_socket = accept(server_socket, NULL, NULL);
  if (client_socket < 0) {
    return -1;
  }

  return SendInt(data);
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
