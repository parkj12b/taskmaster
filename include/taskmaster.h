#ifndef TASKMASTER_H
#define TASKMASTER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <grp.h>

#define MAX_CMD_LEN 1024
#define MAX_ENV_VARS 64
#define MAX_EXIT_CODES 32
#define MAX_PROCS 100
#define MAX_NAME_LEN 64
#define DEFAULT_CONFIG_DIR "/etc/taskmaster"

#include "protocol.h"

typedef enum {
    RESTART_NEVER,
    RESTART_ALWAYS,
    RESTART_UNEXPECTED
} RestartPolicy;

typedef enum {
    STATE_STOPPED,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_EXITED,
    STATE_FATAL,
    STATE_STOPPING
} ProcessState;

typedef struct {
    char name[MAX_NAME_LEN];
    char cmd[MAX_CMD_LEN];
    int numprocs;
    mode_t umask;
    char workingdir[MAX_CMD_LEN];
    bool autostart;
    RestartPolicy autorestart;
    int exitcodes[MAX_EXIT_CODES];
    int num_exitcodes;
    int starttime;
    int startretries;
    int stopsignal;
    int stoptime;
    char stdout_path[MAX_CMD_LEN];
    char stderr_path[MAX_CMD_LEN];
    char *env[MAX_ENV_VARS];
    int num_env;
    char user[MAX_NAME_LEN]; // New field for privilege de-escalation
} ProgramConfig;

typedef struct {
    pid_t pid;
    ProcessState state;
    time_t start_time;
    time_t stop_time;
    int restart_count;
    ProgramConfig *config;
    int proc_index;
} Process;

typedef struct {
    ProgramConfig *configs;
    int num_configs;
    Process *processes;
    int num_processes;
    char *log_file;
    bool running;
    int server_fd;
} Taskmaster;

// Shared Core Logic
void log_event(const char *format, ...);
void update_processes(Taskmaster *tm);
void start_process(Process *proc);
void stop_process(Process *proc);
void parse_config(const char *path, Taskmaster *tm);
void parse_config_dir(const char *path, Taskmaster *tm);
void reload_config(Taskmaster *tm, const char *config_path);
const char *state_to_string(ProcessState state);

// Daemon Specific
void handle_client(Taskmaster *tm, int client_fd);

#endif
