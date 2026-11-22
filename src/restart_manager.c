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