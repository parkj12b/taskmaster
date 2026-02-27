#include "taskmaster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

static char *trim_whitespace(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static int get_signal_number(const char *sig) {
    if (strcmp(sig, "TERM") == 0) return SIGTERM;
    if (strcmp(sig, "KILL") == 0) return SIGKILL;
    if (strcmp(sig, "HUP") == 0) return SIGHUP;
    if (strcmp(sig, "INT") == 0) return SIGINT;
    if (strcmp(sig, "USR1") == 0) return SIGUSR1;
    if (strcmp(sig, "USR2") == 0) return SIGUSR2;
    return SIGTERM;
}

void parse_config(const char *path, Taskmaster *tm) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("fopen");
        return;
    }

    if (tm->configs == NULL) {
        tm->configs = malloc(sizeof(ProgramConfig) * MAX_PROCS);
        tm->num_configs = 0;
    }

    char line[MAX_CMD_LEN];
    ProgramConfig *current_config = NULL;
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *trimmed = trim_whitespace(line);
        int current_indent = (int)(trimmed - line);
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        if (strncmp(trimmed, "programs:", 9) == 0) continue;

        // Check for program name (indented)
        if ((line[0] == ' ' || line[0] == '\t') && strchr(trimmed, ':') && !strchr(trimmed + (strchr(trimmed, ':') - trimmed + 1), ':')) {
            char *colon = strchr(trimmed, ':');
            if (colon && *(colon + 1) == '\0') {
                char name[MAX_NAME_LEN];
                strncpy(name, trimmed, MAX_NAME_LEN - 1);
                name[colon - trimmed] = '\0';
                
                // List of known properties to avoid misidentification
                const char *props[] = {"cmd", "numprocs", "umask", "workingdir", "autostart", 
                                       "autorestart", "exitcodes", "startretries", "starttime", 
                                       "stopsignal", "stoptime", "stdout", "stderr", "env", "user", NULL};
                bool is_prop = false;
                for (int i = 0; props[i]; i++) {
                    if (strcmp(name, props[i]) == 0) {
                        is_prop = true;
                        break;
                    }
                }

                if (!is_prop) {
                    // New program
                    if (tm->num_configs >= MAX_PROCS) {
                        log_event("Error: Maximum number of programs reached (%d)", MAX_PROCS);
                        break;
                    }
                    current_config = &tm->configs[tm->num_configs++];
                    memset(current_config, 0, sizeof(ProgramConfig));
                    strncpy(current_config->name, name, MAX_NAME_LEN - 1);
                    current_config->numprocs = 1;
                    current_config->stopsignal = SIGTERM;
                    current_config->autostart = true;
                    continue;
                }
            }
        }

                if (current_config && strchr(trimmed, ':')) {
                    char *key = strtok(trimmed, ":");
                    char *value = strtok(NULL, "");
                    if (value) {
                        value = trim_whitespace(value);
                        // Strip leading/trailing quotes if present
                        if (value[0] == '"') {
                            value++;
                            char *end = strrchr(value, '"');
                            if (end) *end = '\0';
                        }
                    }
        
                    if (strcmp(key, "cmd") == 0) {
                        if (!value) {
                            log_event("Config error in %s at line %d: missing value for key 'cmd'", path, line_num);
                            continue;
                        }
                        strncpy(current_config->cmd, value, MAX_CMD_LEN - 1);
                    } else if (strcmp(key, "numprocs") == 0) {
                        if (value) current_config->numprocs = atoi(value);
                    } else if (strcmp(key, "umask") == 0) {
                        if (value) current_config->umask = strtol(value, NULL, 8);
                    } else if (strcmp(key, "workingdir") == 0) {
                        if (value) strncpy(current_config->workingdir, value, MAX_CMD_LEN - 1);
                    } else if (strcmp(key, "autostart") == 0) {
                        if (value) current_config->autostart = (strcmp(value, "true") == 0);
                    } else if (strcmp(key, "user") == 0) {
                        if (value) strncpy(current_config->user, value, MAX_NAME_LEN - 1);
                    } else if (strcmp(key, "autorestart") == 0) {
                        if (value) {
                            if (strcmp(value, "always") == 0) current_config->autorestart = RESTART_ALWAYS;
                            else if (strcmp(value, "unexpected") == 0) current_config->autorestart = RESTART_UNEXPECTED;
                            else if (strcmp(value, "never") == 0) current_config->autorestart = RESTART_NEVER;
                            else log_event("Config warning in %s at line %d: unknown autorestart policy '%s'", path, line_num, value);
                        }
                    } else if (strcmp(key, "env") == 0) {
                        long pos = ftell(file);
                        char next_line[MAX_CMD_LEN];
                        while (fgets(next_line, sizeof(next_line), file)) {
                            line_num++;
                            char *trimmed_next = trim_whitespace(next_line);
                            int next_indent = (int)(trimmed_next - next_line);
                            if (trimmed_next[0] == '\0') {
                                pos = ftell(file);
                                continue;
                            }
                            if (next_indent <= current_indent) {
                                fseek(file, pos, SEEK_SET);
                                line_num--;
                                break;
                            }
                            if (strchr(trimmed_next, ':')) {
                                char *ekey = strtok(trimmed_next, ":");

                                if (current_config->num_env < MAX_ENV_VARS) {
                                    char *evalue = strtok(NULL, "");
                                    if (evalue) {
                                        evalue = trim_whitespace(evalue);
                                        if (evalue[0] == '"') {
                                            evalue++;
                                            char *qend = strrchr(evalue, '"');
                                            if (qend) *qend = '\0';
                                        }
                                    }
                                    
                                    char env_buf[MAX_CMD_LEN];
                                    snprintf(env_buf, sizeof(env_buf), "%s=%s", ekey, evalue ? evalue : "");
                                    current_config->env[current_config->num_env++] = strdup(env_buf);
                                }
                                pos = ftell(file);
                            } else {
                                fseek(file, pos, SEEK_SET);
                                line_num--;
                                break;
                            }
                        }
                    } else if (strcmp(key, "exitcodes") == 0) {
                        long pos = ftell(file);
                        char next_line[MAX_CMD_LEN];
                        while (fgets(next_line, sizeof(next_line), file)) {
                            line_num++;
                            char *trimmed_next = trim_whitespace(next_line);
                            int next_indent = (int)(trimmed_next - next_line);
                            if (trimmed_next[0] == '\0') {
                                pos = ftell(file);
                                continue;
                            }
                            if (next_indent <= current_indent) {
                                fseek(file, pos, SEEK_SET);
                                line_num--;
                                break;
                            }
                            if (trimmed_next[0] == '-') {
                                char *val = trim_whitespace(trimmed_next + 1);
                                if (val && current_config->num_exitcodes < MAX_EXIT_CODES) {
                                    current_config->exitcodes[current_config->num_exitcodes++] = atoi(val);
                                }
                                pos = ftell(file);
                            } else {
                                fseek(file, pos, SEEK_SET);
                                line_num--;
                                break;
                            }
                        }
                    } else if (strcmp(key, "starttime") == 0) {
                        if (value) current_config->starttime = atoi(value);
                    } else if (strcmp(key, "startretries") == 0) {
                        if (value) current_config->startretries = atoi(value);
                    } else if (strcmp(key, "stopsignal") == 0) {
                        if (value) current_config->stopsignal = get_signal_number(value);
                    } else if (strcmp(key, "stoptime") == 0) {
                        if (value) current_config->stoptime = atoi(value);
                    } else if (strcmp(key, "stdout") == 0) {
                        if (value) strncpy(current_config->stdout_path, value, MAX_CMD_LEN - 1);
                    } else if (strcmp(key, "stderr") == 0) {
                        if (value) strncpy(current_config->stderr_path, value, MAX_CMD_LEN - 1);
                    } else {
                        log_event("Config warning in %s at line %d: unknown property '%s'", path, line_num, key);
                    }
                }
         else if (trimmed[0] != '\0') {
             log_event("Config error in %s at line %d: unexpected indentation or syntax: '%s'", path, line_num, trimmed);
        }
    }
    fclose(file);
}

