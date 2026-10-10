struct gdt_entry {
    unsigned short limit_low;      /* 16 bits */
    unsigned short base_low;       /* 16 bits */
    unsigned char base_mid;       /*  8 bits */
    unsigned char access;         /*  8 bits */
    unsigned char granularity;    /*  8 bits */
    unsigned char base_high;      /*  8 bits */
} __attribute__((packed));

struct gdt_ptr {
    unsigned short size;           /* 16 bits */
    unsigned int address;        /* 32 bits */
} __attribute__((packed));

void gdt_load(struct gdt_ptr *gp);
static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

static void gdt_set_entry(int i, unsigned int base, unsigned int limit,
                          unsigned char access, unsigned char gran)
{
    gdt[i].base_low  = base & 0xFFFF;              /* low 16 bits of base */
    gdt[i].base_mid = (base >> 16) & 0xFF;    /* bits 16-23 */
    gdt[i].base_high = (base >> 24) & 0xFF;    /* bits 24-31 */

    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void)
{
    gp.size    = sizeof(gdt) - 1;            /* table size in bytes, minus 1 */
    gp.address = (unsigned int) &gdt;        /* where the table lives */

    gdt_set_entry(0, 0, 0, 0, 0);                        /* null descriptor */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);            /* kernel code: base, limit */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);            /* kernel data: base, limit */

    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_load(&gp);
}


