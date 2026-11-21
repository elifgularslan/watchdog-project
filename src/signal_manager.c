#define _POSIX_C_SOURCE 200809L

#include "signal_manager.h"
#include <stddef.h>

/* Sinyal durum bayraklari */
volatile sig_atomic_t g_shutdown_requested = 0;
volatile sig_atomic_t g_sigusr1_received = 0;
volatile sig_atomic_t g_sigchld_received = 0;
volatile sig_atomic_t g_sigalrm_received = 0;

/* Gelen sinyalleri yakalayan handler */
static void signal_handler(int signo)
{
    switch (signo)
    {
        case SIGINT:
        case SIGTERM:
            g_shutdown_requested = 1;
            break;

        case SIGUSR1:
            g_sigusr1_received = 1;
            break;

        case SIGCHLD:
            g_sigchld_received = 1;
            break;
        case SIGALRM:
            g_sigalrm_received = 1;
            break;

        default:
            break;
    }
}

/* Sinyal yoneticisini baslatir */
int signal_manager_init(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = signal_handler;

    if (sigemptyset(&sa.sa_mask) == -1)
        return -1;

    sa.sa_flags = 0;

    /* Guvenli kapanis sinyalleri */
    if (sigaction(SIGINT, &sa, NULL) == -1)
        return -1;

    if (sigaction(SIGTERM, &sa, NULL) == -1)
        return -1;

    /* Durum raporlama sinyali */
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
        return -1;
    
      /* Periyodik kontrol sinyali */
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        return -1;

    /* Child process sonlanma sinyali */
    sa.sa_flags = SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        return -1;

    return 0;
}