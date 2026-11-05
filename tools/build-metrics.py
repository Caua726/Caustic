#!/usr/bin/env python3
# tools/build-metrics.py — run one build and report what it cost, as JSON.
#
#   tools/build-metrics.py [--label NAME] [--interval-ms N] -- <command...>
#
# The figure that matters is peak_pss_sum_mb: the sum of PSS over the command and
# every descendant, sampled while it runs. A forking compiler's largest RSS counts
# shared pages once per process, and says nothing of what the workers add together.
# Being sampled, it can miss a peak shorter than the interval. memory_peak_mb comes
# from the cgroup when the command runs in a scope of its own, is exact, and also
# counts page cache.
import json, os, signal, subprocess, sys, time

def children(pid):
    out = []
    try:
        for tid in os.listdir(f"/proc/{pid}/task"):
            with open(f"/proc/{pid}/task/{tid}/children") as f:
                out += [int(x) for x in f.read().split()]
    except OSError:
        pass
    return out

def tree(root):
    seen, todo = [], [root]
    while todo:
        p = todo.pop()
        seen.append(p)
        todo += children(p)
    return seen

def rollup(pid):
    pss = rss = 0
    try:
        with open(f"/proc/{pid}/smaps_rollup") as f:
            for line in f:
                if line.startswith("Pss:"):
                    pss = int(line.split()[1])
                elif line.startswith("Rss:"):
                    rss = int(line.split()[1])
    except OSError:
        pass
    return pss, rss

def cgroup_peak_mb():
    try:
        with open("/proc/self/cgroup") as f:
            rel = f.read().strip().split("::")[-1]
        with open(f"/sys/fs/cgroup{rel}/memory.peak") as f:
            return int(f.read()) // (1024 * 1024)
    except (OSError, ValueError):
        return None

def main():
    args = sys.argv[1:]
    label, interval = "", 20
    while args and args[0] != "--":
        if args[0] == "--label":
            label = args[1]; args = args[2:]
        elif args[0] == "--interval-ms":
            interval = int(args[1]); args = args[2:]
        else:
            sys.exit(f"unknown option {args[0]}")
    if len(args) < 2:
        sys.exit("usage: build-metrics.py [--label NAME] [--interval-ms N] -- <command...>")
    cmd = args[1:]
    start = time.monotonic()
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    peak_pss = peak_rss = peak_procs = samples = 0
    peak_t = 0.0
    status = usage = None
    while True:
        got, st, ru = os.wait4(proc.pid, os.WNOHANG)
        if got == proc.pid:
            status, usage = st, ru
            break
        pss_sum = max_rss = n = 0
        for p in tree(proc.pid):
            pss, rss = rollup(p)
            if pss:
                pss_sum += pss; n += 1
                max_rss = max(max_rss, rss)
        samples += 1
        if pss_sum > peak_pss:
            peak_pss, peak_procs, peak_t = pss_sum, n, time.monotonic() - start
        peak_rss = max(peak_rss, max_rss)
        time.sleep(interval / 1000)
    proc.returncode = os.waitstatus_to_exitcode(status)
    print(json.dumps({
        "label": label,
        "rc": proc.returncode,
        "wall_s": round(time.monotonic() - start, 3),
        "user_s": round(usage.ru_utime, 2),
        "sys_s": round(usage.ru_stime, 2),
        "minor_faults": usage.ru_minflt,
        "peak_pss_sum_mb": peak_pss // 1024,
        "peak_pss_at_s": round(peak_t, 2),
        "peak_pss_procs": peak_procs,
        "max_rss_mb": max(peak_rss // 1024, usage.ru_maxrss // 1024),
        "memory_peak_mb": cgroup_peak_mb(),
        "samples": samples,
    }))
    return 0

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    sys.exit(main())
