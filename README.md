# A1-37 VIGIL

**A fault-tolerant crew health monitor that runs on its own bare-metal kernel.**
Team CHS Ring Zero, Catholic High School Sibu. Sarawak Space Challenge 2026 (NASA Space Apps), Junior category.

**Challenge:** *Create Health Monitoring Software for Astronauts on Space Missions* (NASA Space Apps 2026).

> **Read this first.** There is no sensor hardware in this project. Vital signs are **typed in by the astronaut** (console key 6, badge "VITALS: ENTERED BY THE ASTRONAUT"). The optional simulator (`bridge.py --sim`) makes the badge change to "VITALS: SIMULATED", and the 8-week replay and the demo history are always labelled SIMULATED. What is real is the kernel, the fault-tolerance mechanism, and the measured repair times.

**Demo video (30 s):** https://youtu.be/_Nj1hWAB3Fg

The video is a real screen capture of the kernel in QEMU and of the Ground Station. It was cut and captioned with AI assistance, and its sound effects are generated tones.

---

## Why we built it

NASA's Human Research Program groups the risks of human spaceflight into five hazards: space radiation, isolation and confinement, distance from Earth, gravity fields, and hostile or closed environments. Two matter for us:

- **Distance from Earth.** The further a crew travels, the harder it is to get help, supplies and instructions from Earth. A crew health monitor has to keep working without a ground team looking over its shoulder.
- **Space radiation.** NASA lists it among the most hazardous aspects of spaceflight. Our own engineering reasoning (not a NASA claim) is that radiation can flip bits in computer memory, so a health monitor that stores a baseline in memory could quietly learn a wrong "normal".

VIGIL learns a crew member's resting baseline, protects it against memory corruption, and warns early when a sustained drift appears.

## How VIGIL answers the challenge

The challenge asks for software that gathers health indicators and lets astronauts evaluate and act on their own health.

| The challenge asks for | What VIGIL does |
|---|---|
| Gather health indicators | Reads five vitals over a simple serial protocol: heart rate, SpO2, body temperature, systolic blood pressure and sleep hours. Four of them are on the challenge's own list. The astronaut types them in on the console (nothing is read from a sensor); an optional simulator can feed them instead and is then labelled SIMULATED. |
| Astronauts must spot changes in themselves | Learns each crew member's own baseline and looks for a sustained drift away from it, not a one-off reading. |
| Colour coding at a glance | One large GREEN / AMBER / RED status. Each vital is also coloured by its own level. |
| Guidance on what to do | A plain "next step" line for each level, for example "Rest, hydrate, re-measure within 24 hours". |
| Charts that show change over time | The Ground Station plots every vital for the last 60 s and since it connected. |
| When should the ground pay attention | The Ground Station shows the link state, the fault counters and the alert. |
| Hazard: space radiation | Each baseline is stored three times and repaired by majority vote, so one corrupted copy cannot change the result. |
| Hazard: distance from Earth | The monitor is a stand-alone kernel. It reaches its alert without any ground link. |

**Now covered by the astronaut console (see below):** blood sugar and mood typed in by the astronaut, three keyboard-measured tests, and a Health guide that explains each indicator and the five HRP hazards. **Not covered yet:** real sensors, real clocks (no day counts), and clinical reference ranges. See Limits.

## NASA and agency resources used

- NASA Human Research Program, *5 Hazards of Human Spaceflight*: https://www.nasa.gov/hrp/hazards
- NASA Human Research Program, *Risks of Human Spaceflight*: https://www.nasa.gov/hrp/risks
- Canadian Space Agency, *Monitoring radiation for astronaut health* (26 Feb 2026): https://www.asc-csa.gc.ca/eng/news/articles/2026/2026-02-26-monitoring-radiation-astronaut-health.asp

We used these for the problem framing only. No NASA data, logos or images are used in the project.

## What it does

