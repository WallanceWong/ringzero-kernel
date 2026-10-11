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

no_error_code_handler 0
no_error_code_handler 1
no_error_code_handler 2
no_error_code_handler 3
no_error_code_handler 4
no_error_code_handler 5
no_error_code_handler 6
no_error_code_handler 7
error_code_handler 8
no_error_code_handler 9
error_code_handler 10
error_code_handler 11
error_code_handler 12
error_code_handler 13
error_code_handler 14
no_error_code_handler 15
no_error_code_handler 16
error_code_handler 17
no_error_code_handler 18
no_error_code_handler 19
no_error_code_handler 20
no_error_code_handler 21
no_error_code_handler 22
no_error_code_handler 23
no_error_code_handler 24
no_error_code_handler 25
no_error_code_handler 26
no_error_code_handler 27
no_error_code_handler 28
no_error_code_handler 29
no_error_code_handler 30
no_error_code_handler 31
no_error_code_handler 32
no_error_code_handler 33
