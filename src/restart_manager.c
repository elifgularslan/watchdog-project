#define _POSIX_C_SOURCE 200809L

#include "restart_manager.h"
#include "signal_manager.h"
#include "logger.h"

#include <signal.h>
#include <string.h>
#include <time.h>


//Sadece beklenmedik sonlanma veya baslatma hatasi nedeniyle yeniden baslatilmayi bekleyen surecler.
static int pending_restart[MAX_PROCESSES];

//Bir sureci yeniden baslatmayi dener.
static int try_restart(int index)
{
    int count = 0;
    ProcessInfo *processes = process_manager_get_list(&count);

    if (processes == NULL ||
        index < 0 ||
        index >= count ||
        index >= MAX_PROCESSES)
    {
        return -1;
    }

    ProcessInfo *process = &processes[index];

    // Kapanis sirasinda yeni surec baslatilmaz.
    if (g_shutdown_requested)
    {
        pending_restart[index] = 0;
        return 0;
    }

    if (!pending_restart[index])
        return 0;

    // Zaten aktif bir surec varsa ikinci kopyasini baslatma.
    if (process->pid > 0)
    {
        pending_restart[index] = 0;
        return 0;
    }

    if (process->restart_count >= MAX_RESTARTS)
    {
        logger_log(
            "Restart limit reached: %s. No further restart attempts.",
            process->name
        );

        pending_restart[index] = 0;
        return 0;
    }

    //Basarisiz fork denemeleri de limite dahildir. Boylece baslatma hatasinda sonsuz deneme yapilmaz.
    process->restart_count++;

    logger_log(
        "Restart attempt: %s (%d/%d).",
        process->name,
        process->restart_count,
        MAX_RESTARTS
    );

    if (process_manager_spawn(index) < 0)
    {
        logger_log(
            "Restart failed: %s (%d/%d).",
            process->name,
            process->restart_count,
            MAX_RESTARTS
        );

        if (process->restart_count >= MAX_RESTARTS)
        {
            pending_restart[index] = 0;

            logger_log(
                "Restart limit reached: %s.",
                process->name
            );
        }

        return -1;
    }

    pending_restart[index] = 0;

    logger_log(
        "Process restarted: %s, PID: %ld, restarts: %d.",
        process->name,
        (long)process->pid,
        process->restart_count
    );

    return 1;
}

int restart_manager_init(void)
{
    memset(pending_restart, 0, sizeof(pending_restart));
    return 0;
}

int restart_manager_handle_exit(
    int process_index,
    int exit_status,
    int is_signaled
)
{
    int count = 0;
    ProcessInfo *processes = process_manager_get_list(&count);

    if (processes == NULL ||
        process_index < 0 ||
        process_index >= count ||
        process_index >= MAX_PROCESSES)
    {
        return -1;
    }

    ProcessInfo *process = &processes[process_index];

    if (g_shutdown_requested)
    {
        pending_restart[process_index] = 0;

        logger_log(
            "Restart skipped during shutdown: %s.",
            process->name
        );

        return 0;
    }

    // Basarili, normal cikis restart gerektirmez.
    if (!is_signaled && exit_status == 0)
    {
        pending_restart[process_index] = 0;
        return 0;
    }

    logger_log(
        "Unexpected termination: %s, %s: %d.",
        process->name,
        is_signaled ? "signal" : "exit code",
        exit_status
    );

    pending_restart[process_index] = 1;

    return try_restart(process_index);
}
void restart_manager_periodic_check(void)
{
    if (g_shutdown_requested)
    {
        memset(pending_restart, 0, sizeof(pending_restart));
        return;
    }

    int count = 0;
    ProcessInfo *processes = process_manager_get_list(&count);

    if (processes == NULL)
        return;

    for (int i = 0; i < count && i < MAX_PROCESSES; i++)
    {
        if (g_shutdown_requested)
            break;

        //Yalnizca bekleyen restart denemelerini ele alıp normal kapanan surecleri yeniden baslatma.
        if (pending_restart[i])
            try_restart(i);
    }
}
// Hala toplanmamis child process var mi?
static int has_active_children(void)
{
    int count = 0;
    ProcessInfo *processes = process_manager_get_list(&count);

    if (processes == NULL)
        return 0;

    for (int i = 0; i < count; i++)
    {
        if (processes[i].pid > 0)
            return 1;
    }

    return 0;
}

void restart_manager_shutdown_timeout(int timeout_seconds)
{
    // Child process'ler toplanirken restart yapilmasini engelle ve bekleyen restart isteklerini temizle.
    g_shutdown_requested = 1;

    memset(pending_restart, 0, sizeof(pending_restart));

    if (timeout_seconds < 0)
        timeout_seconds = 0;

    struct timespec start;

    // Sistem saati degisse bile gecen sureyi dogru olcmek icin CLOCK_MONOTONIC kullanilir.
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    {
        logger_log(
            "Shutdown timer initialization failed. "
            "Sending SIGKILL to remaining children."
        );

        process_manager_terminate_all(SIGKILL);
        return;
    }

    logger_log(
        "Waiting up to %d seconds for child processes.",
        timeout_seconds
    );

    while (1)
    {
        //waitpid islemi process manager'a aittir. Sonlanan child process'leri onun uzerinden topla.
        process_manager_handle_sigchld();

        if (!has_active_children())
        {
            logger_log(
                "All child processes exited before shutdown timeout."
            );

            return;
        }

        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        {
            logger_log(
                "Shutdown timer read failed. "
                "Forcing remaining children to stop."
            );

            break;
        }

        double elapsed =
            (double)(now.tv_sec - start.tv_sec) +
            (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;

        if (elapsed >= timeout_seconds)
        {
            logger_log(
                "Shutdown timeout reached. "
                "Sending SIGKILL to remaining children."
            );

            break;
        }

        // Bekleme sirasinda CPU'yu gereksiz mesgul etme.
        struct timespec delay = {0, 100000000L};
        nanosleep(&delay, NULL);
    }

    process_manager_terminate_all(SIGKILL);

    // SIGKILL sonrasi son toplama islemini main.c icindeki graceful_shutdown dongusu tamamlar.
}