| Part | What it does |
|---|---|
| Baseline | Learns the mean of the first 10 samples of each of five vitals (heart rate, SpO2, temperature, systolic blood pressure, sleep), then locks it. |
| Triple copy (software TMR) | Stores three copies of each baseline and takes a bitwise majority vote. |
| Scrubber | Once per second, checks the three copies and repairs a disagreeing one. |
| Drift detector | Compares each vital with its baseline (heart rate uses the median of the last 7 readings). Levels: GREEN, AMBER, RED. The worst vital sets the screen level. |
| Next step | AMBER: "Rest, hydrate, re-measure within 24 hours". RED: "Contact the flight surgeon". GREEN: "Continue routine monitoring". |
| Heartbeat | A square on screen toggles every 0.25 s. If the kernel freezes, it stops. |
| Counters | The screen shows INJECTED and FIXED totals, counted by the kernel. |
| Serial telemetry | The kernel logs `INJ`, `EVT`, `REPLAY`, `RUN`, `FREEZE` lines on COM1. |

The screen is 800×600 and has panels for the protected baseline, status, fault monitor, 8-week replay and next step.

**Thresholds are demo values, not clinical values.** All sustained for 3 readings. Heart rate: +10 % AMBER, +25 % RED. SpO2: -3 % AMBER, -6 % RED. Temperature: +0.8 C AMBER, +1.5 C RED. Systolic blood pressure: +10 % AMBER, +20 % RED. Sleep: -15 % AMBER, -30 % RED.

## What is real and what is simulated

| Real | Simulated |
|---|---|
| The bare-metal i686 kernel (boots from GRUB, own drivers) | The optional vitals simulator (`bridge.py --sim`) and the demo history |
| The triple-copy vote and scrubber | The 8-week replay data (`gen_replay.py`, seed 37) |
| The fault injection (a bit flipped in one copy) | The "radiation" upset. It is a software bit flip, not a radiation test. |
| The measured repair times | |

The kernel analyses five vitals with the same protected-baseline design: heart rate, SpO2, temperature, systolic blood pressure and sleep. The values are typed in by the astronaut (console key 6) or, with `--sim`, simulated and nearly steady. Only the heart-rate baseline receives the injected faults, and only heart rate is used in the 8-week replay. Blood sugar and mood are **entered by the astronaut** in the astronaut console (see below); they are not read from a sensor.

## Evidence

**Fault injection (real run, `fault.log`, summarised by `fault_table.py`):**
- `fault.log` is one real 20-fault run (20 injected, **20 corrected**; raw serial log `fault_run_raw.log`). Its repair times are **480 to 980 ms, mean 730 ms**.
- **Repair time is not one number.** The scrubber runs once per second, so a repair waits for the next scrub, and the time depends on where the injection lands in that 1 s cycle. We ran the 20-fault test five times, starting the run at different moments after boot. In every run all 20 faults were corrected, and the repair time ranged from about **50 to 980 ms** across the runs:

| Run (key `x` sent after boot) | Corrected | Repair min / max / mean |
|---|---|---|
| 2.0 s | 20 of 20 | 50 / 550 / 300 ms |
| 2.4 s | 20 of 20 | 160 / 660 / 410 ms |
| 2.8 s | 20 of 20 | 290 / 790 / 540 ms |
| 3.2 s | 20 of 20 | 380 / 880 / 630 ms |
| `fault.log` run | 20 of 20 | 480 / 980 / 730 ms |

- Raw serial logs for the four timed runs are in `fault_sweep_raw.log`, so every number can be re-checked with `fault_table.py`. A first 2.0 s attempt was discarded as incomplete (the log was copied too early and held only 4 of 20 faults); it is kept at the end of that file, labelled. The 2.0 s row is from the complete re-run.
- Within one run the faults alternate between two repair times about 500 ms apart, because they are injected 1.5 s apart against a 1 s scrub. We do not quote any single mean as "the" repair time.
- Faults were injected at a fixed 1.5 s spacing, so this run does not sample every possible timing against the 1 s scrub cycle.

**8-week replay (simulated, `gen_replay.py`):**
- 56 daily resting heart-rate values, baseline 63.
- Rule: 7-day rolling median at least 10 % above baseline for 3 consecutive days. The same rule runs in the kernel, the Ground Station dashboard and the script.
- Result: **AMBER on day 47**. The kernel's final median is 77, which is +22 % (AMBER, below the RED threshold of 25 %).

## Run it

Needs Nix (or install grub, xorriso, mtools, qemu yourself).

