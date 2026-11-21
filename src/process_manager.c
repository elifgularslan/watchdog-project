#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#include "../include/process_manager.h"
#include "../include/logger.h"
#include "../include/restart_manager.h"

/* Statik süreç tablosu */
static ProcessInfo g_processes[MAX_PROCESSES];
static int g_process_count = 0;

/* Loglama yardımcısı: Logger modülü henüz hazır değilse stdout'a yazar */
static void internal_log(const char *msg) {
    printf("[PROCESS_MANAGER] %s\n", msg);
    fflush(stdout);
}

int process_manager_init(void) {
    g_process_count = 0;
    memset(g_processes, 0, sizeof(g_processes));

    /* PDF gereksinimi: En az 2 adet child process tanımlanır */
    
    /* 1. Süreç Tanımı */
    g_processes[0].pid = -1;
    strncpy(g_processes[0].name, "Worker_1", sizeof(g_processes[0].name) - 1);
    strncpy(g_processes[0].executable_path, "./workers/worker", sizeof(g_processes[0].executable_path) - 1);
    g_processes[0].args[0] = "./workers/worker";
    g_processes[0].args[1] = "Worker_1";
    g_processes[0].args[2] = NULL;
    g_processes[0].state = PROCESS_STATE_STOPPED;
    g_processes[0].restart_count = 0;

    /* 2. Süreç Tanımı */
    g_processes[1].pid = -1;
    strncpy(g_processes[1].name, "Worker_2", sizeof(g_processes[1].name) - 1);
    strncpy(g_processes[1].executable_path, "./workers/worker", sizeof(g_processes[1].executable_path) - 1);
    g_processes[1].args[0] = "./workers/worker";
    g_processes[1].args[1] = "Worker_2";
    g_processes[1].args[2] = NULL;
    g_processes[1].state = PROCESS_STATE_STOPPED;
    g_processes[1].restart_count = 0;

    g_process_count = 2;
    return 0;
}

int process_manager_spawn(int index) {
    if (index < 0 || index >= g_process_count) {
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        /* Fork hatası */
        perror("[PROCESS_MANAGER] fork() basarisiz");
        return -1;
    }

    if (pid == 0) {
        /* --- CHILD PROCESS --- */
        /* execv ile yeni program calistirilir */
        execv(g_processes[index].executable_path, g_processes[index].args);

        /* execv basarisiz olursa bu satira duser */
        perror("[PROCESS_MANAGER] execv() basarisiz");
        exit(EXIT_FAILURE);
    }

    /* --- PARENT PROCESS (Watchdog) --- */
    g_processes[index].pid = pid;
    g_processes[index].state = PROCESS_STATE_RUNNING;
    g_processes[index].start_time = time(NULL);

    char log_buf[128];
    snprintf(log_buf, sizeof(log_buf), "Surec baslatildi: %s (PID: %d)", g_processes[index].name, pid);
    internal_log(log_buf);

    return pid;
}

int process_manager_start_all(void) {
    int started = 0;
    for (int i = 0; i < g_process_count; i++) {
        if (process_manager_spawn(i) > 0) {
            started++;
        }
    }
    return started;
}

void process_manager_handle_sigchld(void) {
    int status;
    pid_t pid;

    /*
     * Birden fazla cocuk ayni anda kapanmis olabilir.
     * Bu yuzden waitpid() WNOHANG ile dongu icinde cagrilmalidir.
     */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int found_index = -1;
        for (int i = 0; i < g_process_count; i++) {
            if (g_processes[i].pid == pid) {
                found_index = i;
                break;
            }
        }

        if (found_index == -1) {
            continue; /* Bilinmeyen PID */
        }

        g_processes[found_index].pid = -1;
        g_processes[found_index].state = PROCESS_STATE_TERMINATED;

        /* Normal mi kapandi, sinyalle mi? */
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            char log_buf[128];
            snprintf(log_buf, sizeof(log_buf), "%s (PID: %d) normal kapandi. Cikis kodu: %d",
                     g_processes[found_index].name, pid, exit_code);
            internal_log(log_buf);

            /* Eger cikis kodu hataliysa (0 disinda) restart mekanizmasina bildir */
            if (exit_code != 0) {
                restart_manager_handle_exit(found_index, exit_code, 0);
            }
        } else if (WIFSIGNALED(status)) {
            int term_sig = WTERMSIG(status);
            char log_buf[128];
            snprintf(log_buf, sizeof(log_buf), "%s (PID: %d) sinyal ile kapandi. Sinyal no: %d",
                     g_processes[found_index].name, pid, term_sig);
            internal_log(log_buf);

            /* Beklenmedik kapanma durumunda restart mekanizmasi devreye girer */
            restart_manager_handle_exit(found_index, term_sig, 1);
        }
    }
}

void process_manager_terminate_all(int signal_num) {
    for (int i = 0; i < g_process_count; i++) {
        if (g_processes[i].state == PROCESS_STATE_RUNNING && g_processes[i].pid > 0) {
            char log_buf[128];
            snprintf(log_buf, sizeof(log_buf), "%s (PID: %d) surecine %d sinyali gonderiliyor",
                     g_processes[i].name, g_processes[i].pid, signal_num);
            internal_log(log_buf);

            kill(g_processes[i].pid, signal_num);
        }
    }
}

ProcessInfo* process_manager_get_list(int *count) {
    if (count != NULL) {
        *count = g_process_count;
    }
    return g_processes;
}

ProcessInfo* process_manager_get_by_pid(pid_t pid) {
    for (int i = 0; i < g_process_count; i++) {
        if (g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}

int process_manager_setup_timer(int interval_seconds) {
    if (interval_seconds <= 0) {
        return -1;
    }

    struct itimerval timer;
    /* Periyodik tekrar suresi */
    timer.it_interval.tv_sec = interval_seconds;
    timer.it_interval.tv_usec = 0;
    /* Ilk tetiklenme suresi */
    timer.it_value.tv_sec = interval_seconds;
    timer.it_value.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("[PROCESS_MANAGER] setitimer() basarisiz");
        return -1;
    }

    char log_buf[128];
    snprintf(log_buf, sizeof(log_buf), "Periyodik kontrol zamanlayicisi (%d sn) baslatildi", interval_seconds);
    internal_log(log_buf);
    return 0;
}

void process_manager_periodic_check(void) {
    /* Once sonlanan/zombi surecleri topla */
    process_manager_handle_sigchld();

    /* Calisan sureclerin saglik kontrolunu gerceklestir */
    for (int i = 0; i < g_process_count; i++) {
        if (g_processes[i].state == PROCESS_STATE_RUNNING && g_processes[i].pid > 0) {
            /* kill(pid, 0) POSIX uyarinca surecin varligini denetler */
            if (kill(g_processes[i].pid, 0) == -1) {
                if (errno == ESRCH) {
                    /* Surec artik mevcut degil */
                    char log_buf[128];
                    snprintf(log_buf, sizeof(log_buf), "Periyodik kontrol uyarisi: %s (PID: %d) bulunamadi!",
                             g_processes[i].name, g_processes[i].pid);
                    internal_log(log_buf);
                    g_processes[i].state = PROCESS_STATE_TERMINATED;
                    g_processes[i].pid = -1;
                }
            } else {
                /* Surec saglikli sekilde yasamaya devam ediyor */
                char log_buf[128];
                snprintf(log_buf, sizeof(log_buf), "Periyodik kontrol: %s (PID: %d) calisiyor",
                         g_processes[i].name, g_processes[i].pid);
                internal_log(log_buf);
            }
        }
    }
}

