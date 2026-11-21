#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include <time.h>

#define MAX_PROCESSES 10
#define MAX_RESTARTS 3

/* Sürecin yaşam döngüsü durumları */
typedef enum {
    PROCESS_STATE_STOPPED = 0,
    PROCESS_STATE_RUNNING = 1,
    PROCESS_STATE_TERMINATED = 2
} ProcessState;

/* Her bir child process için tutulacak ortak veri yapısı */
typedef struct {
    pid_t pid;                  /* Sürecin PID değeri (-1 ise çalışmıyor) */
    char name[32];              /* Sürecin adı (örn: "Worker_1") */
    char executable_path[256];  /* Çalıştırılacak ikili dosya yolu (örn: "./workers/worker") */
    char *args[16];             /* execv için argüman listesi */
    ProcessState state;         /* Çalışıyor, durdu, sonlandı */
    int restart_count;          /* Kaç kez yeniden başlatıldı (Rana'nın limiti: max 3) */
    time_t start_time;          /* Başlatılma zamanı (Loglama ve raporlama için) */
} ProcessInfo;

/*
 * Process Manager Fonksiyon Prototipleri (Ayşe'nin Görevleri)
 */

/* Tabloyu ve yapıları sıfırlar/hazırlar */
int process_manager_init(void);

/* Tanımlı tüm alt süreçleri (en az 2 adet) fork() ve exec() ile başlatır */
int process_manager_start_all(void);

/* Belirli bir indeksteki alt süreci fork() ve exec() ile başlatır (İlk açılış ve restart için) */
int process_manager_spawn(int index);

/* SIGCHLD geldiğinde veya periyodik kontrolde waitpid() ile sonlanan süreçleri toplar */
void process_manager_handle_sigchld(void);

/* Kapanışta (graceful shutdown) tüm çalışan alt süreçlere sinyal gönderir (SIGTERM / SIGKILL) */
void process_manager_terminate_all(int signal_num);

/* Süreç listesini ve eleman sayısını döndürür (Vildan'ın logger'ı ve Elif'in durumu görmesi için) */
ProcessInfo* process_manager_get_list(int *count);

/* PID değerine göre ilgili süreci bulur */
ProcessInfo* process_manager_get_by_pid(pid_t pid);

/* Periyodik kontrol mekanizmasını (setitimer / SIGALRM) yapılandırır */
int process_manager_setup_timer(int interval_seconds);

/* SIGALRM tetiklendiğinde veya periyodik kontrolde süreçlerin sağlık durumunu denetler */
void process_manager_periodic_check(void);

#endif /* PROCESS_MANAGER_H */
