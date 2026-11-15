#!/usr/bin/env python3
# SIMULATED vitals feeder for A1-37 VIGIL.
# Usage: python3 sim_vitals.py /dev/pts/N
# Sends SIMULATED heart-rate and skin-temperature lines to the kernel,
# prints the kernel's telemetry, and pauses when the kernel starts its replay.
# AI-assisted code (Claude). Values are SIMULATED, not real sensor data.
import os, sys, time, random, select

port = sys.argv[1]
fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
rng = random.Random(37)
buf = b""
paused = False
next_send = time.monotonic()
print("SIMULATED vitals -> " + port + "  (Ctrl+C to stop)", flush=True)
try:
    while True:
        ready, _, _ = select.select([fd], [], [], 0.01)
        if ready:
            buf += os.read(fd, 256)
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("ascii", "replace").strip()
                if not text:
                    continue
                print("KERNEL " + text, flush=True)
                if text.startswith("REPLAY"):
                    paused = True
                    print("SIM paused: kernel is running its SIMULATED replay", flush=True)
        now = time.monotonic()
        if not paused and now >= next_send:
            next_send = now + 1.0
            hr = 72 + rng.randint(-1, 1)
            t10 = 366 + rng.randint(-1, 1)
            os.write(fd, ("h%03d\r\nt%03d\r\n" % (hr, t10)).encode())
            print("SIM h%03d t%03d" % (hr, t10), flush=True)
except KeyboardInterrupt:
    print("SIM stopped")
