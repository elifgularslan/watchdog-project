#!/usr/bin/env bash
set -u

WATCHDOG="./watchdog"
LOG_FILE="watchdog.log"
PASS=0
FAIL=0
WD_PID=""
KNOWN_PIDS=""
TRACKED_WORKERS=""

pass() {
    echo "[PASS] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "[FAIL] $1"
    FAIL=$((FAIL + 1))
}

is_known_pid() {
    local check_pid="$1"
    [[ " $KNOWN_PIDS " == *" $check_pid "* ]]
}

track_worker_pid() {
    local pid="$1"
    if [ -n "$pid" ] && [[ " $TRACKED_WORKERS " != *" $pid "* ]]; then
        TRACKED_WORKERS="$TRACKED_WORKERS $pid"
    fi
}

verify_worker_pid() {
    local pid="$1"
    [ -n "$pid" ] || return 1
    kill -0 "$pid" 2>/dev/null || return 1

    local ppid cmd
    ppid=$(ps -o ppid= -p "$pid" 2>/dev/null | tr -d ' ')
    cmd=$(ps -o args= -p "$pid" 2>/dev/null || true)

    [ "$ppid" = "$WD_PID" ] && [[ "$cmd" == *worker* ]]
}

find_new_worker_pid() {
    local cur_pid
    for cur_pid in $(pgrep -P "$WD_PID" 2>/dev/null || true); do
        if ! is_known_pid "$cur_pid"; then
            if verify_worker_pid "$cur_pid"; then
                echo "$cur_pid"
                return 0
            fi
        fi
    done
    return 1
}

cleanup() {
    local pid cmd
    if [ -n "${WD_PID:-}" ]; then
        for pid in $(pgrep -P "$WD_PID" 2>/dev/null || true); do
            track_worker_pid "$pid"
        done
        if kill -0 "$WD_PID" 2>/dev/null; then
            kill -SIGKILL "$WD_PID" 2>/dev/null || true
            wait "$WD_PID" 2>/dev/null || true
        fi
    fi

    for pid in $TRACKED_WORKERS; do
        if kill -0 "$pid" 2>/dev/null; then
            cmd=$(ps -o args= -p "$pid" 2>/dev/null || true)
            if [[ "$cmd" == *worker* ]]; then
                kill -SIGKILL "$pid" 2>/dev/null || true
            fi
        fi
    done
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

CHILDREN=($(pgrep -P "$WD_PID" 2>/dev/null || true))
VERIFIED_CHILDREN=()
for cpid in "${CHILDREN[@]}"; do
    if verify_worker_pid "$cpid"; then
        VERIFIED_CHILDREN+=("$cpid")
        KNOWN_PIDS="$KNOWN_PIDS $cpid"
        track_worker_pid "$cpid"
    fi
done

if [ "${#VERIFIED_CHILDREN[@]}" -ge 2 ]; then
    pass "En az 2 dogrulanmis worker baslatildi (PIDs: ${VERIFIED_CHILDREN[*]})."
else
    fail "En az 2 dogrulanmis worker bulunamadi (bulunan: ${#VERIFIED_CHILDREN[@]})."
fi

echo "=== 3. SIGUSR1 Durum Raporlama Testi ==="
kill -SIGUSR1 "$WD_PID"
sleep 1

if [ -f "$LOG_FILE" ] && grep -q "STATUS REPORT (SIGUSR1)" "$LOG_FILE"; then
    pass "SIGUSR1 durum raporu watchdog.log dosyasina yazildi."
else
    fail "SIGUSR1 durum raporu log dosyasinda bulunamadi."
fi

echo "=== 4. Beklenmedik Sonlanma (SIGKILL) ve Kesin Restart Limiti (Max 3) ==="
TARGET_PID="${VERIFIED_CHILDREN[0]:-}"
RESTART_COUNT=0

if [ -n "$TARGET_PID" ]; then
    for attempt in 1 2 3; do
        if ! verify_worker_pid "$TARGET_PID"; then
            fail "Deneme $attempt oncesi hedef worker PID ($TARGET_PID) dogrulanamadi."
            break
        fi

        echo "  -> Deneme $attempt/3: Dogrulanan worker PID $TARGET_PID (PPID=$WD_PID) SIGKILL ile sonlandiriliyor..."
        kill -SIGKILL "$TARGET_PID"

        NEW_PID=""
        for _ in 1 2 3 4 5 6 7 8 9 10; do
            sleep 0.5
            NEW_PID=$(find_new_worker_pid || true)
            if [ -n "$NEW_PID" ]; then
                break
            fi
        done

        if [ -n "$NEW_PID" ]; then
            KNOWN_PIDS="$KNOWN_PIDS $NEW_PID"
            track_worker_pid "$NEW_PID"
            TARGET_PID="$NEW_PID"
            RESTART_COUNT=$((RESTART_COUNT + 1))
            echo "  -> Restart #$RESTART_COUNT algilandi (yeni dogrulanan PID: $NEW_PID)."
        else
            echo "  -> Deneme $attempt sonrasi yeni worker baslatilmadi."
            break
        fi
    done

    if [ "$RESTART_COUNT" -eq 3 ]; then
        pass "Hedef worker beklenmedik sonlanmalarda tam 3 kez yeniden baslatildi."

        if verify_worker_pid "$TARGET_PID"; then
            echo "  -> Limit testi (4. sonlandirma): Dogrulanan worker PID $TARGET_PID (PPID=$WD_PID) SIGKILL ile sonlandiriliyor..."
            kill -SIGKILL "$TARGET_PID"
            sleep 3

            EXTRA_PID=$(find_new_worker_pid || true)
            if [ -z "$EXTRA_PID" ] && ! kill -0 "$TARGET_PID" 2>/dev/null; then
                pass "Maksimum 3 restart limiti dogrulandi: 4. beklenmedik sonlanmadan sonra yeniden baslatma yapilmadi."
            else
                fail "Restart limiti ihlali: 4. beklenmedik sonlanmadan sonra worker tekrar baslatildi (PID: ${EXTRA_PID:-$TARGET_PID})."
            fi
        else
            fail "4. sonlandirma testi oncesi hedef worker PID ($TARGET_PID) dogrulanamadi."
        fi
    else
        fail "Beklenen 3 restart gerceklesmedi (gozlenen: $RESTART_COUNT)."
    fi
else
    fail "SIGKILL testi icin dogrulanmis worker PID alinamadi."
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
echo "Test Ozeti: $PASS PASS, $FAIL FAIL"
echo "Not: Ctrl+C (SIGINT) ve normal cikis senaryolarini ayrica manuel test edin."
echo "========================================"

[ "$FAIL" -eq 0 ]
