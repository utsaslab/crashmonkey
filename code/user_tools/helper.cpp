#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdint>

#include "helper.h"


#include <iostream>
#include <errno.h>


int command_sender(const char* path, int32_t command, int32_t reply) {
  int socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    int err = errno;
    std::cerr << "Error getting socket: " << err << std::endl;
    return -1;
  }
  struct sockaddr_un socket_info;
  socket_info.sun_family = AF_LOCAL;
  strcpy(socket_info.sun_path, path);

  if (connect(socket_fd, (struct sockaddr*) &socket_info,
        SUN_LEN(&socket_info)) < 0) {
    int err = errno;
    std::cerr << "Error connecting to socket: " << err << std::endl;
    close(socket_fd);
    return -2;
  }

  if (send_command(socket_fd, command) < 0) {
    int err = errno;
    std::cerr << "Error sending command: " << err << std::endl;
    close(socket_fd);
    return -3;
  }

  int32_t c = wait_for_command(socket_fd);
  if (c < 0 || c != reply) {
    close(socket_fd);
    return -4;
  }

  close(socket_fd);
  return 0;
}

int send_command(int client_fd, int32_t command) {
  int32_t c = htonl(command);
  //std::cerr << "data to send is at " << &c << std::endl;
  int bytes_written = 0;
  do {
    int res = send(client_fd, (char*) &c + bytes_written,
        sizeof(c), 0);
    if (res < 0) {
      int err = errno;
      std::cerr << "Error sending data: " << err << std::endl;
      return -5;
    }
    bytes_written += res;
  } while (bytes_written < sizeof(c));
  return 0;
}

int32_t wait_for_command(int socket) {
  int bytes_read = 0;
  int32_t command;
  do {
    int res = recv(socket, (char*) &command + bytes_read, sizeof(command),
        0);
    if (res < 0) {
      return -6;
    }
    bytes_read += res;
  } while(bytes_read < sizeof(command));
  
  // Correct the byte order of the command.
  command = ntohl(command);
  return command;
}