```
nix-shell -p grub2 xorriso mtools qemu --run "make run-iso"
```

This builds `os.iso` and opens it in QEMU. The kernel needs no input to run, but the baseline only locks after vitals arrive.

**Optional: feed SIMULATED vitals and run the Ground Station link (second terminal setup).** Without `--sim` the bridge only relays the kernel's telemetry, and the vitals are typed in on the kernel console:

```
nix-shell -p qemu --run 'qemu-system-i386 -cdrom os.iso -serial pty > /tmp/q.out 2>&1 & Q=$!; sleep 4; P=$(grep -o "/dev/pts/[0-9]*" /tmp/q.out | head -1); echo "serial port: $P"; python3 bridge.py "$P" --sim; kill $Q'
```

This starts QEMU, finds the serial port, and runs `bridge.py`, which (with `--sim`) sends one set of simulated readings per second, and always serves the kernel's own telemetry to the Ground Station on 127.0.0.1:8037.

**Keys (click the QEMU window first):**

| Key | Action |
|---|---|
| Tab | Inject one fault (bit flip in one of the three copies, rotating A, B, C) |
| X | Inject 20 automatic faults, 150 ticks apart (1 tick = 10 ms) |
| R | Play the 8-week SIMULATED replay (about 4 s), ends in AMBER |
| G | Debug: instantly lock the baseline at 72 |
| F | Freeze the kernel (the heartbeat square stops) |

## Astronaut console (keyboard only, on the kernel screen)

The challenge is about astronauts checking their own health, so the astronaut-facing features live in the kernel. The Ground Station only receives results and alerts. Press **M** to open the console, **Esc** to go back or close.

| Key | Action |
|---|---|
| M / Esc | Open / back / close the console |
| 1 | Health guide (8 pages: heart rate, SpO2, blood pressure, temperature, sleep, blood sugar, mood, cognitive check). Left/Right or N/P change page. Each page: what it is, why we check it, how the NASA HRP hazards affect it, when to alert crew or flight surgeon, safe general action. |
| 2 | Check-in: type blood sugar in mg/dL (Backspace, Enter), then press 1-5 for mood (1 low, 5 good) |
| 3 | Reaction test: 5 trials, Space to start, Space when the screen turns green, result = median ms. Pressing early repeats the trial. |
| 4 | Memory test: repeat a growing 4-colour sequence with keys 1-4, result = longest correct sequence |
| 5 | Mental math: 5 questions (+, -, x), typed answers, result = number correct |
| 6 | Enter vitals (press **C** on that screen to clear all vitals and start over): heart rate, SpO2, systolic blood pressure, temperature (type 366 for 36.6), sleep hours (type 75 for 7.5). Enter on an empty line skips a vital. Each vital's baseline locks after 10 entries (demo value), then it is compared with your own baseline. Telemetry: `VIT hr 72`. |
| D (in the menu) | Load a deterministic DEMO HISTORY (SIMULATED) so drift to AMBER/RED can be shown quickly |
| I | Inject a fault into the astronaut channels (Tab still hits heart rate only) |

**Where each number comes from (labelled on screen):** the 5 vitals, blood sugar and mood = ENTERED BY THE ASTRONAUT (the vitals read SIMULATED only while the `--sim` feed is running; the badge shows which). Reaction, memory and math = MEASURED from the person using the keyboard. Demo history = SIMULATED.

**Fault tolerance:** every saved value, every baseline and every best value is stored in three copies, voted on read, and repaired by the scrubber once per second. It is the same `tmr.c` code as the vitals (no second voter). Serial lines `AINJ` / `AEVT` show injected and repaired faults. The history position counters are plain integers and are not protected.

**Own-baseline rule (demo thresholds, not clinical limits; one `#define` block in `astro.c`):** baseline = median of the first 5 check-in entries, or the first 3 test sessions. Status = median of the last 3 entries compared with that baseline: 10 % worse = AMBER, 25 % worse = RED. Blood sugar: a change either way counts. Mood, memory, math: lower is worse. Reaction: higher is worse. Until the baseline exists the screen says "Building baseline (n/N)". We give no clinical reference ranges.

