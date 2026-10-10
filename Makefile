OBJECTS = loader.o kmain.o fb.o io.o serial.o string.o fb_move_cursor.o gdt.o gdt_load.o idt.o idt_load.o interrupt.o isr.o pic.o pit.o speaker.o gfx.o sensor.o kbd.o
CC = gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
-nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c
LDFLAGS = -T link.ld -melf_i386
AS = nasm
ASFLAGS = -f elf

all: kernel.elf

kernel.elf: $(OBJECTS)
	ld $(LDFLAGS) $(OBJECTS) -o kernel.elf

os.iso: kernel.elf iso/boot/grub/grub.cfg
	cp kernel.elf iso/boot/kernel.elf
	grub-mkrescue -o os.iso iso

run-iso: os.iso
	qemu-system-i386 -cdrom os.iso -serial stdio

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf *.o kernel.elf os.iso
run: kernel.elf
	qemu-system-i386 -kernel kernel.elf -serial stdio
