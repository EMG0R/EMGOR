/* simulator bench: draws both probe pages with made-up measurements into a panel buffer (screenshots) and exercises
 * the key toggle. Includes ofp_probe.c so the statics can be filled. */
#define OFP_SIM 1
#include "../ofp_probe.c"

#define FB ((uint16_t *)0x03000000)
volatile uint32_t result;

int main(void)
{
    uint32_t bad = 0;
    int i;

    /* key toggle: only shift + speech, off -> 1 -> 2 -> off */
    *TE_SHIFT_HELD = 0;
    if (ofp_key(TE_KEY_SPEECH, 1) || page != 0) bad |= 1;
    *TE_SHIFT_HELD = 1;
    if (ofp_key(TE_KEY_T1, 1) || page != 0) bad |= 2;
    if (!ofp_key(TE_KEY_SPEECH, 1) || page != 1) bad |= 4;
    if (!ofp_key(TE_KEY_SPEECH, 0) || page != 1) bad |= 8;
    if (!reset_req) bad |= 16;

    ofp_core_reset(&core);
    core.nominal = 580498; core.aud_pct = 31; core.aud_pk = 58; core.max_d = 410000; core.hot = 0; core.late = 2;
    core.max_p = 600000; core.min_p = 570000; core.blocks = 123456;
    cpu_now = 47; cpu_pk = 72;
    tlsf_used = 3355443; tlsf_peak = 4404019; heap_free[0] = 13000000; heap_size[0] = 16776960; heap_free[1] = 9000000;
    heap_size[1] = 16515000; flash_free_mb = 312; flash_total_mb = 380; tx_peak = 0x5a000000; isr_alive = 1;
    core.usb_seen = 1; core.lead_now = 118; core.lead_min = 62; core.lead_max = 171; core.usb_und = 0; core.usb_ovr = 0;
    core.usb_jump = 1; core.lead_base16 = 1700; core.lead_avg16 = 1760; core.lead_secs = 340; core.late_usb = 1;
    tw_n = 14; tw_last = 3000000; tw_max = 9000000; tw_slow = 1; core.late_tape = 0;
    for (i = 0; i < 2; i++) {
        page = (uint8_t)(i + 1);
        ofp_frame(FB + 0);
        if (i == 0) {                       /* copy page 1 aside for the second screenshot */
            uint32_t *s = (uint32_t *)FB, *d = (uint32_t *)0x03100000;
            unsigned n;
            for (n = 0; n < 320 * 512 / 4; n++) d[n] = s[n];
        }
    }
    result = bad;
    return (int)bad;
}
