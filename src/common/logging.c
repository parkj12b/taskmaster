#include "taskmaster.h"
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>

void log_event(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // Log to syslog
    vsyslog(LOG_INFO, format, args);
    
    // Mirror to stderr
    va_end(args);
    va_start(args, format);
    fprintf(stderr, "log: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}