void parse_config_dir(const char *path, Taskmaster *tm) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".conf") == 0)) {
                char full_path[MAX_CMD_LEN];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                parse_config(full_path, tm);
            }
        }
    }
    closedir(dir);
}

static bool configs_equal(ProgramConfig *a, ProgramConfig *b) {
    if (strcmp(a->name, b->name) != 0) return false;
    if (strcmp(a->cmd, b->cmd) != 0) return false;
    if (a->numprocs != b->numprocs) return false;
    if (a->umask != b->umask) return false;
    if (strcmp(a->workingdir, b->workingdir) != 0) return false;
    if (a->autostart != b->autostart) return false;
    if (a->autorestart != b->autorestart) return false;
    if (a->starttime != b->starttime) return false;
    if (a->startretries != b->startretries) return false;
    if (a->stopsignal != b->stopsignal) return false;
    if (a->stoptime != b->stoptime) return false;
    if (strcmp(a->stdout_path, b->stdout_path) != 0) return false;
    if (strcmp(a->stderr_path, b->stderr_path) != 0) return false;
    if (strcmp(a->user, b->user) != 0) return false;
    if (a->num_exitcodes != b->num_exitcodes) return false;
    for (int i = 0; i < a->num_exitcodes; i++) {
        if (a->exitcodes[i] != b->exitcodes[i]) return false;
    }
    if (a->num_env != b->num_env) return false;
    for (int i = 0; i < a->num_env; i++) {
        if (strcmp(a->env[i], b->env[i]) != 0) return false;
    }
    return true;
}

