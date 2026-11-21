#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include <time.h>

#define MAX_PROCESSES 10
#define MAX_RESTARTS 3

/* Surec durumlari */
typedef enum {
    PROCESS_STATE_STOPPED = 0,
    PROCESS_STATE_RUNNING = 1,
    PROCESS_STATE_TERMINATED = 2
} ProcessState;

/* Surec bilgisi */
typedef struct {
    pid_t pid;
    char name[32];
    char executable_path[256];
    char *args[16];
    ProcessState state;
    int restart_count;
    time_t start_time;
} ProcessInfo;

int process_manager_init(void);
int process_manager_start_all(void);
int process_manager_spawn(int index);
void process_manager_handle_sigchld(void);
void process_manager_terminate_all(int signal_num);
ProcessInfo* process_manager_get_list(int *count);
ProcessInfo* process_manager_get_by_pid(pid_t pid);
int process_manager_setup_timer(int interval_seconds);
void process_manager_periodic_check(void);

#endif /* PROCESS_MANAGER_H */
