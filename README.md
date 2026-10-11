# ringzero-kernel

A small x86 (32-bit) kernel written from scratch in C and NASM assembly, booted by GRUB2 on BIOS.

## Built so far
- Multiboot boot via GRUB2, 800x600x32 framebuffer
- GDT, IDT and interrupt handling, PIC remapping
- PIT timer at 100 Hz, PC speaker driver
- PS/2 keyboard driver: scan codes to ASCII, interrupt stores keys in a ring buffer, the main loop draws them
- Serial port output (COM1) for debugging
- Pixel, rectangle and bitmap-font text drawing

## Build and run
```
make run-iso      # builds os.iso with grub-mkrescue, runs in QEMU
```
Needs: gcc (32-bit), nasm, ld, grub-mkrescue (with xorriso and mtools), qemu-system-i386.

To boot on real hardware, write `os.iso` to a USB stick and boot it in Legacy/CSM (BIOS) mode with Secure Boot off.

## Measurements
| Test | QEMU | ASUS laptop (real hardware) |
|---|---|---|
| Timer (100 Hz PIT) | 61 ticks in 61 s | on-screen square flips every 1.00 s (10 s sample, within 0.3%) |
| Full-screen 800x600 fill | 26.7 ms per-pixel, 2.4 ms with row writes | not measured |
| PC speaker | audible (QEMU audio) | no sound at all: this laptop has no usable PC speaker path |

## Credits
- Follows *The Little Book about OS Development* by Erik Helin and Adam Renberg (kernel structure and early chapters).
- Font: `font.h` is generated from DejaVu Sans Mono Bold, under the Bitstream Vera licence.
- Code was typed by me while learning.

## Disclosure (Sarawak Space Challenge 2026)
This repository is a personal open-source project that existed before the 48-hour event on 14-15 November 2026. It contains only general-purpose OS parts, plus two items built before the event and kept in place:
- `sensor.c`: a parser for text sensor lines (`h072`, `o098`, `t366`), written on 10-11 October 2026.
- The on-screen title "A1-37 VIGIL" drawn in `kmain.c`, added on 10-11 October 2026.

Everything else for the competition entry will be written during the event. The last commit before the event is tagged `pre-hackathon`.