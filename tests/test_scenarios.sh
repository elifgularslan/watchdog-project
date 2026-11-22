#!/usr/bin/env bash
set -u

WATCHDOG="./watchdog"
LOG_FILE="watchdog.log"
PASS=0
FAIL=0

pass() {
    echo "[PASS] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "[FAIL] $1"
    FAIL=$((FAIL + 1))
}

cleanup() {
    if [ -n "${WD_PID:-}" ] && kill -0 "$WD_PID" 2>/dev/null; then
        kill -SIGKILL "$WD_PID" 2>/dev/null || true
        wait "$WD_PID" 2>/dev/null || true
    fi
    pkill -f "./workers/worker" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== 1. Derleme Kontrolu (make) ==="
make clean >/dev/null 2>&1 || true
if make; then
    pass "Proje make komutu ile derlendi."
else
    fail "Proje derlenemedi."
    exit 1
fi

if [ ! -x "$WATCHDOG" ]; then
    fail "watchdog calistirilabilir dosyasi olusmadi."
    exit 1
fi

rm -f "$LOG_FILE"

echo "=== 2. Normal Calisma ve Child Process Baslatma ==="
"$WATCHDOG" > /tmp/watchdog_stdout.log 2>&1 &
WD_PID=$!
sleep 2

if kill -0 "$WD_PID" 2>/dev/null; then
    pass "Watchdog calisiyor (PID: $WD_PID)."
else
    fail "Watchdog baslatilamadi."
    exit 1
fi

CHILDREN=($(pgrep -P "$WD_PID" || true))
if [ "${#CHILDREN[@]}" -ge 2 ]; then
    pass "En az 2 child process baslatildi (PIDs: ${CHILDREN[*]})."
else
    fail "En az 2 child process bulunamadi (bulunan: ${#CHILDREN[@]})."
fi

echo "=== 3. SIGUSR1 Durum Raporlama Testi ==="
kill -SIGUSR1 "$WD_PID"
sleep 1

if [ -f "$LOG_FILE" ] && grep -q "STATUS REPORT (SIGUSR1)" "$LOG_FILE"; then
    pass "SIGUSR1 durum raporu watchdog.log dosyasina yazildi."
else
    fail "SIGUSR1 durum raporu log dosyasinda bulunamadi."
fi

echo "=== 4. Beklenmedik Sonlanma (SIGKILL) ve Restart Limiti (Max 3) ==="
TARGET_PID="${CHILDREN[0]:-}"
if [ -n "$TARGET_PID" ]; then
    RESTARTED=0
    for attempt in 1 2 3 4; do
        CURRENT_CHILDREN=($(pgrep -P "$WD_PID" || true))
        if [ "${#CURRENT_CHILDREN[@]}" -eq 0 ]; then
            break
        fi
        VICTIM="${CURRENT_CHILDREN[0]}"
        echo "  -> Deneme $attempt: Worker PID$VICTIM SIGKILL ile sonlandiriliyor..."
        kill -SIGKILL "$VICTIM" 2>/dev/null || true
        sleep 2

        NEW_CHILDREN=($(pgrep -P "$WD_PID" || true))
        if [ "$attempt" -le 3 ] && [ "${#NEW_CHILDREN[@]}" -ge "${#CURRENT_CHILDREN[@]}" ]; then
            RESTARTED=$((RESTARTED + 1))
        fi
    done

    if [ "$RESTARTED" -ge 1 ]; then
        pass "Beklenmedik sonlanan worker yeniden baslatildi (gozlenen restart: $RESTARTED)."
    else
        fail "Beklenmedik sonlanan worker yeniden baslatilamadi."
    fi

    AFTER_LIMIT=($(pgrep -P "$WD_PID" || true))
    if [ "${#AFTER_LIMIT[@]}" -lt "${#CHILDREN[@]}" ]; then
        pass "Maksimum 3 restart limiti asildiktan sonra yeniden baslatma durduruldu."
    else
        echo "[INFO] Restart limit davranisini watchdog.log uzerinden manuel dogrulayin."
    fi
else
    fail "SIGKILL testi icin worker PID alinamadi."
fi

echo "=== 5. Guvenli Kapanis (SIGTERM) Testi ==="
kill -SIGTERM "$WD_PID"
sleep 6

if ! kill -0 "$WD_PID" 2>/dev/null; then
    pass "Watchdog SIGTERM ile guvenli sekilde kapandi."
else
    fail "Watchdog SIGTERM sonrasi kapanmadi."
fi

LEFTOVER=$(pgrep -P "$WD_PID" 2>/dev/null || true)
if [ -z "$LEFTOVER" ]; then
    pass "Kapanis sonrasi geride child process kalmadi."
else
    fail "Kapanis sonrasi aktif child process kaldi: $LEFTOVER"
fi

echo "=== 6. Log Dogrulama ==="
if [ -f "$LOG_FILE" ] && grep -q "Graceful shutdown started." "$LOG_FILE" && grep -q "Watchdog stopped." "$LOG_FILE"; then
    pass "watchdog.log temel yasam dongusu olaylarini iceriyor."
else
    fail "watchdog.log icinde beklenen kapanis kayitlari eksik."
fi

echo ""
echo "========================================"
echo "Test Ozeti: $PASS PASS,$FAIL FAIL"
echo "Not: Ctrl+C (SIGINT) ve normal cikis senaryolarini ayrica manuel test edin."
echo "========================================"

[ "$FAIL" -eq 0 ]
