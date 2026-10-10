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

void interrupt_handler_3(void);     /* defined in interrupt.s */
void interrupt_handler_33(void);
void interrupt_handler_32(void);

void idt_init(void)
{
    ip.size    = sizeof(idt) - 1;
    ip.address = (unsigned int)&idt;
    idt_set_entry(3, (unsigned int)interrupt_handler_3);   /* which number? */
    idt_set_entry(33, (unsigned int)interrupt_handler_33);
    idt_set_entry(32, (unsigned int)interrupt_handler_32);
    idt_load (&ip);	/* load the IDT */
}



