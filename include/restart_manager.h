#ifndef RESTART_MANAGER_H
#define RESTART_MANAGER_H

#include "process_manager.h"

int restart_manager_init(void);
int restart_manager_handle_exit(int process_index, int exit_status, int is_signaled);
void restart_manager_periodic_check(void);
void restart_manager_shutdown_timeout(int timeout_seconds);

#endif /* RESTART_MANAGER_H */
