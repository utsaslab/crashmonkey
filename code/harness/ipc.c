//
// Created by Fábio Domingues on 27/07/17.
//

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>

int start_server(char *path) {
  // TODO: get errors code and messages
  int server;
  struct sockaddr_un address;
  socklen_t len;

  server = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server == -1) {
    printf("ERROR: socket server creating\n");
    close(server);
    return -1;
  }

  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);
  len = sizeof(address);

  unlink(path);
  if (bind(server, (struct sockaddr *) &address, len) == -1) {
    printf("ERROR: socket server binding\n");
    close(server);
    return -1;
  }

  if (listen(server, 1)) {
    printf("ERROR: socket server listen\n");
    close(server);
    return -1;
  }

  return server;
}

int accept_connection(int server) {
  int client;
  struct sockaddr_un address;
  socklen_t len;

  len = sizeof(address);
  if ((client = accept(server, (struct sockaddr *) &address, &len)) == -1) {
    printf("ERROR: socket server accept\n");
    close(client);
    close(server);
    return -1;
  }

  return client;
}

int connect_server(char *path) {// TODO: get errors code and messages
  int connection;
  struct sockaddr_un address;
  socklen_t len;

  connection = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connection == -1) {
    printf("ERROR: socket client creating\n");
    close(connection);
    return -1;
  }

  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);
  len = sizeof(address);
  if (connect(connection, (struct sockaddr *) &address, len) == -1) {
    printf("ERROR: socket connecting to server\n");
    return -1;
  }

  return connection;
}
