#!/bin/bash

# Usage: ./experiment.sh <1|2|3>
# 1 = 1 Worker, No Mutex
# 2 = 3 Workers, No Mutex
# 3 = 3 Workers, Use Mutex

cd "$(dirname "$0")" || exit 1

case "${1:-}" in
  1) EXP=1 ;;
  2) EXP=2 ;;
  3) EXP=3 ;;
  *)
    echo "Usage: $0 <1|2|3>"
    exit 1
    ;;
esac

CLIENTS=${CLIENTS:-5}
SEAT=${SEAT:-10}
OUT=results
SERVER_LOG="$OUT/exp${EXP}_server.log"

# เช็กว่ามี server เก่ารันอยู่หรือไม่
if pgrep -x server >/dev/null 2>&1; then
    echo "A server is already running. Stop it first."
    exit 1
fi

# Compile
echo "Compiling..."
gcc -Wall -Wextra -O2 -pthread src/server.c -o server -lrt || exit 1
gcc -Wall -Wextra -O2 src/client.c -o client -lrt || exit 1

mkdir -p "$OUT"
rm -f "$OUT"/exp${EXP}_*

# เปิด Server
echo "Starting Server EXP $EXP..."
./server "$EXP" > >(tee "$SERVER_LOG") 2>&1 &
SERVER_PID=$!

# ส่งสัญญาณให้ server ปิดตัว
cleanup() {
    kill -INT "$SERVER_PID" 2>/dev/null
}

trap cleanup EXIT

sleep 1

# ตรวจว่า Server ยังทำงานอยู่ไหม
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "Server failed to start:"
    cat "$SERVER_LOG"
    exit 1
fi

echo "Sending $CLIENTS clients to reserve Seat $SEAT..."

# สร้าง Client หลายตัว
pids=()

# ส่งคำสั่งให้ Client
for i in $(seq 1 "$CLIENTS"); do
    printf "RESERVE $SEAT\nQUIT\n" | ./client "$i" \
        > "$OUT/exp${EXP}_client${i}.out" 2>&1 &
    pids+=($!)
done

# รอ client ทุกตัวทำงานเสร็จ
for pid in "${pids[@]}"; do
    wait "$pid"
done

echo "All clients finished."

# ปิด Server
kill -INT "$SERVER_PID" 2>/dev/null
sleep 1

if kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null
fi

wait "$SERVER_PID" 2>/dev/null

echo
echo "=== Experiment $EXP ==="
echo "Workers/Mutex EXP: $EXP"
echo "Clients: $CLIENTS"
echo "Seat: $SEAT"
echo

# สรุปผลจาก client output
success=0

for i in $(seq 1 "$CLIENTS"); do
    if grep -q "reserved successfully" "$OUT/exp${EXP}_client${i}.out"; then
        result="SUCCESS"
        success=$((success + 1))
    elif grep -q "already reserved" "$OUT/exp${EXP}_client${i}.out"; then
        result="FAILED"
    else
        result="NO REPLY"
    fi

    printf "Client %d : %s\n" "$i" "$result"
done

echo
echo "Clients that reserved Seat $SEAT successfully: $success"

# ตรวจว่าเกิด Race Condition หรือไม่
if [ "$success" -gt 1 ]; then
    echo ">>> RACE CONDITION: more than one client reserved the same seat!"
elif [ "$success" -eq 1 ]; then
    echo ">>> Exactly one client reserved the seat."
else
    echo ">>> No client reserved the seat. Check the logs."
fi

echo

echo "Server log: $SERVER_LOG"
