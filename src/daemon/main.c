#include "taskmaster.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

Taskmaster g_tm;
char *g_config_path = NULL;
volatile sig_atomic_t g_reload_requested = 0;

void handle_sighup(int sig) {
    (void)sig;
    g_reload_requested = 1;
}

static void setup_server_socket(Taskmaster *tm) {
    tm->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (tm->server_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);
    if (bind(tm->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(tm->server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }
    
    fcntl(tm->server_fd, F_SETFL, O_NONBLOCK);
}

void handle_client(Taskmaster *tm, int client_fd) {
    TMRequest req;
    TMResponse res;
    memset(&res, 0, sizeof(res));
    res.success = true;

    if (recv(client_fd, &req, sizeof(req), 0) <= 0) return;

    switch (req.type) {
        case CMD_STATUS:
            {
                char *ptr = res.response;
                int remaining = MAX_MSG_LEN - 1;
                int written = snprintf(ptr, remaining, "%-20s %-10s %-10s %-20s\n", "NAME", "INDEX", "STATE", "INFO");
                ptr += written; remaining -= written;
                
                for (int i = 0; i < tm->num_processes; i++) {
                    Process *p = &tm->processes[i];
                    written = snprintf(ptr, remaining, "%-20s %-10d %-10s pid %d\n", 
                        p->config->name, p->proc_index, state_to_string(p->state), p->pid);
                    ptr += written; remaining -= written;
                    if (remaining <= 0) break;
                }
            }
            break;
        case CMD_START:
            log_event("Client requested start: %s", req.payload);
            for (int i = 0; i < tm->num_processes; i++) {
                if (strcmp(tm->processes[i].config->name, req.payload) == 0) {
                    start_process(&tm->processes[i]);
                }
            }
            snprintf(res.response, MAX_MSG_LEN, "Started %s\n", req.payload);
            break;
        case CMD_STOP:
            log_event("Client requested stop: %s", req.payload);
            for (int i = 0; i < tm->num_processes; i++) {
                if (strcmp(tm->processes[i].config->name, req.payload) == 0) {
                    stop_process(&tm->processes[i]);
                }
            }
            snprintf(res.response, MAX_MSG_LEN, "Stopped %s\n", req.payload);
            break;
        case CMD_RELOAD:
            g_reload_requested = 1;
            snprintf(res.response, MAX_MSG_LEN, "Reload requested\n");
            break;
        case CMD_SHUTDOWN:
            tm->running = false;
            snprintf(res.response, MAX_MSG_LEN, "Daemon shutting down\n");
            break;
        default:
            res.success = false;
            snprintf(res.response, MAX_MSG_LEN, "Unknown command\n");
    }

    send(client_fd, &res, sizeof(res), 0);
}

int main(int argc, char **argv) {
    openlog("taskmasterd", LOG_PID | LOG_CONS, LOG_DAEMON);
    memset(&g_tm, 0, sizeof(Taskmaster));
    g_tm.running = true;

    signal(SIGHUP, handle_sighup);
    signal(SIGCHLD, SIG_DFL);

    if (argc > 1) g_config_path = strdup(argv[1]);
    else g_config_path = strdup(DEFAULT_CONFIG_DIR);

    struct stat st;
    if (stat(g_config_path, &st) == 0 && S_ISDIR(st.st_mode)) parse_config_dir(g_config_path, &g_tm);
    else parse_config(g_config_path, &g_tm);

    g_tm.num_processes = 0;
    for (int i = 0; i < g_tm.num_configs; i++) g_tm.num_processes += g_tm.configs[i].numprocs;
    g_tm.processes = calloc(g_tm.num_processes, sizeof(Process));
    int proc_idx = 0;
    for (int i = 0; i < g_tm.num_configs; i++) {
        for (int j = 0; j < g_tm.configs[i].numprocs; j++) {
            g_tm.processes[proc_idx].config = &g_tm.configs[i];
            g_tm.processes[proc_idx].proc_index = j;
            if (g_tm.configs[i].autostart) start_process(&g_tm.processes[proc_idx]);
            proc_idx++;
        }
    }

    setup_server_socket(&g_tm);
    log_event("Daemon started, config: %s", g_config_path);

    while (g_tm.running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_tm.server_fd, &readfds);

        struct timeval tv = {1, 0};
        int ret = select(g_tm.server_fd + 1, &readfds, NULL, NULL, &tv);

        if (ret > 0 && FD_ISSET(g_tm.server_fd, &readfds)) {
            int client_fd = accept(g_tm.server_fd, NULL, NULL);
            if (client_fd >= 0) {
                handle_client(&g_tm, client_fd);
                close(client_fd);
            }
        }

        if (g_reload_requested) {
            g_reload_requested = 0;
            reload_config(&g_tm, g_config_path);
        }
        update_processes(&g_tm);
    }

    unlink(SOCKET_PATH);
    free(g_config_path);
    return 0;
}
