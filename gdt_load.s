global gdt_load

; gdt_load - load the GDT and reload the segment registers
; stack: [esp + 4] pointer to the gdt_ptr struct
;        [esp    ] return address
gdt_load:
    mov eax, [esp + 4]      ; the argument: which offset?
    lgdt [eax]                 ; load the table (note the brackets)

    mov ax, 0x10               ; the kernel DATA selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush            ; far jump: the kernel CODE selector, then the label
.flush:
    ret