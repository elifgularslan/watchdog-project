#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE *g_log_file = NULL;

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info != NULL) {
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(buffer, size, "0000-00-00 00:00:00");
    }
}

static const char *process_state_to_string(ProcessState state) {
    switch (state) {
        case PROCESS_STATE_STOPPED:
            return "STOPPED";
        case PROCESS_STATE_RUNNING:
            return "RUNNING";
        case PROCESS_STATE_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

int logger_init(const char *log_file_path) {
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    const char *path = (log_file_path != NULL && log_file_path[0] != '\0')
                           ? log_file_path
                           : "watchdog.log";

    g_log_file = fopen(path, "a");
    if (g_log_file == NULL) {
        perror("logger_init: log dosyasi acilamadi");
        return -1;
    }

    return 0;
}

void logger_log(const char *format, ...) {
    if (g_log_file == NULL || format == NULL) {
        return;
    }

    char timestamp[32];
    char message[1024];
    va_list args;

    get_timestamp(timestamp, sizeof(timestamp));

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    size_t len = strlen(message);
    int has_newline = (len > 0 && message[len - 1] == '\n');

    fprintf(g_log_file, "[%s] %s%s", timestamp, message, has_newline ? "" : "\n");
    fflush(g_log_file);
}

void logger_report_status(ProcessInfo *processes, int count) {
    if (g_log_file == NULL) {
        return;
    }

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(g_log_file, "[%s] === STATUS REPORT (SIGUSR1) ===\n", timestamp);

    if (processes == NULL || count <= 0) {
        fprintf(g_log_file, "[%s] No managed child processes.\n", timestamp);
    } else {
        for (int i = 0; i < count; i++) {
            const char *name = (processes[i].name[0] != '\0')
                                   ? processes[i].name
                                   : "unnamed";

            fprintf(g_log_file,
                    "[%s] Process[%d] | Name: %s | PID: %ld | State: %s | Restarts: %d\n",
                    timestamp,
                    i,
                    name,
                    (long)processes[i].pid,
                    process_state_to_string(processes[i].state),
                    processes[i].restart_count);
        }
    }

    fprintf(g_log_file, "[%s] === END OF STATUS REPORT ===\n", timestamp);
    fflush(g_log_file);
}

void logger_close(void) {
    if (g_log_file != NULL) {
        fflush(g_log_file);
        fclose(g_log_file);
        g_log_file = NULL;
    }
}