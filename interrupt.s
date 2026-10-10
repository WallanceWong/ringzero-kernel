extern interrupt_handler
section .text

%macro no_error_code_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
    push dword 0            ; fake error code
    push dword %1           ; interrupt number
    jmp common_interrupt_handler
%endmacro

%macro error_code_handler 1
global interrupt_handler_%1
interrupt_handler_%1:
    push dword %1           ; CPU already pushed the error code
    jmp common_interrupt_handler
%endmacro

common_interrupt_handler:
    pushad                   ; save all general registers in ONE instruction
    push esp                ; pass a pointer to the saved state (cdecl argument)
    call interrupt_handler
    add esp, 4           ; remove that argument (how many bytes?)
    popad                    ; restore all general registers
    add esp, 8           ; remove the interrupt number and error code
    iret                    ; return from interrupt (not `ret`)

no_error_code_handler 3     ; test interrupt: the `int 3` instruction
no_error_code_handler 32    ; IRQ0, the timer
no_error_code_handler 33    ; IRQ1, the keyboard