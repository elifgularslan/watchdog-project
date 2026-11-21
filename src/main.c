#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "signal_manager.h"
#include "process_manager.h"
#include "restart_manager.h"
#include "logger.h"

#define LOG_FILE "watchdog.log"
#define CHECK_INTERVAL 1
#define SHUTDOWN_TIMEOUT 5

/* CPU'nun gereksiz kullanilmasini engeller */
static void wait_short(void)
{
    struct timespec delay = {0, 100000000L};
    nanosleep(&delay, NULL);
}

/* Aktif child process olup olmadigini kontrol eder */
static int has_active_processes(void)
{
    int count = 0;
    ProcessInfo *processes = process_manager_get_list(&count);

    for (int i = 0; i < count; i++)
    {
        if (processes[i].pid > 0)
            return 1;
    }

    return 0;
}

/* Sinyal bayraklarini guvenli sekilde okur ve temizler */
static int take_events(int *child, int *report, int *timer)
{
    sigset_t mask, old_mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == -1)
        return -1;

    *child = g_sigchld_received;
    *report = g_sigusr1_received;
    *timer = g_sigalrm_received;

    g_sigchld_received = 0;
    g_sigusr1_received = 0;
    g_sigalrm_received = 0;

    return sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

/* Guvenli kapanis mekanizmasi */
static void graceful_shutdown(void)
{
    g_shutdown_requested = 1;

    /* Periyodik zamanlayiciyi durdur */
    struct itimerval timer = {0};

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
        logger_log("Timer could not be stopped.");

    logger_log("Graceful shutdown started.");

    /* Child process'lere kapanis sinyali gonder */
    process_manager_terminate_all(SIGTERM);

    /* Kapanmayan process'ler icin timeout mekanizmasi */
    if (has_active_processes())
        restart_manager_shutdown_timeout(SHUTDOWN_TIMEOUT);

    /* Sonlanan child process'leri topla */
    while (has_active_processes())
    {
        process_manager_handle_sigchld();

        if (has_active_processes())
            wait_short();
    }

    logger_log("All child processes terminated.");
    logger_log("Watchdog stopped.");
}

int main(void)
{
    int exit_code = EXIT_SUCCESS;

    /* Sinyal yoneticisini baslat */
    if (signal_manager_init() < 0)
    {
        fprintf(stderr, "Signal manager initialization failed.\n");
        return EXIT_FAILURE;
    }

    /* Log sistemini baslat */
    if (logger_init(LOG_FILE) < 0)
    {
        fprintf(stderr, "Logger initialization failed.\n");
        return EXIT_FAILURE;
    }

    logger_log("Watchdog starting. PID: %ld", (long)getpid());

    /* Process yoneticisini baslat */
    if (process_manager_init() < 0)
    {
        logger_log("Process manager initialization failed.");
        logger_close();
        return EXIT_FAILURE;
    }

    /* Restart yoneticisini baslat */
    if (restart_manager_init() < 0)
    {
        logger_log("Restart manager initialization failed.");
        logger_close();
        return EXIT_FAILURE;
    }

    printf("Watchdog PID: %ld\n", (long)getpid());

    /* En az iki child process baslat */
    if (!g_shutdown_requested)
    {
        int started = process_manager_start_all();

        if (started < 2)
        {
            logger_log("Failed to start at least two workers.");
            exit_code = EXIT_FAILURE;
            g_shutdown_requested = 1;
        }
    }

    /* Periyodik kontrol zamanlayicisini baslat */
    if (!g_shutdown_requested &&
        process_manager_setup_timer(CHECK_INTERVAL) < 0)
    {
        logger_log("Timer initialization failed.");
        exit_code = EXIT_FAILURE;
        g_shutdown_requested = 1;
    }

    /* Ana kontrol dongusu */
    while (!g_shutdown_requested)
    {
        int child, report, timer;

        if (take_events(&child, &report, &timer) == -1)
        {
            logger_log("Signal mask operation failed.");
            exit_code = EXIT_FAILURE;
            break;
        }

        if (g_shutdown_requested)
            break;

        /* SIGCHLD: Sonlanan child process'leri kontrol et */
        if (child)
            process_manager_handle_sigchld();

        if (g_shutdown_requested)
            break;

        /* SIGUSR1: Durum raporu olustur */
        if (report)
        {
            int count = 0;
            ProcessInfo *processes =
                process_manager_get_list(&count);

            logger_report_status(processes, count);
        }

        /* SIGALRM: Periyodik kontrol */
        if (timer && !g_shutdown_requested)
        {
            process_manager_periodic_check();

            if (!g_shutdown_requested)
                restart_manager_periodic_check();
        }

        if (!g_shutdown_requested)
            wait_short();
    }

    /* Programi guvenli sekilde kapat */
    graceful_shutdown();

    logger_close();

    return exit_code;
}