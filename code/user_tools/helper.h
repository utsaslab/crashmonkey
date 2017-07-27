#include <cstdint>

int command_sender(const char* path, int32_t command, int32_t reply);
int send_command(int socket_fd, int32_t command);
int32_t wait_for_command(int socket);
