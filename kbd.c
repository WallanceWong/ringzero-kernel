#define KB_SIZE 64

static volatile char kb_buf[KB_SIZE];
static volatile unsigned int kb_head = 0;     /* only the interrupt writes this */
static volatile unsigned int kb_tail = 0;     /* only the main loop writes this */

void kb_push(char c)                          /* called from the interrupt handler */
{
    unsigned int next = (kb_head + 1) % KB_SIZE;
    if (next == kb_tail)                      /* buffer full: drop the key */
        return;
    kb_buf[kb_head] = c;
    kb_head = next;                           /* move head forward */
}

int kb_pop(void)                              /* returns -1 when empty */
{
    if (kb_tail == kb_head)
        return -1;
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_SIZE;           /* wrap around */
    return c;
}