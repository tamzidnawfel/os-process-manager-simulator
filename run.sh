#!/usr/bin/env bash
set -e

echo "================================================================"
echo "   Multithreaded OS Process Manager Simulator (1-Click Run)"
echo "================================================================"
echo

CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--Wall -Wextra -pthread -std=c11 -Iinclude}"
TARGET="pm_sim"

if ! command -v "$CC" &> /dev/null; then
    if command -v clang &> /dev/null; then
        CC="clang"
    else
        echo "[ERROR] Neither gcc nor clang was found on this system."
        echo "Please install build-essential or a C compiler."
        exit 1
    fi
fi

echo "[*] Compiling project with $CC..."
$CC $CFLAGS src/pm_sim.c src/process_manager.c -o "$TARGET"
echo "[OK] Compilation successful: $TARGET generated."
echo

echo "[*] Running simulation..."
if [ -f "snapshots.txt" ]; then
    rm -f "snapshots.txt"
    echo "[*] Cleared previous snapshots.txt."
fi
CHOICE="${1:-1}"
echo "----------------------------------------------------------------"
case "$CHOICE" in
    1)
        echo "[*] Running Scenario 1: Standard Benchmark (tests/thread1.txt tests/thread2.txt)..."
        ./"$TARGET" tests/thread1.txt tests/thread2.txt
        ;;
    2)
        echo "[*] Running Scenario 2: Deep Process Tree (tests/tree_t0.txt tests/tree_t1.txt tests/tree_t2.txt)..."
        ./"$TARGET" tests/tree_t0.txt tests/tree_t1.txt tests/tree_t2.txt
        ;;
    3)
        echo "[*] Running Scenario 3: High Concurrency (tests/stress_t0.txt tests/stress_t1.txt tests/stress_t2.txt)..."
        ./"$TARGET" tests/stress_t0.txt tests/stress_t1.txt tests/stress_t2.txt
        ;;
    4)
        echo "[*] Running All Scenarios Sequentially..."
        ./"$TARGET" tests/thread1.txt tests/thread2.txt
        ./"$TARGET" tests/tree_t0.txt tests/tree_t1.txt tests/tree_t2.txt
        ./"$TARGET" tests/stress_t0.txt tests/stress_t1.txt tests/stress_t2.txt
        ;;
    *)
        echo "[*] Running with custom arguments: $@..."
        ./"$TARGET" "$@"
        ;;
esac
echo "----------------------------------------------------------------"
echo

if [ -f "snapshots.txt" ]; then
    echo "[*] Fresh process table snapshots captured in snapshots.txt:"
    echo "    (Showing first 18 lines)"
    echo "----------------------------------------------------------------"
    head -n 18 snapshots.txt
    echo "... [See snapshots.txt for complete chronological history]"
    echo "----------------------------------------------------------------"
fi

echo
echo "[DONE] Simulation finished successfully."
