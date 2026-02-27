#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SOCKET_PATH "/tmp/taskmaster.sock"
#define MAX_MSG_LEN 8192

typedef enum {
    CMD_STATUS,
    CMD_START,
    CMD_STOP,
    CMD_RESTART,
    CMD_RELOAD,
    CMD_SHUTDOWN
} CommandType;

typedef struct {
    CommandType type;
    char payload[MAX_NAME_LEN];
} TMRequest;

typedef struct {
    char response[MAX_MSG_LEN];
    bool success;
} TMResponse;

#endif
