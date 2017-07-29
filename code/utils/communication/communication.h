#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#define HARNESS_ERROR         -1
#define HARNESS_WRONG_COMMAND -1
// From perspective of calling shim script.
#define HARNESS_PREPARE       1
#define HARNESS_BEGIN_LOG     3
#define HARNESS_END_LOG       5
#define HARNESS_RUN_TESTS     7

// From perspective of CrashMonkey harness. i.e. CrashMonkey harness' responses
// to the above requests.
#define HARNESS_PREPARE_DONE  2
#define HARNESS_LOG_READY     4
#define HARNESS_LOG_DONE      6
#define HARNESS_TESTS_DONE    8

#define SOCKET_DIR           "/tmp"
#define SOCKET_NAME_OUTBOUND SOCKET_DIR "/crash_monkey_harness"

#endif
