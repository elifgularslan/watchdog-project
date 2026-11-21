#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include <time.h>

#define MAX_PROCESSES 10
#define MAX_RESTARTS 3

/* Alt sureclerin anlik durumlarini belirten enum */
typedef enum {
    PROCESS_STATE_STOPPED = 0,
    PROCESS_STATE_RUNNING = 1,
    PROCESS_STATE_TERMINATED = 2
} ProcessState;

/* Her bir alt surecin bilgilerini tutan yapi */
typedef struct {
    pid_t pid;                  /* Calisan surecin PID degeri (-1 ise aktif degil) */
    char name[32];              /* Surece verilen tanimlayici isim */
    char executable_path[256];  /* Calistirilacak dosyanin yolu */
    char *args[16];             /* execv cagrisina gonderilecek parametreler */
    ProcessState state;         /* Surecin mevcut durumu */
    int restart_count;          /* Yeniden baslatilma adedi */
    time_t start_time;          /* Baslatilma zaman damgasi */
} ProcessInfo;

/* Surec tablosunu ilk durumuna getirir */
int process_manager_init(void);

/* Listede tanimli olan tum alt surecleri baslatir */
int process_manager_start_all(void);

/* Belirtilen indisteki alt sureci fork ve execv ile calistirir */
int process_manager_spawn(int index);

/* Kapanan veya sinyal alan alt surecleri waitpid ile yakalar */
void process_manager_handle_sigchld(void);

/* Tum aktif cocuk sureclere belirtilen sinyali gonderir */
void process_manager_terminate_all(int signal_num);

/* Surec tablosunu ve eleman sayisini dondurur */
ProcessInfo* process_manager_get_list(int *count);

/* PID degerine gore ilgili surecin adresini bulur */
ProcessInfo* process_manager_get_by_pid(pid_t pid);

/* Belirli araliklarla SIGALRM gonderecek zamanlayiciyi kurar */
int process_manager_setup_timer(int interval_seconds);

/* Sureclerin gercekten calisip calismadigini kontrol eder */
void process_manager_periodic_check(void);

#endif /* PROCESS_MANAGER_H */
