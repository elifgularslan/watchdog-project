#ifndef LOGGER_H
#define LOGGER_H

#include "process_manager.h"

/* Loglama fonksiyonları (Vildan'ın Görevleri) */
int logger_init(const char *log_file_path);
void logger_log(const char *format, ...);
void logger_report_status(ProcessInfo *processes, int count);
void logger_close(void);

#endif /* LOGGER_H */
