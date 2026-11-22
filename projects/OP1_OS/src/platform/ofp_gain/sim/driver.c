/* simulator bench driver: runs xcheck_run on the Blackfin model; results are read by run_gainsim.py through gdb */
#include "../xcheck.h"

volatile uint32_t sum, cyc[XCHECK_N];

static uint32_t cycles(void)
{
    uint32_t c;

    __asm__ volatile("%0 = CYCLES;" : "=d"(c));
    return c;
}

int main(void)
{
    uint32_t c[XCHECK_N];
    int i;

    sum = xcheck_run(cycles, c);
    for (i = 0; i < XCHECK_N; i++)
        cyc[i] = c[i];
    return 0;
}
