#!/bin/bash
#
# DEMIURGE — Mask CPU 3 from all userspace IRQs so it runs audio DSP
# uninterrupted. Installed to: /usr/local/bin/demiurge-irq-isolate.sh
# Called by: demiurge-irq-isolate.service (at boot, before demiurge.service).
#
# Pairs with the kernel cmdline `isolcpus=3` and the per-process pin
# (DEMIURGE_RT_PIN, see config/systemd/demiurge.service.d/cpu-affinity.conf).
# NOTE: `nohz_full=3` / `rcu_nocbs=3` are also on the cmdline but the stock
# Raspberry Pi OS kernel REJECTS both ("Housekeeping: nohz unsupported");
# only isolcpus is live. isolcpus alone is NOT enough either — it keeps the
# scheduler off CPU 3, but interrupt handlers still land there via the
# default IRQ affinity mask (which is 0-3). A single storage/network/audio
# IRQ taking 30-50us on CPU 3 while the engine is mid-DSP at
# quantum = 128 (5.33 ms per block) is a ~1% xrun per event — audible as
# intermittent crackle that looks like bitcrush.
#
# What this does:
#   /proc/irq/default_smp_affinity = 7 (= 0b0111 = CPUs 0,1,2)
#       → every IRQ created after boot avoids CPU 3
#   /proc/irq/<n>/smp_affinity_list = 0-2 for every existing IRQ
#       → retroactively moves early boot IRQs off CPU 3
#   /sys/devices/virtual/workqueue/cpumask = 7
#       → UNBOUND kernel workqueues stop scheduling workers on CPU 3
#
# Some IRQs (per-CPU timers, IPIs, kernel-reserved) refuse the write with
# EIO — that's expected and silently ignored. Anything that CAN be moved
# WILL be moved.
#
# WORKQUEUE SCOPE — read before "fixing" the leftovers. The cpumask above
# only governs UNBOUND workqueues. PER-CPU pools (kworker/3:0, kworker/3:1,
# and the highpri kworker/3:0H / kworker/3:1H-kblockd) are bound to CPU 3 by
# construction and CANNOT be moved by any sysfs knob; they will still appear
# in `ps -eLo psr,comm | awk '$1==3'` after this runs. That is expected, not
# a failure.
#
# MEASURED (2026-08-11, 60 s sample via /proc/<tid>/schedstat, engine live):
#   ksoftirqd/3           36.4 ms/60 s   0.061 % of CPU 3
#   kworker/3:1-events     0.80 ms/60 s  0.0013 %
#   ktimers/3              0.54 ms/60 s  0.0009 %
#   kworker/3:1H, 3:2H, 3:2   0 ns       idle for the whole sample
#   → total non-engine kernel time on CPU 3 ≈ 0.063 % of the core.
# All of them are SCHED_OTHER except ktimers/3 (FIFO 1); the engine runs
# SCHED_FIFO 74, so none of them can PREEMPT it — they only get the core
# while the engine sleeps between blocks. This mask is therefore correct
# hygiene and a guard against future unbound work, NOT a latency fix:
# no engine-tail improvement should be expected or claimed from it.
set -eu

MASK_HEX=7          # cores 0-2
MASK_LIST=0-2

# Default for any IRQ registered AFTER this script runs.
echo $MASK_HEX > /proc/irq/default_smp_affinity

# Existing IRQs: best-effort.
moved=0
skipped=0
for d in /proc/irq/[0-9]*; do
    [ -w "$d/smp_affinity_list" ] || { skipped=$((skipped+1)); continue; }
    if echo $MASK_LIST > "$d/smp_affinity_list" 2>/dev/null; then
        moved=$((moved+1))
    else
        skipped=$((skipped+1))
    fi
done

echo "[demiurge-irq-isolate] CPU 3 masked from IRQs (moved=$moved skipped=$skipped)"

# Unbound kernel workqueues: keep their workers on 0-2. Per-CPU pools are
# unaffected by this knob (see WORKQUEUE SCOPE above). Best-effort — the
# file is absent on kernels built without sysfs workqueue support.
WQ_MASK=/sys/devices/virtual/workqueue/cpumask
if [ -w "$WQ_MASK" ] && echo $MASK_HEX > "$WQ_MASK" 2>/dev/null; then
    echo "[demiurge-irq-isolate] unbound workqueues restricted to CPUs $MASK_LIST"
else
    echo "[demiurge-irq-isolate] unbound workqueue cpumask not writable — skipped"
fi
