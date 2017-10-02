#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>

#include "BaseSocket.h"

namespace fs_testing {
namespace utils {
namespace communication {

using std::memset;
using std::string;
using std::strncpy;

int BaseSocket::ReadMessageFromSocket(int socket, SocketMessage *m) {
  // What sort of message are we dealing with and how big is it?
  // TODO(ashmrtn): Improve error checking/recoverability.
  int res = ReadIntFromSocket(socket, (int *) &m->type);
  if (res < 0) {
    return res;
  }
  res = ReadIntFromSocket(socket, (int *) &m->size);
  if (res < 0) {
    return res;
  }

  switch (m->type) {
    // These messges should contain no extra data.
    case SocketMessage::kHarnessError:
    case SocketMessage::kInvalidCommand:
    case SocketMessage::kPrepare:
    case SocketMessage::kPrepareDone:
    case SocketMessage::kBeginLog:
    case SocketMessage::kBeginLogDone:
    case SocketMessage::kEndLog:
    case SocketMessage::kEndLogDone:
    case SocketMessage::kRunTests:
    case SocketMessage::kRunTestsDone:
    case SocketMessage::kCheckpoint:
    case SocketMessage::kCheckpointDone:
    case SocketMessage::kCheckpointFailed:
      // Somebody sent us extra data anyway. Gobble it up and throw it away.
      if (m->size != 0) {
        res = GobbleData(socket, m->size);
      }
      break;
    default:
      res = -1;
  }
  return res;
};

int BaseSocket::WriteMessageToSocket(int socket, SocketMessage &m) {
  // What sort of message are we dealing with and how big is it?
  // TODO(ashmrtn): Improve error checking/recoverability.
  int res = WriteIntToSocket(socket, (int) m.type);
  if (res < 0) {
    return res;
  }

  switch (m.type) {
    // These messges should contain no extra data.
    case SocketMessage::kHarnessError:
    case SocketMessage::kInvalidCommand:
    case SocketMessage::kPrepare:
    case SocketMessage::kPrepareDone:
    case SocketMessage::kBeginLog:
    case SocketMessage::kBeginLogDone:
    case SocketMessage::kEndLog:
    case SocketMessage::kEndLogDone:
    case SocketMessage::kRunTests:
    case SocketMessage::kRunTestsDone:
    case SocketMessage::kCheckpoint:
    case SocketMessage::kCheckpointDone:
    case SocketMessage::kCheckpointFailed:
      // By default, always send the proper size of the message and no other,
      // extra data.
      res = WriteIntToSocket(socket, 0);
      if (res < 0) {
        return res;
      }
      break;
    default:
      res = -1;
  }
  return res;
};

int BaseSocket::GobbleData(int socket, unsigned int len) {
  int bytes_read = 0;
  char tmp[len];
  do {
    int res = recv(socket, tmp + bytes_read, sizeof(tmp) - bytes_read, 0);
    if (res < 0) {
      return -1;
    }
    bytes_read += res;
  } while(bytes_read < sizeof(tmp));
  return 0;
}

int BaseSocket::ReadIntFromSocket(int socket, int* data) {
  int bytes_read = 0;
  int32_t d;
  do {
    int res = recv(socket, (char*) &d + bytes_read, sizeof(d) - bytes_read, 0);
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
    int res = send(socket, (char*) &d + bytes_written,
        sizeof(d) - bytes_written, 0);
    if (res < 0) {
      return -1;
    }
    bytes_written += res;
  } while (bytes_written < sizeof(d));
  return 0;
}

int BaseSocket::ReadStringFromSocket(int socket, unsigned int len,
    string *data) {
  // Assume all messages are sent in network endian. Furthermore, strings have
  // been rounded up to the nearest multiple of sizeof(uint32_t) bytes.

  // Not a multiple of sizeof(uint32_t) bytes.
  if (len & (sizeof(uint32_t) - 1)) {
    return -1;
  }

  int bytes_read = 0;
  char read_string[len];
  do {
    int res = recv(socket, read_string + bytes_read, len - bytes_read, 0);
    if (res < 0) {
      return -1;
    }
    bytes_read += res;
  } while(bytes_read < len);

  // Correct the byte order of the string by abusing type casts... yay!
  uint32_t *tmp = (uint32_t*) read_string;
  for (int i = 0; i < len / sizeof(uint32_t); ++i) {
    *(tmp + i) = ntohl(*(tmp + i));
  }
  *data = read_string;
  return 0;
}

// Assume all messages are sent in network endian. Furthermore, strings are
// rounded up to the nearest multiple of sizeof(uint32_t) bytes.
int BaseSocket::WriteStringToSocket(int socket, string &data) {
  // Some prep work so we can send everything one after the other.
  const int len =
    (data.size() + (sizeof(uint32_t) - 1)) & ~(sizeof(uint32_t) - 1);

  char send_data[len];
  memset(send_data, 0, len);
  strcpy(send_data, data.c_str());
  uint32_t *tmp = (uint32_t*) send_data;
  for (int i = 0; i < len / sizeof(uint32_t); ++i) {
    *(tmp + i) = htonl(*(tmp + i));
  }

  // Send string size.
  int res = WriteIntToSocket(socket, len);
  if (res < 0) {
    return res;
  }

  // Send string itself.
  int bytes_written = 0;
  do {
    int res = send(socket, send_data + bytes_written, len - bytes_written, 0);
    if (res < 0) {
      return -1;
    }
    bytes_written += res;
  } while(bytes_written < len);

  return 0;
}

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing
