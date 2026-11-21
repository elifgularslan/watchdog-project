#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

/* Sinyal bayraklari */
extern volatile sig_atomic_t g_shutdown_requested;
extern volatile sig_atomic_t g_sigusr1_received;
extern volatile sig_atomic_t g_sigchld_received;

int signal_manager_init(void);

#endif /* SIGNAL_MANAGER_H */
