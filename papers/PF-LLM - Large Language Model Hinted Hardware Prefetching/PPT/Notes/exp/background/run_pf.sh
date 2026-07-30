#!/bin/bash
# run_pf.sh — Intel HW prefetcher ON/OFF 대조 실험
# usage: sudo ./run_pf.sh
#
# MSR 0x1A4 = MSR_MISC_FEATURE_CONTROL  (disable 비트: 1=끔)
#   bit0 L2 streamer / bit1 L2 adjacent line / bit2 DCU(L1 next-line) / bit3 DCU IP(L1 stride)
#   0x0 = 넷 다 켬,  0xF = 넷 다 끔

set -u
CORE=${CORE:-2}
PATTERNS=${PATTERNS:-"seq line page chase"}
EV=cycles,instructions,L1-dcache-loads,L1-dcache-load-misses

[ "$EUID" -ne 0 ] && { echo "sudo로 실행하세요: sudo ./run_pf.sh"; exit 1; }
modprobe msr 2>/dev/null

restore() { wrmsr -a 0x1a4 0x0 2>/dev/null; echo "[복구] prefetcher 전부 ON"; }
trap restore EXIT INT TERM

echo "### 실험 전 MSR 0x1A4 (코어 0-11)"
rdmsr -a 0x1a4 | tr '\n' ' '; echo; echo

for state in 0x0 0xF; do
    if [ "$state" = "0x0" ]; then label="ON"; else label="OFF"; fi
    wrmsr -a 0x1a4 $state
    got=$(rdmsr -p $CORE 0x1a4)
    echo "════════ prefetcher $label   (0x1a4 씀=$state, 읽음=$got) ════════"
    for p in $PATTERNS; do
        out=$(taskset -c $CORE perf stat -e $EV -x, ./pf_bench "$p" 2>/tmp/pf_perf.txt)
        echo "  $out"
        awk -F, '$3=="cycles"||$3=="instructions"||$3~/L1-dcache/ {printf "      %-24s %s\n", $3, $1}' /tmp/pf_perf.txt
    done
    echo
done
