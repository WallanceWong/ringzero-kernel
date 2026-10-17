# ringzero-kernel

A small x86 (32-bit) kernel written from scratch in C and NASM assembly, booted by GRUB2 on BIOS.

## Built so far
- Multiboot boot via GRUB2, 800x600x32 framebuffer
- GDT, IDT and interrupt handling, PIC remapping
- PIT timer at 100 Hz, PC speaker driver
- PS/2 keyboard driver: scan codes to ASCII, the interrupt stores keys in a ring buffer, the main loop reads them
- Serial port output (COM1) for debugging
- Pixel, rectangle and bitmap-font text drawing
- Parser for simple text sensor lines (`h072`, `o098`, `t366`)

## Build and run
```
make run-iso      # builds os.iso with grub-mkrescue, runs in QEMU
```
Needs: gcc (32-bit), nasm, ld, grub-mkrescue (with xorriso and mtools), qemu-system-i386.

To boot on real hardware, write `os.iso` to a USB stick and boot it in Legacy/CSM (BIOS) mode with Secure Boot off.

## Measurements
| Test | QEMU (Virtual Machine) | Real hardware |
|---|---|---|
| Timer (100 Hz PIT) | 61 ticks in 61 s | on-screen square flips every 1.00 s (10 s sample, within 0.3%) |
| Full-screen 800x600 fill | 26.7 ms per-pixel, 2.4 ms with row writes | not measured |
| PC speaker | audible (QEMU audio) | no sound: this laptop has no usable PC speaker path |
| USB keyboard | n/a | works (BIOS legacy USB emulation) |

## Credits and licence
- This is my project. I built it by following *The Little Book about OS Development* by Erik Helin and Adam Renberg.
- Font: `font.h` is generated from DejaVu Sans Mono Bold, under the Bitstream Vera licence (see `FONT-LICENSE.txt`).
- The code in this repository is MIT licensed (see `LICENSE`). The font and the book keep their own terms.
