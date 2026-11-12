// proto.h -- the CDC line protocol (docs/PROTOCOL.md). Hardware-free: the firmware and the host
// simulator both drive it through ProtoOps; byte transport is whatever `tx` / protoRx() connect to.
#pragma once
#include <stdint.h>
#include <stddef.h>

enum ProtoMode : uint8_t { PM_STANDALONE = 0, PM_NEPTR, PM_STUDIO, PM_EDIT };
const char* protoModeName(ProtoMode m);

struct ProtoOps {
    // byte transport
    size_t   (*tx)(const uint8_t* d, size_t n);       // returns bytes accepted (may be < n)
    size_t   (*tx_space)(void);                       // free bytes in the transport's TX FIFO
    bool     (*bus_idle)(void);                       // MIDI TX quiet: bulk chunks allowed
    uint32_t (*now_ms)(void);
    const char* fw;                                   // firmware version string
    // patch / state
    uint8_t  (*patch_slot)(void);
    uint32_t (*patch_epoch)(void);                    // bumps on every patch load / new / clear
    void     (*param_changed)(void);                  // a host SET_PARAM landed: mark the patch dirty (autosave)
    bool     (*follow_locked)(void);                  // tempo belongs to the external clock
    void     (*status)(char* buf, size_t n);          // "clk=.. running=.. bpm=.. ..."
    // sample pool (id = patch*16 + slot)
    int      (*pool_scan)(uint8_t patch);             // 1 ready, 0 pending (call again), -1 patch has no audio
    uint32_t (*pool_len)(uint8_t patch, uint8_t slot);
    uint32_t (*pool_ver)(uint8_t patch, uint8_t slot);
    int32_t  (*pool_read)(uint16_t id, uint32_t off, uint8_t* buf, uint32_t max);   // bytes, 0 = end, -1 err, -2 pending
    int      (*assign)(uint8_t slot, uint16_t id);    // 0 = queued (completion via protoSampleDone)
    int      (*del)(uint8_t slot);
    int      (*clear)(void);
    void     (*audition)(char kind, uint8_t a, uint8_t vel);
    void     (*set_clockmode)(uint8_t m);             // 0 AUTO 1 INTERNAL 2 FOLLOW 3 MASTER (persisted by the firmware)
    // events
    void     (*legacy_line)(const char* line);        // console lines (no '!' prefix)
    void     (*mode_changed)(ProtoMode m);
    // raw card files, read-only: kind 'P' patch .PMR, 'K' its .BAK, 'A' audio sidecar .AUD, 'L' .AUD.BAK. bytes, 0 = end, -1 error
    int32_t  (*file_read)(char kind, uint16_t slot, uint32_t off, uint8_t* buf, uint32_t max, uint32_t* total);   // + 'M' MANIFEST.TXT, 'T' PATCHES/Pnn.TXT, 'R' PRESETS/<engine#>.TXT, 'W' SAMPLES/S<id>.WAV
    int      (*save_scratch)(uint8_t slot);            // SAVE_SCRATCH: write the live patch into an EMPTY slot (both layouts), 0 ok, -1 refused, -2 io
    // optional new-layout sample pool (MANIFEST.TXT): global sample ids. NULL or man_count()==0 -> legacy patch*16+slot manifest
    int      (*man_count)(void);
    bool     (*man_at)(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* ver, char* name /*[16]*/);
    uint32_t (*sample_len)(uint16_t id);
    uint16_t (*alias)(uint16_t old_id);               // old patch*16+slot -> new pool id (0 = none)              // samples of a pool id (0 = unknown)
    int      (*store_retry)(void);                     // STORE_RETRY: clear the skip flag, rerun the storage boot tasks (0 queued)
    void     (*service)(void);                         // keep USB / MIDI clock serviced between bulk chunks (pmor_usb_service on the device)
    int      (*join_begin)(uint16_t spp16);            // JOIN: 0 accepted, -1 no live clock
    int      (*join_poll)(int32_t* late_us);           // 1 applied, 0 waiting, <0 gave up
    void     (*nosave)(bool on);                      // suppress autosave/QSPI writes while a test session mutates RAM
    void     (*tx_reset)(void);                       // optional: TX watchdog fired, drop whatever the transport still holds (tud_cdc_write_clear)
    void     (*tempo_seed)(uint32_t bpm_milli);       // optional: TEMPO <bpm_milli> seeds the follower tempo (usb/clock_engine.c)
    int      (*save_preset)(int engine, int n, const char* name, int pad);   // optional: SAVE_PRESET (pad -1 = the group's first pad), 0 ok, -1 arg, -2 io, -3 nosave
    // optional recycle bin (store/sdlayout.h storeBin*): every card op is one bounded step per protoPoll pass
    int      (*refs_scan)(uint16_t id, unsigned first, unsigned n);   // patches in [first, first+n) referencing the pool id
    int      (*bin_delete)(uint16_t id);                              // 0 ok, -1 no such sample, -2 bin full, -3 io, -4 unsupported, -5 nosave
    int      (*bin_count)(void);
    bool     (*bin_at)(int i, uint16_t* id, uint32_t* len, uint32_t* at);
    uint32_t (*bin_token)(void);
    int      (*bin_restore)(uint16_t id);                             // 0 ok, -1 not in bin, -2 id taken, -3 io
    int      (*bin_empty)(uint32_t token);                            // erased count, -1 token mismatch, -3 io
    // optional ARCHIVE_PATCHES (rename-only, background job owned by the device, NOT the session): scan accumulates hash + patch count for slots [first, first+n);
    // begin starts (or resumes the unfinished ARCHIVE_<k>): k>=1, -3 io, -5 nosave; list = patch files still on the card for a slot (names into out), count;
    // state fills "state=running|idle k= moved= failed= fail=..." and returns 1 while the job runs
    int      (*arch_scan)(unsigned first, unsigned n, uint32_t* hash, uint16_t* patches);
    int      (*arch_begin)(void);
    int      (*arch_list)(unsigned slot, char* out, size_t n);
    int      (*arch_state)(char* out, size_t n);
};

void protoInit(const ProtoOps* ops);
void protoRx(const uint8_t* d, size_t n);             // feed received CDC bytes (any chunking)
void protoPoll(void);                                 // call every main-loop pass while the port is open
void protoConnected(bool dtr);                        // port opened / closed (DTR)
ProtoMode protoMode(void);
bool protoActive(void);                               // STUDIO or EDIT
uint32_t protoSchemaHash(void);
void protoSampleDone(uint8_t slot, int err);          // async ASSIGN_SAMPLE result (0 = ready)
uint32_t protoStatBulkChunks(void);
void protoUsbReset(void);                             // the transport saw a mount/unmount/suspend/resume edge: start a clean session
uint32_t protoStatResets(int cause);                  // session resets by cause: 0 dtr_fall 1 dtr_rise 2 usb 3 liveness 4 bye 5 tx_stall 6 hello(aborted old job/backlog)
void protoPresetsChanged(int engine);                  // push PRESETS_CHANGED <engine> (the PRESETS file of that engine was rewritten)
uint32_t protoStatTxStalls(void);                     // TX watchdog self-heals (STATUS tx_stalls=)
uint32_t protoDebugJob(void);                         // (job kind << 24) | tx bytes pending: STATUS job=
