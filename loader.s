KERNEL_STACK_SIZE equ 4096          ; size of stack in bytes
global loader                       ; the entry symbol for ELF
extern kmain

MAGIC_NUMBER equ 0x1BADB002
FLAGS        equ 0x4                ; bit 2 = "I want a video mode"
CHECKSUM     equ -(MAGIC_NUMBER + FLAGS)

section .text                       ; NO colon (book typo)
align 4
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0                ; 5 unused address fields (they must be present)
    dd 0                            ; mode type: 0 = linear graphics
    dd 800                          ; width
    dd 600                          ; height
    dd 32                           ; depth (bits per pixel)

loader:
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    push ebx                        ; pass GRUB's info pointer to kmain
    call kmain

.loop:
    jmp .loop

section .bss
align 4
kernel_stack:
    resb KERNEL_STACK_SIZE