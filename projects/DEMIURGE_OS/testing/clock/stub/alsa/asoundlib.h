// testing/clock/stub/alsa/asoundlib.h
//
// Minimal stand-in for <alsa/asoundlib.h> so demiurge-clock.cpp can be
// TYPE-CHECKED / COMPILED on macOS (or any box without real ALSA), as a
// whole-file compile sanity check. This is NOT a functional ALSA
// implementation -- every function is a stub that does nothing useful.
// Never link this into anything that actually needs to talk to ALSA.
//
// Kept intentionally tiny: only what src/demiurge-clock.cpp actually
// references. If the daemon starts using more of the real API, add the
// matching stub here rather than reaching for the real header.
#pragma once

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_seq snd_seq_t;

#define SND_SEQ_OPEN_DUPLEX 3

#define SND_SEQ_PORT_CAP_READ       (1 << 0)
#define SND_SEQ_PORT_CAP_WRITE      (1 << 1)
#define SND_SEQ_PORT_CAP_SUBS_READ  (1 << 2)
#define SND_SEQ_PORT_CAP_SUBS_WRITE (1 << 3)

#define SND_SEQ_PORT_TYPE_MIDI_GENERIC (1 << 0)
#define SND_SEQ_PORT_TYPE_APPLICATION  (1 << 20)

typedef enum {
    SND_SEQ_EVENT_START = 1,
    SND_SEQ_EVENT_CONTINUE,
    SND_SEQ_EVENT_STOP,
    SND_SEQ_EVENT_CLOCK,
    SND_SEQ_EVENT_CONTROLLER
} snd_seq_event_type_t;

typedef struct {
    unsigned char channel;
    unsigned int  param;
    int           value;
} snd_seq_ev_ctrl_t;

typedef union {
    snd_seq_ev_ctrl_t control;
} snd_seq_ev_data_t;

typedef struct {
    unsigned char client;
} snd_seq_addr_t;

typedef struct {
    int               type;
    snd_seq_addr_t    source;
    snd_seq_ev_data_t data;
} snd_seq_event_t;

static inline int snd_seq_open(snd_seq_t **h, const char *, int, int) { *h = NULL; return -1; }
static inline void snd_seq_close(snd_seq_t *) {}
static inline void snd_seq_set_client_name(snd_seq_t *, const char *) {}
static inline int snd_seq_client_id(snd_seq_t *) { return -1; }
static inline int snd_seq_create_simple_port(snd_seq_t *, const char *, unsigned int, unsigned int) { return -1; }
static inline int snd_seq_connect_to(snd_seq_t *, int, int, int) { return -1; }
static inline int snd_seq_connect_from(snd_seq_t *, int, int, int) { return -1; }
static inline int snd_seq_event_input(snd_seq_t *, snd_seq_event_t **ev) { *ev = NULL; return -1; }
static inline int snd_seq_event_output_direct(snd_seq_t *, snd_seq_event_t *) { return -1; }

static inline void snd_seq_ev_clear(snd_seq_event_t *ev) { memset(ev, 0, sizeof(*ev)); }
static inline void snd_seq_ev_set_source(snd_seq_event_t *, int) {}
static inline void snd_seq_ev_set_subs(snd_seq_event_t *) {}
static inline void snd_seq_ev_set_direct(snd_seq_event_t *) {}
static inline void snd_seq_ev_set_controller(snd_seq_event_t *ev, int chan, int param, int value) {
    ev->data.control.channel = (unsigned char)chan;
    ev->data.control.param   = (unsigned int)param;
    ev->data.control.value   = value;
}

#ifdef __cplusplus
}
#endif
