//
// Created by Fábio Domingues on 27/07/17.
//

#ifndef CRASHMONKEY_IPC_H
#define CRASHMONKEY_IPC_H

int start_server(char *path);

int accept_connection(int server);

int connect_server(char *path);

#endif //CRASHMONKEY_IPC_H