static int find_config_index(ProgramConfig *configs, int num_configs, const char *name) {
    for (int i = 0; i < num_configs; i++) {
        if (strcmp(configs[i].name, name) == 0) return i;
    }
    return -1;
}

static int total_processes_for_configs(ProgramConfig *configs, int num_configs) {
    int total = 0;
    for (int i = 0; i < num_configs; i++) total += configs[i].numprocs;
    return total;
}

void reload_config(Taskmaster *tm, const char *config_path) {
    Taskmaster next_tm;
    memset(&next_tm, 0, sizeof(Taskmaster));
    
    struct stat st;
    if (stat(config_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        parse_config_dir(config_path, &next_tm);
    } else {
        parse_config(config_path, &next_tm);
    }

    log_event("Reloading configuration from %s", config_path);

    ProgramConfig *old_configs = tm->configs;
    int old_num_configs = tm->num_configs;
    Process *old_processes = tm->processes;
    int old_num_processes = tm->num_processes;
    int new_num_processes = total_processes_for_configs(next_tm.configs, next_tm.num_configs);

    Process *new_processes = NULL;
    bool *old_used = NULL;
    bool *preserved = NULL;

    if (new_num_processes > 0) {
        new_processes = calloc(new_num_processes, sizeof(Process));
        preserved = calloc(new_num_processes, sizeof(bool));
    }
    if (old_num_processes > 0) old_used = calloc(old_num_processes, sizeof(bool));

    if ((new_num_processes > 0 && (!new_processes || !preserved)) ||
        (old_num_processes > 0 && !old_used)) {
        log_event("Reload failed: memory allocation failure");
        free(new_processes);
        free(preserved);
        free(old_used);
        free(next_tm.configs);
        return;
    }

    int dst_index = 0;
    for (int i = 0; i < next_tm.num_configs; i++) {
        int old_cfg_idx = find_config_index(old_configs, old_num_configs, next_tm.configs[i].name);
        bool unchanged = false;
        if (old_cfg_idx >= 0) {
            unchanged = configs_equal(&old_configs[old_cfg_idx], &next_tm.configs[i]);
        }

        for (int inst = 0; inst < next_tm.configs[i].numprocs; inst++) {
            Process *dst = &new_processes[dst_index];
            if (unchanged) {
                for (int j = 0; j < old_num_processes; j++) {
                    if (old_used[j]) continue;
                    if (old_processes[j].config == &old_configs[old_cfg_idx] &&
                        old_processes[j].proc_index == inst) {
                        *dst = old_processes[j];
                        dst->config = &next_tm.configs[i];
                        old_used[j] = true;
                        preserved[dst_index] = true;
                        break;
                    }
                }
            }
            if (!preserved[dst_index]) {
                dst->config = &next_tm.configs[i];
                dst->proc_index = inst;
                dst->state = STATE_STOPPED;
            }
            dst_index++;
        }
    }

    // Stop processes that are no longer represented in the new config
    for (int i = 0; i < old_num_processes; i++) {
        if (old_used[i]) continue;
        if (old_processes[i].pid > 0) {
            log_event("Stopping outdated process %s[%d] during reload",
                      old_processes[i].config->name, old_processes[i].proc_index);
            stop_process(&old_processes[i]);
        }
    }

    // Swap process/config tables
    tm->configs = next_tm.configs;
    tm->num_configs = next_tm.num_configs;
    tm->processes = new_processes;
    tm->num_processes = new_num_processes;

    // Autostart new/changed process instances
    for (int i = 0; i < tm->num_processes; i++) {
        if (preserved[i]) continue;
        if (tm->processes[i].config->autostart) {
            log_event("Starting process %s[%d] due to reload",
                      tm->processes[i].config->name, tm->processes[i].proc_index);
            start_process(&tm->processes[i]);
        }
    }

    log_event("Reload complete (applied %d programs, %d process slots)",
              tm->num_configs, tm->num_processes);

    free(old_used);
    free(preserved);
    free(old_processes);
    free(old_configs);
}
