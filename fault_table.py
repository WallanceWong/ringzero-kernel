# fault_table.py -- builds the fault-injection table from the kernel's serial log
import re, sys

inj, evt = [], []
for line in open(sys.argv[1]):
    line = line.strip()
    m = re.match(r"INJ,tick=(\d+)", line)
    if m:
        inj.append(int(m.group(1)))
        continue
    m = re.match(r"EVT,tick=(\d+),copy=(\w),bad=(\d+),good=(\d+),repaired=1", line)
    if m:
        evt.append((int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(4))))

print(" #  copy  bad  good  injected_tick  fixed_tick  repair_ms")
ms = []
for i, (t_fix, copy, bad, good) in enumerate(evt):
    t_inj = inj[i] if i < len(inj) else None
    r = (t_fix - t_inj) * 10 if t_inj is not None else None   # 1 tick = 10 ms
    if r is not None:
        ms.append(r)
    print(f"{i+1:2d}    {copy}   {bad:3d}   {good:3d}   {t_inj}   {t_fix}   {r}")

print()
print("injected:", len(inj), " corrected:", len(evt))
if ms:
    print("repair ms: min", min(ms), " max", max(ms), " mean", sum(ms) // len(ms))