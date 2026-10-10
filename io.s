global outb ; make the label outb visible outside this file

; outb - send a byte to an I/O port
; stack: [esp + 8] the data byte
;        [esp + 4] the I/O port
;        [esp    ] return address
outb:
    mov al, [esp + 8]   ; the data byte
    mov dx, [esp + 4]   ; the I/O port
    out dx, al          ; send the data to the I/O port
    ret

global inb

; inb - returns a byte from the given serial_is_transmit_fifo_emptyI/O port
; stack: [esp + 4] the address of the I/O port
;        [esp    ] the return address
inb:
    mov dx, [esp + 4]   ; the port is the first argument
    in  al, dx          ; read a byte from that port into al
    ret                 ; the result is returned in al