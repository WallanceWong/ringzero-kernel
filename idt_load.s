global idt_load
section .text
idt_load:
    mov eax, [esp+4]
    lidt [eax]          ; the instruction that loads the IDT register
    ret