#include "taskmaster.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

const char *state_to_string(ProcessState state) {
    switch (state) {
        case STATE_STOPPED: return "STOPPED";
        case STATE_STARTING: return "STARTING";
        case STATE_RUNNING: return "RUNNING";
        case STATE_EXITED: return "EXITED";
        case STATE_FATAL: return "FATAL";
        case STATE_STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

void start_process(Process *proc) {
    proc->state = STATE_STARTING;
    proc->start_time = time(NULL);
    
    pid_t pid = fork();
    if (pid == 0) {
        // 1. Open files as root (if we are root) before dropping privileges
        if (strlen(proc->config->stdout_path) > 0) {
            int fd = open(proc->config->stdout_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
            else { perror("open stdout"); }
        }
        if (strlen(proc->config->stderr_path) > 0) {
            int fd = open(proc->config->stderr_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) { dup2(fd, STDERR_FILENO); close(fd); }
            else { perror("open stderr"); }
        }

        // 2. Privilege De-escalation
        if (strlen(proc->config->user) > 0) {
            struct passwd *pw = getpwnam(proc->config->user);
            if (pw) {
                if (setgid(pw->pw_gid) != 0) {
                    perror("setgid");
                    exit(1);
                }
                if (setuid(pw->pw_uid) != 0) {
                    perror("setuid");
                    exit(1);
                }
            } else {
                fprintf(stderr, "Error: User %s not found\n", proc->config->user);
                exit(1);
            }
        }

        // 3. Set Environment Variables
        for (int i = 0; i < proc->config->num_env; i++) {
            char *env_str = strdup(proc->config->env[i]);
            char *key = strtok(env_str, "=");
            char *val = strtok(NULL, "");
            if (key) {
                setenv(key, val ? val : "", 1);
            }
            free(env_str);
        }

        umask(proc->config->umask);
        if (strlen(proc->config->workingdir) > 0) {
            if (chdir(proc->config->workingdir) != 0) {
                perror("chdir");
                exit(1);
            }
        }

        char *args[64];
        char *cmd_copy = strdup(proc->config->cmd);
        int i = 0;
        char *token = strtok(cmd_copy, " ");
        while (token && i < 63) { args[i++] = token; token = strtok(NULL, " "); }
        args[i] = NULL;

        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        proc->pid = pid;
        log_event("Started process %s[%d] (PID %d)", proc->config->name, proc->proc_index, pid);
    } else {
        perror("fork");
        proc->state = STATE_FATAL;
    }
}

void stop_process(Process *proc) {
    if (proc->pid > 0) {
        log_event("Stopping process %s[%d] (PID %d) with signal %d", proc->config->name, proc->proc_index, proc->pid, proc->config->stopsignal);
        kill(proc->pid, proc->config->stopsignal);
        proc->state = STATE_STOPPING;
    }
}

void update_processes(Taskmaster *tm) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < tm->num_processes; i++) {
            if (tm->processes[i].pid == pid) {
                Process *proc = &tm->processes[i];
                proc->pid = 0;
                proc->stop_time = time(NULL);
                
                bool expected = false;
                if (WIFEXITED(status)) {
                    int code = WEXITSTATUS(status);
                    for (int j = 0; j < proc->config->num_exitcodes; j++) {
                        if (proc->config->exitcodes[j] == code) { expected = true; break; }
                    }
                    if (proc->config->num_exitcodes == 0 && code == 0) expected = true;
                    log_event("Process %s[%d] exited with code %d (%s)", 
                        proc->config->name, proc->proc_index, code, expected ? "expected" : "unexpected");
                } else if (WIFSIGNALED(status)) {
                    log_event("Process %s[%d] killed by signal %d", proc->config->name, proc->proc_index, WTERMSIG(status));
                }

                if (proc->state == STATE_STOPPING) {
                    proc->state = STATE_STOPPED;
                } else {
                    proc->state = STATE_EXITED;
                    bool should_restart = false;
                    if (proc->config->autorestart == RESTART_ALWAYS) should_restart = true;
                    else if (proc->config->autorestart == RESTART_UNEXPECTED && !expected) should_restart = true;

                    if (should_restart && proc->restart_count < proc->config->startretries) {
                        proc->restart_count++;
                        log_event("Restarting process %s[%d] (attempt %d)", proc->config->name, proc->proc_index, proc->restart_count);
                        start_process(proc);
                    } else if (should_restart) {
                        proc->state = STATE_FATAL;
                        log_event("Process %s[%d] failed to start after %d retries", proc->config->name, proc->proc_index, proc->config->startretries);
                    }
                }
                break;
            }
        }
    }

    time_t now = time(NULL);
    for (int i = 0; i < tm->num_processes; i++) {
        Process *proc = &tm->processes[i];
        if (proc->state == STATE_STARTING && (now - proc->start_time) >= proc->config->starttime) {
            proc->state = STATE_RUNNING;
            proc->restart_count = 0;
        }
    }
}
