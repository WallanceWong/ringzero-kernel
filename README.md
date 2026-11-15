
# A1-37 VIGIL

**A fault-tolerant crew health monitor that runs on its own bare-metal kernel.**
Team CHS Ring Zero, Catholic High School Sibu. Sarawak Space Challenge 2026 (NASA Space Apps), Junior category.

> **Read this first.** All vital signs in this project are **SIMULATED**. There is no sensor hardware in the submission. The screen shows a permanent "VITALS: SIMULATED" badge, and the 8-week replay is labelled SIMULATED too. What is real is the kernel, the fault-tolerance mechanism, and the measured repair times.

**Demo video (30 s):** TODO(team): paste the YouTube link here

---

## Why we built it

NASA's Human Research Program groups the risks of human spaceflight into five hazards: space radiation, isolation and confinement, distance from Earth, gravity fields, and hostile or closed environments. Two matter for us:

- **Distance from Earth.** The further a crew travels, the harder it is to get help, supplies and instructions from Earth. A crew health monitor has to keep working without a ground team looking over its shoulder.
- **Space radiation.** NASA lists it among the most hazardous aspects of spaceflight. Our own engineering reasoning (not a NASA claim) is that radiation can flip bits in computer memory, so a health monitor that stores a baseline in memory could quietly learn a wrong "normal".
VIGIL learns a crew member's resting baseline, protects it against memory corruption, and warns early when a sustained drift appears.

## NASA and agency resources used

- NASA Human Research Program, *5 Hazards of Human Spaceflight*: https://www.nasa.gov/hrp/hazards
- NASA Human Research Program, *Risks of Human Spaceflight*: https://www.nasa.gov/hrp/risks
- Canadian Space Agency, *Monitoring radiation for astronaut health* (26 Feb 2026): https://www.asc-csa.gc.ca/eng/news/articles/2026/2026-02-26-monitoring-radiation-astronaut-health.asp
We used these for the problem framing only. No NASA data, logos or images are used in the project.

## What it does

| Part | What it does |
|---|---|
| Baseline | Learns the mean of the first 10 heart-rate samples, then locks it. |
| Triple copy (software TMR) | Stores three copies of the baseline and takes a bitwise majority vote. |
| Scrubber | Once per second, checks the three copies and repairs a disagreeing one. |
| Drift detector | Compares the median of the last 7 readings with the baseline. Levels: GREEN, AMBER, RED. |
| Next step | AMBER: "Rest, hydrate, re-measure within 24 hours". RED: "Contact the flight surgeon". GREEN: "Continue routine monitoring". |
| Heartbeat | A square on screen toggles every 0.25 s. If the kernel freezes, it stops. |
| Serial telemetry | The kernel logs `INJ`, `EVT`, `REPLAY`, `RUN`, `FREEZE` lines on COM1. |

The screen is 800×600 and has panels for the protected baseline, status, fault monitor, 8-week replay and next step.

**Thresholds are demo values, not clinical values.** Heart rate: 10 % drift = AMBER, 25 % = RED, sustained for 3 readings.

## What is real and what is simulated

| Real | Simulated |
|---|---|
| The bare-metal i686 kernel (boots from GRUB, own drivers) | Every vital sign (heart rate, SpO2, temperature) |
| The triple-copy vote and scrubber | The 8-week replay data (`gen_replay.py`, seed 37) |
| The fault injection (a bit flipped in one copy) | The "radiation" upset. It is a software bit flip, not a radiation test. |
| The measured repair times | |

Heart rate is the vital the system actually analyses. SpO2 and temperature are fed in as steady simulated values. Blood sugar and mood are **entered by the astronaut** in the astronaut console (see below); they are not read from a sensor.

## Evidence

**Fault injection (real run, `fault.log`, summarised by `fault_table.py`):**
- 20 faults injected, **20 corrected**.
- Time from injection to repair: **160 to 660 ms, mean 410 ms**.
- Faults were injected at a fixed 1.5 s spacing, so this run does not sample every possible timing against the 1 s scrub cycle.
**8-week replay (simulated, `gen_replay.py`):**
- 56 daily resting heart-rate values, baseline 63.
- Rule: 7-day rolling median at least 10 % above baseline for 3 consecutive days. The same rule runs in the kernel, the Ground Station dashboard and the script.
- Result: **AMBER on day 47**. The kernel's final median is 77, which is +22 % (AMBER, below the RED threshold of 25 %).
**Bare-metal proof:** TODO(team): add the Dell boot photo here (check there are no faces or reflections of faces)