**Limits of this part (honest list):**
- Timing resolution is **10 ms** (PIT at 100 Hz). In QEMU the keyboard path adds extra delay, so reaction numbers there are for testing the logic, not for judging a person.
- The history (8 entries per indicator) is in RAM and is **lost on reboot**. "Sessions this boot" counts only this boot. There is no real-time clock, so there are no day counts or streaks.
- "CHECK-IN DUE" is an **uptime-based demo reminder** (120 s since boot or the last check-in), not a real schedule.
- Guidance is general information, not medical advice, and is not a diagnosis. It always points to mission medical procedures and the flight surgeon.
- Serial telemetry added: `CHK` (sugar, mood), `COG` (reaction, memory, math), `AINJ`, `AEVT`, `DEMO`. Demo entries carry a `SIM` suffix. A small panel in the Ground Station shows them.
## Ground Station

`ground_station/ground_station.html` is a browser dashboard designed by Kayla. Run `python3 bridge.py` (see Run it), then open the file in a browser.

- **Live from the kernel:** the faults-injected and repaired counts, the event lines, and the start of the replay come from the kernel's serial telemetry.
- **Live from the simulator:** the five vital values and their charts show what the simulator sent to the kernel. They are typed in by the astronaut (`VIT` telemetry) or, with `--sim`, SIMULATED; the badge says which.
- **Replay:** when the kernel starts its 8-week replay, the dashboard runs the same rule on the same 56 simulated values and shows AMBER at day 47. The kernel does not send per-day data over serial, so this part is a recomputation, not a live feed.
- Without the bridge the page says NOT CONNECTED.
- Reload the page after you restart the kernel. An open page keeps the alert badge from the previous run.
- The page needs no internet: Chart.js is bundled as `ground_station/chart.umd.js`.

## Limits (honest list)

- Only the stored baselines and the astronaut-console values (three copies each) are protected. The rest of the kernel is not. The Tab key and the 20 of 20 evidence (`x`) inject into the heart-rate baseline only; the `I` key injects into the astronaut-console values.
- Two corruptions of the same bit in two copies inside one scrub window would defeat the vote.
- The fault is simulated. We did not test with real radiation.
- We call it **fault-tolerant**. We do not claim it is radiation-proof.
- Thresholds are demo values, not medical advice.
- Vitals are typed in by the astronaut; nothing checks they came from a real device (or, with `--sim`, they are simulated). We have not validated it on a person.
- Blood sugar and mood are **typed in by the astronaut** (console key 2). Nothing checks that the number came from a real device. The five vitals are typed in on console key 6 (same checks, same protected storage) or arrive over the serial protocol from the simulator.
- The main-screen advice is one short line per level. The explanations live in the Health guide (console key 1) and are general information, not medical advice.
- Console history is in RAM only and is lost on reboot.

## Input protocol

The kernel reads 4-character lines on COM1 (115200 baud): `h072` heart rate, `o098` SpO2, `t366` temperature ×10, `p120` systolic blood pressure, `s075` sleep in tenths of an hour, `b1` inject a fault. There is no sensor hardware in the submission.

## AI use disclosure

Claude (Anthropic) wrote most of the kernel code from the start of the hackathon, plus the Python scripts (including `bridge.py`), the live wiring of the Ground Station, and the first draft of this README. Kayla designed the Ground Station layout; Claude rewired it to live data. The team designed the project, directed the work, built, tested and verified every claim, and ran all measurements. Claude also cut and captioned the demo video from the team's own screen recordings and generated its sound effects. The tag `pre-hackathon` marks the repository as it was before the hackathon started; the commit history after that tag is the hackathon work. The astronaut console (`astro.c`, `guide.c`), the bridge/Ground Station additions and this section were also AI-assisted (Claude, Anthropic); the team directed and tested them.

## Team

- **Wallance Wong**: kernel, fault tolerance, integration
- **Kayla Wee**: Ground Station web dashboard (design)
- **Nathania Chua**: proofreading and filming support
- Advisor: Mr. Wong Qing Hao

## Credits

- Font: DejaVu Sans Mono Bold (see `FONT-LICENSE.txt`).
- Charts: Chart.js 4.4.0, MIT licence, bundled in `ground_station/`.
