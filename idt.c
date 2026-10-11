struct idt_entry {
    unsigned short offset_low;   /* 16 bits */
    unsigned short selector;     /* 16 bits */
    unsigned char zero;         /*  8 bits */
    unsigned char type_attr;    /*  8 bits */
    unsigned short offset_high;  /* 16 bits */
} __attribute__((packed));

struct idt_ptr {
    unsigned short size;         /* 16 bits */
    unsigned int address;      /* 32 bits */
} __attribute__((packed));

void idt_load(struct idt_ptr *ip);   /* will be written in assembly later */

static struct idt_entry idt[256];   /* how many entries does x86 allow? */
static struct idt_ptr ip;

void idt_set_entry(unsigned char i, unsigned int handler)
{
    idt[i].offset_low  = handler & 0xFFFF;
    idt[i].offset_high = (handler >> 16) & 0xFFFF;
    idt[i].selector    = 0x08;       /* kernel code selector from your GDT */
    idt[i].zero        = 0;
    idt[i].type_attr   = 0x8E;
}

void interrupt_handler_0(void);
void interrupt_handler_1(void);
void interrupt_handler_2(void);
void interrupt_handler_3(void);
void interrupt_handler_4(void);
void interrupt_handler_5(void);
void interrupt_handler_6(void);
void interrupt_handler_7(void);
void interrupt_handler_8(void);
void interrupt_handler_9(void);
void interrupt_handler_10(void);
void interrupt_handler_11(void);
void interrupt_handler_12(void);
void interrupt_handler_13(void);
void interrupt_handler_14(void);
void interrupt_handler_15(void);
void interrupt_handler_16(void);
void interrupt_handler_17(void);
void interrupt_handler_18(void);
void interrupt_handler_19(void);
void interrupt_handler_20(void);
void interrupt_handler_21(void);
void interrupt_handler_22(void);
void interrupt_handler_23(void);
void interrupt_handler_24(void);
void interrupt_handler_25(void);
void interrupt_handler_26(void);
void interrupt_handler_27(void);
void interrupt_handler_28(void);
void interrupt_handler_29(void);
void interrupt_handler_30(void);
void interrupt_handler_31(void);
void interrupt_handler_32(void);
void interrupt_handler_33(void);

static void (*const exception_stubs[32])(void) = {
    interrupt_handler_0,  interrupt_handler_1,  interrupt_handler_2,  interrupt_handler_3,
    interrupt_handler_4,  interrupt_handler_5,  interrupt_handler_6,  interrupt_handler_7,
    interrupt_handler_8,  interrupt_handler_9,  interrupt_handler_10, interrupt_handler_11,
    interrupt_handler_12, interrupt_handler_13, interrupt_handler_14, interrupt_handler_15,
    interrupt_handler_16, interrupt_handler_17, interrupt_handler_18, interrupt_handler_19,
    interrupt_handler_20, interrupt_handler_21, interrupt_handler_22, interrupt_handler_23,
    interrupt_handler_24, interrupt_handler_25, interrupt_handler_26, interrupt_handler_27,
    interrupt_handler_28, interrupt_handler_29, interrupt_handler_30, interrupt_handler_31,
};

void idt_init(void)
{
    ip.size    = sizeof(idt) - 1;
    ip.address = (unsigned int)&idt;
    for (int i = 0; i < 32; i++)
        idt_set_entry(i, (unsigned int)exception_stubs[i]);
    idt_set_entry(32, (unsigned int)interrupt_handler_32);
    idt_set_entry(33, (unsigned int)interrupt_handler_33);
    idt_load(&ip);   /* load the IDT */
}