## Run it

Needs Nix (or install grub, xorriso, mtools, qemu yourself).

```
nix-shell -p grub2 xorriso mtools qemu --run "make run-iso"
```

This builds `os.iso` and opens it in QEMU. The kernel needs no input to run, but the baseline only locks after vitals arrive.

**Feed simulated vitals (second terminal setup):**

```
nix-shell -p qemu --run 'qemu-system-i386 -cdrom os.iso -serial pty > /tmp/q.out 2>&1 & Q=$!; sleep 4; P=$(grep -o "/dev/pts/[0-9]*" /tmp/q.out | head -1); echo "serial port: $P"; python3 sim_vitals.py "$P"; kill $Q'
```

This starts QEMU, finds the serial port, and runs `sim_vitals.py`, which sends one simulated reading per second.

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
| D (in the menu) | Load a deterministic DEMO HISTORY (SIMULATED) so drift to AMBER/RED can be shown quickly |
| I | Inject a fault into the astronaut channels (Tab still hits heart rate only) |

**Where each number comes from (labelled on screen):** the 5 vitals = SIMULATED. Blood sugar and mood = ENTERED BY THE ASTRONAUT. Reaction, memory and math = MEASURED from the person using the keyboard. Demo history = SIMULATED.

**Fault tolerance:** every saved value, every baseline and every best value is stored in three copies, voted on read, and repaired by the scrubber once per second. It is the same `tmr.c` code as the vitals (no second voter). Serial lines `AINJ` / `AEVT` show injected and repaired faults. The history position counters are plain integers and are not protected.

**Own-baseline rule (demo thresholds, not clinical limits; one `#define` block in `astro.c`):** baseline = median of the first 5 check-in entries, or the first 3 test sessions. Status = median of the last 3 entries compared with that baseline: 10 % worse = AMBER, 25 % worse = RED. Blood sugar: a change either way counts. Mood, memory, math: lower is worse. Reaction: higher is worse. Until the baseline exists the screen says "Building baseline (n/N)". We give no clinical reference ranges.

**Limits of this part (honest list):**
- Timing resolution is **10 ms** (PIT at 100 Hz). In QEMU the keyboard path adds extra delay, so reaction numbers there are for testing the logic, not for judging a person.
- The history (8 entries per indicator) is in RAM and is **lost on reboot**. "Sessions this boot" counts only this boot. There is no real-time clock, so there are no day counts or streaks.
- "CHECK-IN DUE" is an **uptime-based demo reminder** (120 s since boot or the last check-in), not a real schedule.
- Guidance is general information, not medical advice, and is not a diagnosis. It always points to mission medical procedures and the flight surgeon.
- Serial telemetry added: `CHK` (sugar, mood), `COG` (reaction, memory, math), `AINJ`, `AEVT`, `DEMO`. Demo entries carry a `SIM` suffix. A small panel in the Ground Station shows them.

## Limits (honest list)

- Only the three baseline copies are protected. The rest of the kernel is not.
- Two corruptions of the same bit in two copies inside one scrub window would defeat the vote.
- The fault is simulated. We did not test with real radiation.
- We call it **fault-tolerant**. We do not claim it is radiation-proof.
- Thresholds are demo values, not medical advice.
- All vitals are simulated. We have not validated it on a person.
## Input protocol

The kernel reads 4-character lines on COM1 (115200 baud): `h072` heart rate, `o098` SpO2, `t366` temperature ×10, `b1` inject a fault.

## AI use disclosure

The astronaut console (`astro.c`, `guide.c`), the bridge/Ground Station additions and this section were also AI-assisted (Claude, Anthropic); the team directed and tested them. Claude (Anthropic) wrote most of the kernel code from the start of the hackathon, plus the Python scripts and the first draft of this README. The team designed the project, directed the work, built, tested and verified every claim, and ran all measurements. Commit history shows what existed before the hackathon (tag `pre-hackathon`). TODO(team): one sentence on what existed before the hackathon.

## Team

- **Wallance Wong**: kernel, fault tolerance, integration
- **Kayla Wee**: Ground Station web dashboard
- **Nathania Chua**: proofreading and filming support
- Advisor: Mr. Wong Qing Hao
TODO(team): confirm names and roles are as you want them shown.

## Credits

Font: DejaVu Sans Mono Bold (see `FONT-LICENSE.txt`).

