#!/usr/bin/env python3
"""A1-37 VIGIL bridge: simulator + kernel telemetry -> Ground Station.

usage: python3 bridge.py /dev/pts/N [--sim]
- default: vitals are typed in by the astronaut on the kernel console; the kernel reports them as "VIT hr 72" lines
- with --sim: also sends SIMULATED vitals to the kernel (h072 / o098 / t366), one set per second
- reads the kernel's own telemetry lines (INJ / EVT / REPLAY / RUN / FREEZE, plus CHK / COG / AINJ / AEVT / DEMO from the astronaut console)
- serves them on http://127.0.0.1:8037/events for ground_station.html
Nothing here is a real sensor. Vitals are either typed in by the astronaut or (with --sim) simulated.
"""
import os, sys, time, json, random, threading, termios, tty
from http.server import BaseHTTPRequestHandler, HTTPServer

SIM = "--sim" in sys.argv
port = [a for a in sys.argv[1:] if not a.startswith("--")][0]
fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
try: tty.setraw(fd)
except Exception: pass

lock = threading.Lock()
events = []                       # {"id","type","text"}
state = {"inj": 0, "fix": 0, "frozen": False, "sim_hr": None, "paused_until": 0.0, "n": 0, "o2": None, "t10": None, "bp": None, "sl": None, "vsrc": ("sim" if SIM else "entered"), "astro": {}, "ainj": 0, "afix": 0, "demo": False}

def add(t, text):
    with lock:
        events.append({"id": len(events) + 1, "type": t, "text": text})

def handle(line):
    print("KERNEL", line, flush=True)
    if line.startswith("INJ"):
        with lock: state["inj"] += 1
        add("INJ", line)
    elif line.startswith("EVT"):
        with lock: state["fix"] += 1
        add("EVT", line)
    elif line.startswith("REPLAY"):
        with lock: state["paused_until"] = time.time() + 8
        add("REPLAY", line)
    elif line.startswith("RUN"):
        add("RUN", line)
    elif line.startswith(("CHK ", "COG ")):          # astronaut console: CHK sugar 108 GREEN [SIM]
        p = line.split()
        if len(p) >= 4:
            with lock: state["astro"][p[1]] = {"v": p[2], "lvl": p[3], "sim": len(p) > 4, "kind": p[0]}
        add("ASTRO", line)
    elif line.startswith("VIT "):                    # vital typed in by the astronaut: VIT hr 72
        p = line.split()
        if len(p) == 2 and p[1] == "reset":           # astronaut cleared all vitals on the console
            with lock:
                for k in ("sim_hr", "o2", "t10", "bp", "sl"): state[k] = None
                state["n"] += 1
            add("VITRESET", line)
            return
        key = {"hr": "sim_hr", "spo2": "o2", "temp": "t10", "bp": "bp", "sleep": "sl"}.get(p[1]) if len(p) == 3 else None
        if key and p[2].isdigit():
            with lock: state[key] = int(p[2]); state["n"] += 1
        add("VIT", line)
    elif line.startswith("AINJ"):
        with lock: state["ainj"] += 1
        add("AINJ", line)
    elif line.startswith("AEVT"):
        with lock: state["afix"] += 1
        add("AEVT", line)
    elif line.startswith("DEMO"):
        with lock: state["demo"] = True
        add("DEMO", line)
    elif line.startswith("FREEZE"):
        with lock: state["frozen"] = True
        add("FREEZE", line)

def reader():
    buf = b""
    while True:
        try:
            d = os.read(fd, 256)
        except OSError:
            time.sleep(0.1); continue
        if not d: time.sleep(0.05); continue
        buf += d
        while b"\n" in buf:
            ln, buf = buf.split(b"\n", 1)
            ln = ln.decode("ascii", "ignore").strip()
            if ln: handle(ln)

def sender():
    r1, r2 = random.Random(37), random.Random(38)
    while True:
        with lock: paused = time.time() < state["paused_until"]; frozen = state["frozen"]
        if SIM and not paused and not frozen:
            hr = 72 + r1.choice([-1, 0, 1])
            o2 = 98 + r2.choice([-1, 0, 0, 1])
            t10 = 366 + r1.choice([-1, 0, 1])
            bp = 120 + r2.choice([-2, -1, 0, 1, 2])   # simulated systolic
            sl = 75                                   # simulated sleep, tenths of an hour (7.5 h)
            with lock:
                state["sim_hr"] = hr; state["o2"] = o2; state["t10"] = t10; state["bp"] = bp; state["sl"] = sl; state["n"] += 1
            try:
                os.write(fd, ("h%03d\no%03d\nt%03d\np%03d\ns%03d\n" % (hr, o2, t10, bp, sl)).encode())
            except OSError:
                pass
        time.sleep(1)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        since = 0
        if "since=" in self.path:
            try: since = int(self.path.split("since=")[1].split("&")[0])
            except ValueError: pass
        with lock:
            body = json.dumps({"events": [e for e in events if e["id"] > since],
                               "last": len(events), "inj": state["inj"], "fix": state["fix"],
                               "frozen": state["frozen"], "sim_hr": state["sim_hr"],
                               "o2": state["o2"], "t10": state["t10"], "bp": state["bp"], "sl": state["sl"], "n": state["n"],
                               "astro": state["astro"], "ainj": state["ainj"], "afix": state["afix"], "demo": state["demo"], "vsrc": state["vsrc"]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers(); self.wfile.write(body)

threading.Thread(target=reader, daemon=True).start()
threading.Thread(target=sender, daemon=True).start()
print("bridge on", port, "-> http://127.0.0.1:8037/events  (SIMULATED vitals)", flush=True)
HTTPServer(("127.0.0.1", 8037), H).serve_forever()
