#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

static volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        keep_running = 0;
    }
}

int main(int argc, char *argv[]) {
    char worker_name[32] = "Worker";
    int lifespan = -1; /* Calisma suresi (sn), -1 ise sonsuz */

    if (argc > 1) {
        strncpy(worker_name, argv[1], sizeof(worker_name) - 1);
        worker_name[sizeof(worker_name) - 1] = '\0';
    }

    if (argc > 2) {
        lifespan = atoi(argv[2]);
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
#endif

    printf("[%s] Baslatildi (PID: %d)\n", worker_name, getpid());
    fflush(stdout);

    int elapsed = 0;
    while (keep_running) {
        sleep(1);
        elapsed++;

        if (lifespan > 0 && elapsed >= lifespan) {
            printf("[%s] (PID: %d) Calisma suresi bitti, cikiyor.\n", worker_name, getpid());
            fflush(stdout);
            return EXIT_SUCCESS;
        }
    }

    printf("[%s] (PID: %d) Sinyal alindi, kapaniyor.\n", worker_name, getpid());
    fflush(stdout);
    return EXIT_SUCCESS;
}
