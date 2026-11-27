# Watchdog — POSIX Signal-Based Process Monitoring System

[Türkçe](#türkçe) | [English](#english)

---

## Türkçe

Sakarya Üniversitesi Yazılım Mühendisliği Bölümü **SWE301 Sistem Programlama 1. Dönem Projesi (2026-2027)** kapsamında C dilinde ve POSIX sistem çağrılarıyla geliştirilmiş süreç izleme ve yönetim uygulaması.

### Özellikler

- `fork()` ve `execv()` kullanarak en az iki alt süreç (worker) başlatma
- `SIGCHLD` ve `waitpid()` ile alt süreçlerin sonlanmasını takip etme
- Normal çıkış ile hata/sinyal kaynaklı beklenmedik sonlanmayı ayırt etme
- Beklenmedik şekilde sonlanan süreçleri **en fazla 3 kez** otomatik yeniden başlatma
- `SIGALRM` / `setitimer()` ile 1 saniyelik periyodik durum kontrolü
- `SIGUSR1` sinyali alındığında tüm alt süreçlerin PID, durum ve restart bilgilerini `watchdog.log` dosyasına raporlama
- `SIGINT` (`Ctrl+C`) veya `SIGTERM` ile 5 saniyelik timeout süresine sahip güvenli kapanış (graceful shutdown)
- Tüm kritik olayları zaman damgasıyla `watchdog.log` dosyasına kaydetme

### Proje Yapısı

```text
watchdog-project/
├── Makefile
├── README.md
├── include/
│   ├── logger.h
│   ├── process_manager.h
│   ├── restart_manager.h
│   └── signal_manager.h
├── src/
│   ├── main.c
│   ├── signal_manager.c
│   ├── process_manager.c
│   ├── restart_manager.c
│   └── logger.c
├── workers/
│   └── worker.c
├── tests/
│   └── test_scenarios.sh
└── docs/
    └── REPORT.pdf
```

### Derleme ve Çalıştırma (Linux / WSL)

Proje kök dizininde:

```bash
make
./watchdog
```

Derleme çıktılarını ve log dosyasını temizlemek için:

```bash
make clean
```

### Test Komutları

#### Otomatik Test Senaryoları

```bash
./tests/test_scenarios.sh
```

#### Manuel Sinyal Testleri

Watchdog başlatıldığında ekrana kendi PID değerini yazdırır (`Watchdog PID: <PID>`). Başka bir terminalden aşağıdaki testleri uygulayabilirsiniz:

```bash
# Durum raporu alma (watchdog.log dosyasina yazar)
kill -SIGUSR1 <WATCHDOG_PID>

# Bir worker surecini beklenmedik sekilde sonlandirma (restart testi)
pgrep -P <WATCHDOG_PID>
kill -SIGKILL <WORKER_PID>

# Guvenli kapanis (SIGTERM)
kill -SIGTERM <WATCHDOG_PID>
```

`SIGINT` güvenli kapanış davranışı, watchdog'un çalıştığı terminalde doğrudan `Ctrl+C` tuşlarına basılarak ayrıca test edilmelidir.

#### Log Dosyasını İnceleme

```bash
cat watchdog.log
```

### Modül Sorumlulukları

- **Sinyal Yönetimi ve Ana Program:** `src/signal_manager.c`, `src/main.c`
- **Alt Süreç Yönetimi:** `src/process_manager.c`, `workers/worker.c`
- **Otomatik Restart ve Timeout Yönetimi:** `src/restart_manager.c`
- **Loglama, Durum Raporlama, Derleme ve Test Altyapısı:** `src/logger.c`, `Makefile`, `tests/test_scenarios.sh`, `README.md`

---

## English

A robust process monitoring and management application developed in C using POSIX system calls for Sakarya University Department of Software Engineering, **SWE301 System Programming Term Project 1 (Fall 2026-2027)**.

### Features

- Spawns at least two child worker processes via `fork()` and `execv()`
- Tracks child process termination using `SIGCHLD` and non-blocking `waitpid()`
- Distinguishes between clean exits (`exit(0)`) and crashes/fatal signal terminations
- Automatically restarts unexpectedly terminated processes up to **a maximum of 3 times**
- Implements 1-second periodic child health checks using `SIGALRM` / `setitimer()`
- Generates a full status report (PID, state, restart counts) in `watchdog.log` upon receiving `SIGUSR1`
- Provides graceful shutdown with a 5-second timeout on `SIGINT` (`Ctrl+C`) or `SIGTERM`
- Logs all critical lifecycle events with precise ISO-style timestamps in `watchdog.log`

### Project Structure

```text
watchdog-project/
├── Makefile
├── README.md
├── include/
│   ├── logger.h
│   ├── process_manager.h
│   ├── restart_manager.h
│   └── signal_manager.h
├── src/
│   ├── main.c
│   ├── signal_manager.c
│   ├── process_manager.c
│   ├── restart_manager.c
│   └── logger.c
├── workers/
│   └── worker.c
├── tests/
│   └── test_scenarios.sh
└── docs/
    └── REPORT.pdf
```

### Building and Running (Linux / WSL)

From the project root directory:

```bash
make
./watchdog
```

To clean compiled binaries, object files, and logs:

```bash
make clean
```

### Testing

#### Automated Test Suite

```bash
./tests/test_scenarios.sh
```

#### Manual Signal Testing

Upon launch, the watchdog prints its own PID (`Watchdog PID: <PID>`). Run the following commands from another terminal:

```bash
# Request a status report (logged to watchdog.log)
kill -SIGUSR1 <WATCHDOG_PID>

# Terminate a worker unexpectedly to verify restart logic
pgrep -P <WATCHDOG_PID>
kill -SIGKILL <WORKER_PID>

# Initiate graceful shutdown via SIGTERM
kill -SIGTERM <WATCHDOG_PID>
```

Graceful shutdown on `SIGINT` can be verified by pressing `Ctrl+C` directly in the terminal where the watchdog is running.

#### Inspecting Logs

```bash
cat watchdog.log
```

### Module Responsibilities

- **Signal Handling & Main Loop:** `src/signal_manager.c`, `src/main.c`
- **Child Process Management:** `src/process_manager.c`, `workers/worker.c`
- **Restart Policy & Timeout Management:** `src/restart_manager.c`
- **Logging, Reporting, Build & Test Infrastructure:** `src/logger.c`, `Makefile`, `tests/test_scenarios.sh`, `README.md`
