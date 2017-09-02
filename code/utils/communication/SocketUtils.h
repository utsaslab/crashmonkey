#ifndef UTILS_COMMUNICATION_SOCKET_UTILS_H
#define UTILS_COMMUNICATION_SOCKET_UTILS_H

#include <string>

namespace fs_testing {
namespace utils {
namespace communication {

const char kSocketDir[] = "/tmp";
// C++11 annoyingly doesn't support compile-time concatenation of const char[].
// Instead of adding a bunch of workaround code to do that, **please, please**
// make sure that the path below matches the path above (with the exception of
// the appended "crash_monkey_harness" part).
const char kSocketNameOutbound[] = "/tmp/crash_monkey_harness";

/*******************************************************************************
 * Basic information about the layout of messages sent and received by
 * CrashMonkey:
 *
 * * all messages should be sent with network endian encoding
 * * all messages contain the type they are supposed to be:
 *     * one of the control messages
 *     * a checkpoint message
 * * all messages contain the size they are
 *     * more of a fail-safe than anything as it allows the socket to clear out
 *       any data that may have been sent in a message but that was not needed
 *         ex. sending a string along with a control message
 * * the data returned to callers depends on the type of a message
 ******************************************************************************/


// Simple container for the different responses that you can get back from a
// socket connection. I'm lazy and using C++ so I'm using strings instead of a
// char* where I'd have to do some memory management myself. The fields at the
// end of this struct are not part of a union because C++ does not allow unions
// on types that have constructors (which string does). This is also not worth
// subclassing/using static_cast on for the different data types as there are
// only really two options right now. If it ever comes to the point where we
// need that sort of flexibility, we can go that route.
struct SocketMessage {
  // Commands saying to do something are from perspective of calling shim
  // script. Commands saying something is done are from perspective of
  // CrashMonkey harness (i.e. CrashMonkey harness' responses to the commands).
  enum CmCommand : uint32_t {
    kHarnessError,
    kInvalidCommand,
    kPrepare,
    kPrepareDone,
    kBeginLog,
    kBeginLogDone,
    kEndLog,
    kEndLogDone,
    kRunTests,
    kRunTestsDone,
    kCheckpoint,
    kCheckpointDone,
  };

  CmCommand type;
  // Size of the data **after** the CmCommand type of the message.
  unsigned int size;
  int int_value;
  std::string string_value;
};

// Basic errors to handle reporting back to the user. They aren't meant to
// shadow/cover/encapsulate what Linux already provides with errno (though they
// probably should as errno is a bit... uncouth?). They are mostly around to
// make it easy to differentiate between when you got the right type message
// back or not for the WaitForX functions.
enum SocketError {
  // No error occurred.
  kNone,
  // Some system error occurred (send, recv, accept, etc. returned < 0).
  kSyscall,
  // A type different than what was requested was returned (ex. asked for int
  // but got string).
  kWrongType,
  kAlreadyConnected,
  kNotConnected,
};

}  // namespace communication
}  // namespace utils
}  // namespace fs_testing

#endif  // UTILS_COMMUNICATION_SOCKET_UTILS_H
