# Watchdog — POSIX Signal-Based Process Monitoring System

Sakarya Üniversitesi Yazılım Mühendisliği Bölümü **Sistem Programlama 1. Dönem Projesi (2026-2027)** kapsamında C dilinde ve POSIX sistem çağrılarıyla geliştirilmiş süreç izleme ve yönetim uygulaması.

## Özellikler

- `fork()` ve `execv()` kullanarak en az iki alt süreç (worker) başlatma
- `SIGCHLD` ve `waitpid()` ile alt süreçlerin sonlanmasını takip etme
- Normal çıkış ile hata/sinyal kaynaklı beklenmedik sonlanmayı ayırt etme
- Beklenmedik şekilde sonlanan süreçleri **en fazla 3 kez** otomatik yeniden başlatma
- `SIGALRM` / `setitimer()` ile 1 saniyelik periyodik durum kontrolü
- `SIGUSR1` sinyali alındığında tüm alt süreçlerin PID, durum ve restart bilgilerini `watchdog.log` dosyasına raporlama
- `SIGINT` (`Ctrl+C`) veya `SIGTERM` ile 5 saniyelik timeout süresine sahip güvenli kapanış (graceful shutdown)
- Tüm kritik olayları zaman damgasıyla `watchdog.log` dosyasına kaydetme

## Proje Yapısı

~~~text
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
└── tests/
    └── test_scenarios.sh
~~~

## Derleme ve Çalıştırma (Linux / WSL)

Proje kök dizininde:

~~~bash
make
./watchdog
~~~

Derleme çıktılarını ve log dosyasını temizlemek için:

~~~bash
make clean
~~~

## Test Komutları

### Otomatik Test Senaryoları

~~~bash
./tests/test_scenarios.sh
~~~

### Manuel Sinyal Testleri

Watchdog başlatıldığında ekrana kendi PID değerini yazdırır (`Watchdog PID: <PID>`). Başka bir terminalden aşağıdaki testleri uygulayabilirsiniz:

~~~bash
# Durum raporu alma (watchdog.log dosyasina yazar)
kill -SIGUSR1 <WATCHDOG_PID>

# Bir worker surecini beklenmedik sekilde sonlandirma (restart testi)
pgrep -P <WATCHDOG_PID>
kill -SIGKILL <WORKER_PID>

# Guvenli kapanis (SIGTERM)
kill -SIGTERM <WATCHDOG_PID>
~~~

`SIGINT` güvenli kapanış davranışı, watchdog'un çalıştığı terminalde doğrudan `Ctrl+C` tuşlarına basılarak ayrıca test edilmelidir.

### Log Dosyasını İnceleme

~~~bash
cat watchdog.log
~~~

## Modül Sorumlulukları

- **Sinyal Yönetimi ve Ana Program:** `src/signal_manager.c`, `src/main.c`
- **Alt Süreç Yönetimi:** `src/process_manager.c`, `workers/worker.c`
- **Otomatik Restart ve Timeout Yönetimi:** `src/restart_manager.c`
- **Loglama, Durum Raporlama, Derleme ve Test Altyapısı:** `src/logger.c`, `Makefile`, `tests/test_scenarios.sh`, `README.md`
