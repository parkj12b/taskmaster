#include "taskmaster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int connect_to_daemon() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void send_command(CommandType type, const char *payload) {
    int fd = connect_to_daemon();
    if (fd < 0) {
        fprintf(stderr, "Error: Could not connect to daemon at %s\n", SOCKET_PATH);
        return;
    }

    TMRequest req;
    memset(&req, 0, sizeof(req));
    req.type = type;
    if (payload) strncpy(req.payload, payload, sizeof(req.payload) - 1);

    send(fd, &req, sizeof(req), 0);

    TMResponse res;
    memset(&res, 0, sizeof(res));
    if (recv(fd, &res, sizeof(res), 0) > 0) {
        printf("%s", res.response);
    } else {
        fprintf(stderr, "Error: No response from daemon\n");
    }

    close(fd);
}

void handle_client_line(char *line) {
    char *saveptr;
    char *cmd = strtok_r(line, " \n", &saveptr);
    if (!cmd) return;

    if (strcmp(cmd, "status") == 0) {
        send_command(CMD_STATUS, NULL);
    } else if (strcmp(cmd, "start") == 0) {
        char *name = strtok_r(NULL, " \n", &saveptr);
        if (name) send_command(CMD_START, name);
    } else if (strcmp(cmd, "stop") == 0) {
        char *name = strtok_r(NULL, " \n", &saveptr);
        if (name) send_command(CMD_STOP, name);
    } else if (strcmp(cmd, "restart") == 0) {
        char *name = strtok_r(NULL, " \n", &saveptr);
        if (name) send_command(CMD_RESTART, name);
    } else if (strcmp(cmd, "reload") == 0) {
        send_command(CMD_RELOAD, NULL);
    } else if (strcmp(cmd, "shutdown") == 0) {
        send_command(CMD_SHUTDOWN, NULL);
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        exit(0);
    } else {
        printf("Unknown command: %s\n", cmd);
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        char cmd_buf[MAX_CMD_LEN] = {0};
        for (int i = 1; i < argc; i++) {
            strncat(cmd_buf, argv[i], MAX_CMD_LEN - strlen(cmd_buf) - 1);
            if (i < argc - 1) strncat(cmd_buf, " ", MAX_CMD_LEN - strlen(cmd_buf) - 1);
        }
        handle_client_line(cmd_buf);
        return 0;
    }

    char line[256];
    printf("taskmasterctl> ");
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        handle_client_line(line);
        printf("taskmasterctl> ");
        fflush(stdout);
    }

    return 0;
}
