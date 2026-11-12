// usb_descriptors.c — Pocket OpGorator composite USB descriptors + UAC2
// control-request handling.
//
// Topology (see README.md for the full writeup):
//
//   IAD -> ITF0 AudioControl (AC)
//            Clock Source (fixed 48kHz)
//            Input Terminal  (stereo analog-in, PCM3060 ADC)
//            Feature Unit    (mute/volume: master, L, R)
//            Output Terminal (USB streaming, host-bound -- this IS the
//                              capture direction: audio flows device->host)
//       ITF1 AudioStreaming (AS), alt 0 (zero-bandwidth) / alt 1 (active)
//            -> EP1 IN, isochronous, asynchronous, no feedback EP
//               (feedback endpoints exist for host->device OUT streams so
//               the device can tell the host its consumption rate; capture
//               is IN-only and free-runs off the device's own 48kHz clock,
//               so there is nothing to feed back -- see README "known
//               unfinished items")
//   ITF2/3  USB-MIDI 1.0 (AC + MS), 1 embedded cable each direction
//       -> EP2 OUT / EP2 IN
//   IAD -> ITF4/5 CDC-ACM (debug/config console)
//       -> EP4 IN (notif), EP3 OUT/IN (data)
//
// Credit: the UAC2 entity/terminal layout here is adapted from TinyUSB's
// own `examples/device/uac2_headset` reference descriptor (mic half only --
// we drop the speaker half entirely, capture-only per design spec) and from
// the community prior art at community.daisy.audio/t/9174 ("TinyUSB UAC2
// 2x2 + CDC serial I/O for the Daisy Seed", nadavb) which validated that a
// UAC2 (not UAC1) descriptor set enumerates fine at full-speed on a Daisy
// Seed's internal FS PHY -- UAC2 does not require high-speed, despite some
// TinyUSB examples defaulting to UAC1 on FS-only ports.

#include <string.h>
#include "tusb.h"
#include "audio_format.h"
#include "usb_descriptors.h"

// Implemented in board_stm32h7.c: reads the STM32's 96-bit factory unique
// ID and formats it as a UTF-16 hex string. We deliberately do NOT depend
// on TinyUSB's hw/bsp board_api.h (board_usb_get_serial()) -- that whole
// board-support layer assumes you're building one of TinyUSB's own example
// boards; this project only borrows TinyUSB's core + class drivers and
// otherwise sits directly on libDaisy, so it gets its own minimal board
// glue instead (see README.md "why no hw/bsp").
extern size_t board_get_serial_utf16(uint16_t *buf, size_t max_chars);

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

// Vendor/Product IDs: pocket-OpGorator-specific placeholder pair (TinyUSB's
// own 0xCafe/0x____ default range, community convention for pre-VID hobby
// projects). Replace with a real VID:PID pair before any wide distribution.
#define USB_VID   0xCafe
#define USB_PID   0x4f70   // 'O','p' -- ad hoc, not registered

static tusb_desc_device_t const desc_device = {
    .bLength         = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB          = 0x0200,

    // Composite device: class/subclass/protocol at the device level say
    // "look at the Interface Association Descriptors" (required whenever a
    // function spans >1 interface, which both our Audio and CDC functions
    // do).
    .bDeviceClass    = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor  = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iSerialNumber = 0x03,
    .iProduct      = 0x02,

    .bNumConfigurations = 0x01};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// UAC2 capture-only (stereo mic-style) descriptor block
//--------------------------------------------------------------------+
// Compiled out entirely for the MIDI-only isolation build
// (POCKET_OPGORATOR_ISOLATION_MIDI => CFG_TUD_AUDIO 0, see tusb_config.h)
// -- see README.md "USB bring-up isolation ladder".
#if CFG_TUD_AUDIO
// Modeled on TinyUSB's own TUD_AUDIO20_MIC_FOUR_CH_DESCRIPTOR template
// (src/device/usbd.h) but for 2 physical channels with an explicit L/R
// channel-config bitmap instead of NON_PREDEFINED, and entity IDs from
// usb_descriptors.h so the control-request handlers below stay in sync.

#define TUD_AUDIO_MIC_STEREO_DESC_LEN                     \
  (TUD_AUDIO20_DESC_IAD_LEN + TUD_AUDIO20_DESC_STD_AC_LEN  \
   + TUD_AUDIO20_DESC_CS_AC_LEN + TUD_AUDIO20_DESC_CLK_SRC_LEN \
   + TUD_AUDIO20_DESC_INPUT_TERM_LEN + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN \
   + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2) /* 2 = L+R, +1 master implicit */ \
   + TUD_AUDIO20_DESC_STD_AS_LEN /* alt 0 */                \
   + TUD_AUDIO20_DESC_STD_AS_LEN /* alt 1 */                \
   + TUD_AUDIO20_DESC_CS_AS_INT_LEN + TUD_AUDIO20_DESC_TYPE_I_FORMAT_LEN \
   + TUD_AUDIO20_DESC_STD_AS_ISO_EP_LEN + TUD_AUDIO20_DESC_CS_AS_ISO_EP_LEN)

// Channel config: explicit Front-Left | Front-Right (not NON_PREDEFINED) --
// tells the host these two logical channels really are a stereo pair, so
// e.g. PipeWire/ALSA label them L/R instead of generic channel 1/2.
#define UAC2_CHANNEL_CONFIG_STEREO \
  (AUDIO20_CHANNEL_CONFIG_FRONT_LEFT | AUDIO20_CHANNEL_CONFIG_FRONT_RIGHT)

#define TUD_AUDIO_MIC_STEREO_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epin, _epsize) \
  /* Interface Association: groups ITF(_itfnum) [AC] + ITF(_itfnum+1) [AS] */ \
  TUD_AUDIO20_DESC_IAD(/*_firstitf*/ _itfnum, /*_nitfs*/ 0x02, /*_stridx*/ 0x00), \
  /* Standard AC Interface (4.7.1) */ \
  TUD_AUDIO20_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx), \
  /* Class-Specific AC Interface Header (4.7.2): category = Microphone \
   * (closest UAC2 function category to "fixed analog line/mic capture \
   * source"; there is no generic "line input" function category in the \
   * UAC2 function-category table). */ \
  TUD_AUDIO20_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO20_FUNC_MICROPHONE, \
      /*_totallen*/ TUD_AUDIO20_DESC_CLK_SRC_LEN + TUD_AUDIO20_DESC_INPUT_TERM_LEN \
        + TUD_AUDIO20_DESC_OUTPUT_TERM_LEN + TUD_AUDIO20_DESC_FEATURE_UNIT_LEN(2), \
      /*_ctrl*/ AUDIO20_CS_AS_INTERFACE_CTRL_LATENCY_POS), \
  /* Clock Source (4.7.2.1): internal, fixed (not programmable, not \
   * selectable from a clock selector -- there is exactly one clock in \
   * this device, the PCM3060's crystal-derived 48kHz). */ \
  TUD_AUDIO20_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, \
      /*_attr*/ AUDIO20_CLOCK_SOURCE_ATT_INT_FIX_CLK, \
      /*_ctrl*/ (AUDIO20_CTRL_R << AUDIO20_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), \
      /*_assocTerm*/ 0x00, /*_stridx*/ 0x00), \
  /* Input Terminal (4.7.2.4): the "analog in" side of the ADC, 2 logical \
   * channels (stereo), clocked from UAC2_ENTITY_CLOCK. */ \
  TUD_AUDIO20_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_INPUT_TERMINAL, \
      /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, \
      /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ 0x02, \
      /*_channelcfg*/ UAC2_CHANNEL_CONFIG_STEREO, /*_idxchannelnames*/ 0x00, \
      /*_ctrl*/ 0, /*_stridx*/ 0x00), \
  /* Feature Unit (4.7.2.8): mute + volume, RW, for master + ch1(L) + \
   * ch2(R). Purely a UAC2-protocol nicety for host mixer apps -- the \
   * firmware does not currently apply gain/mute in the audio path (see \
   * README "known unfinished items"); a future revision can wire these \
   * into the DaisySeed audio callback. */ \
  TUD_AUDIO20_DESC_FEATURE_UNIT(/*_unitid*/ UAC2_ENTITY_FEATURE_UNIT, \
      /*_srcid*/ UAC2_ENTITY_INPUT_TERMINAL, /*_stridx*/ 0x00, \
      /*_ctrlch0master*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) \
                        | (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), \
      /*_ctrlch1*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) \
                  | (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS), \
      /*_ctrlch2*/ (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_MUTE_POS) \
                  | (AUDIO20_CTRL_RW << AUDIO20_FEATURE_UNIT_CTRL_VOLUME_POS)), \
  /* Output Terminal (4.7.2.5): USB streaming -- this is the terminal that \
   * makes this a CAPTURE path. Audio flows Input Terminal -> Feature Unit \
   * -> [here] -> USB (device to host). There is no matching Input \
   * Terminal of type USB_STREAMING anywhere in this descriptor set, i.e. \
   * no host->device audio path exists at all: capture-only by \
   * construction, not just by convention. */ \
  TUD_AUDIO20_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_OUTPUT_TERMINAL, \
      /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, \
      /*_srcid*/ UAC2_ENTITY_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, \
      /*_ctrl*/ 0x0000, /*_stridx*/ 0x00), \
  /* Standard AS Interface (4.9.1), alt 0: zero-bandwidth idle setting. \
   * Required by the UAC2 spec so the host can "turn off" the stream \
   * (select alt 0) without a control transfer -- this is also what a \
   * host does between application opens. */ \
  TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t) ((_itfnum) + 1), \
      /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ 0x00), \
  /* Standard AS Interface (4.9.1), alt 1: the active streaming setting -- \
   * selecting this is what tud_audio_set_itf_cb() below treats as \
   * "host opened the capture stream, start filling the FIFO". */ \
  TUD_AUDIO20_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t) ((_itfnum) + 1), \
      /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx), \
  /* Class-Specific AS Interface (4.9.2): Type I PCM, 2 physical channels. */ \
  TUD_AUDIO20_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_OUTPUT_TERMINAL, \
      /*_ctrl*/ AUDIO20_CTRL_NONE, /*_formattype*/ AUDIO20_FORMAT_TYPE_I, \
      /*_formats*/ AUDIO20_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ 0x02, \
      /*_channelcfg*/ UAC2_CHANNEL_CONFIG_STEREO, /*_stridx*/ 0x00), \
  /* Type I Format Type (2.3.1.6): subslot size (bytes/sample) and bit \
   * resolution come from audio_format.h, i.e. the same POCKET_OPGORATOR_ \
   * AUDIO_16BIT build flag that picks 24-bit-in-3-bytes vs 16-bit-in-2- \
   * bytes for the actual endpoint payload. */ \
  TUD_AUDIO20_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample), \
  /* Standard AS Isochronous Data Endpoint (4.10.1.1): EP1 IN, \
   * asynchronous sync type, no explicit feedback endpoint -- correct for \
   * a capture (IN) pipe, see file header. Max packet size sized for the \
   * worst-case 49-sample drift-compensated packet. */ \
  TUD_AUDIO20_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, \
      /*_attr*/ (uint8_t) (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA), \
      /*_maxEPsize*/ _epsize, /*_interval*/ 0x01), \
  /* Class-Specific AS Isochronous Data Endpoint (4.10.1.2). */ \
  TUD_AUDIO20_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO20_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, \
      /*_ctrl*/ AUDIO20_CTRL_NONE, \
      /*_lockdelayunit*/ AUDIO20_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, \
      /*_lockdelay*/ 0x0000)
#endif  // CFG_TUD_AUDIO

//--------------------------------------------------------------------+
// Full Configuration Descriptor
//--------------------------------------------------------------------+
// Isolation builds (see tusb_config.h / README.md "USB bring-up isolation
// ladder") drop the UAC2 and/or CDC blocks entirely -- both the length
// macro and the descriptor array below follow CFG_TUD_AUDIO/CFG_TUD_CDC so
// there's exactly one place that decides which interfaces exist.
#if CFG_TUD_AUDIO
#define AUDIO_DESC_LEN_TERM  + TUD_AUDIO_MIC_STEREO_DESC_LEN
#else
#define AUDIO_DESC_LEN_TERM
#endif
#if CFG_TUD_CDC
#define CDC_DESC_LEN_TERM    + TUD_CDC_DESC_LEN
#else
#define CDC_DESC_LEN_TERM
#endif

#define CONFIG_TOTAL_LEN \
  (TUD_CONFIG_DESC_LEN AUDIO_DESC_LEN_TERM + TUD_MIDI_DESC_LEN CDC_DESC_LEN_TERM)

// String index used both as the AC interface string and the AS interface
// string (matches the pattern TinyUSB's own templates use).
#define STRID_AUDIO_FUNC   0x04
#define STRID_MIDI_FUNC    0x05
#define STRID_CDC_FUNC     0x06

static uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attributes, power (mA)
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

#if CFG_TUD_AUDIO
    // UAC2 capture: interface, string idx, bytes/sample, bit resolution, EP, EP size
    TUD_AUDIO_MIC_STEREO_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, STRID_AUDIO_FUNC,
        AUDIO_BYTES_PER_SAMPLE, AUDIO_BIT_RESOLUTION,
        EPNUM_AUDIO_IN | 0x80, AUDIO_EP_IN_SZ_MAX),
#endif

    // USB-MIDI 1.0: interface, string idx, EP out, EP in, EP size
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI_COMM, STRID_MIDI_FUNC,
        EPNUM_MIDI, EPNUM_MIDI | 0x80, 64),

#if CFG_TUD_CDC
    // CDC-ACM: interface, string idx, notif EP + size, data EP out/in, EP size
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_COMM, STRID_CDC_FUNC,
        EPNUM_CDC_NOTIF | 0x80, 8,
        EPNUM_CDC_DATA, EPNUM_CDC_DATA | 0x80, 64),
#endif
};

TU_VERIFY_STATIC(sizeof(desc_fs_configuration) == CONFIG_TOTAL_LEN,
                  "Configuration descriptor size mismatch");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  // 0x04.. reserved for the interface strings above
};

static char const *string_desc_arr[] = {
    (const char[]) {0x09, 0x04},  // 0: English (0x0409)
    "EMGOR SYNTH",                // 1: Manufacturer
    "Pocket OpGorator",           // 2: Product -- Demiurge's aggregator matches on this string
    NULL,                         // 3: Serial -- filled from the STM32 unique ID, see below
    "Pocket OpGorator Audio",     // 4: Audio function (AC/AS)
    "Pocket OpGorator MIDI",      // 5: MIDI function
    "Pocket OpGorator Console",   // 6: CDC function
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch (index) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      // Gives every Pocket OpGorator a distinct USB serial number without
      // any provisioning step.
      chr_count = board_get_serial_utf16(_desc_str + 1, 32);
      break;

    default:
      if (!(index < TU_ARRAY_SIZE(string_desc_arr))) return NULL;

      char const *str = string_desc_arr[index];
      chr_count        = strlen(str);
      size_t const max_count = TU_ARRAY_SIZE(_desc_str) - 1;
      if (chr_count > max_count) chr_count = max_count;

      for (size_t i = 0; i < chr_count; i++) _desc_str[1 + i] = (uint16_t) str[i];
      break;
  }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}

//--------------------------------------------------------------------+
// UAC2 control-request handling
//--------------------------------------------------------------------+
// Whole section compiled out for the MIDI-only isolation build -- there
// is no AC/AS interface to field these control requests for when
// CFG_TUD_AUDIO is 0 (tud_audio_*_cb are only ever invoked by TinyUSB's
// own audio_device.c, which itself compiles to nothing when CFG_TUD_AUDIO
// is 0 -- see tusb.h's class-driver include guards).
#if CFG_TUD_AUDIO
// Fixed-function device: the clock never actually changes rate (48kHz is
// hardwired in both the descriptor and the Daisy audio callback), so the
// clock GET/SET handlers below exist purely so hosts that politely probe
// "what rate are you at, can I change it" get a sane, spec-legal answer
// instead of a stalled control transfer. Real gain/mute values are tracked
// but --- as noted in the Feature Unit descriptor comment above --- are not
// yet wired into the audio path.

static bool     fu_mute[3];       // [0]=master, [1]=L, [2]=R
static int16_t  fu_volume_db256[3];  // 1/256 dB units, UAC2 convention

#define VOLUME_CTRL_0DB    0
#define VOLUME_CTRL_MIN_DB (-96 * 256)

static bool audio_clock_get_request(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);

  if (ctrl_sel == AUDIO20_CS_CTRL_SAM_FREQ) {
    if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_4_t cur = {.bCur = (int32_t) AUDIO_SAMPLE_RATE};
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur, sizeof(cur));
    } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
      // Single fixed point: min == max == 48000, no step. Firmware never
      // negotiates rate (see design spec section 2).
      audio20_control_range_4_n_t(1) range = {.wNumSubRanges = tu_htole16(1)};
      range.subrange[0].bMin = (int32_t) AUDIO_SAMPLE_RATE;
      range.subrange[0].bMax = (int32_t) AUDIO_SAMPLE_RATE;
      range.subrange[0].bRes = 0;
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &range, sizeof(range));
    }
  } else if (ctrl_sel == AUDIO20_CS_CTRL_CLK_VALID && p_request->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t valid = {.bCur = 1};  // always locked: internal fixed clock
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &valid, sizeof(valid));
  }
  return false;
}

static bool audio_clock_set_request(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t const *buf) {
  (void) rhport;
  (void) buf;
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);
  // Politely accept a SET_CUR on sample rate (some hosts always send one on
  // stream open) but the rate never actually changes -- there is nothing
  // else to do since the descriptor only ever advertises one rate.
  return (p_request->bRequest == AUDIO20_CS_REQ_CUR && ctrl_sel == AUDIO20_CS_CTRL_SAM_FREQ);
}

static bool audio_feature_unit_get_request(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);
  uint8_t const ch       = TU_U16_LOW(p_request->wValue);
  if (ch >= TU_ARRAY_SIZE(fu_mute)) return false;

  if (ctrl_sel == AUDIO20_FU_CTRL_MUTE && p_request->bRequest == AUDIO20_CS_REQ_CUR) {
    audio20_control_cur_1_t cur = {.bCur = fu_mute[ch]};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur, sizeof(cur));
  } else if (ctrl_sel == AUDIO20_FU_CTRL_VOLUME) {
    if (p_request->bRequest == AUDIO20_CS_REQ_CUR) {
      audio20_control_cur_2_t cur = {.bCur = tu_htole16(fu_volume_db256[ch])};
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur, sizeof(cur));
    } else if (p_request->bRequest == AUDIO20_CS_REQ_RANGE) {
      audio20_control_range_2_n_t(1) range = {.wNumSubRanges = tu_htole16(1)};
      range.subrange[0].bMin = tu_htole16(VOLUME_CTRL_MIN_DB);
      range.subrange[0].bMax = tu_htole16(VOLUME_CTRL_0DB);
      range.subrange[0].bRes = tu_htole16(256);  // 1 dB steps
      return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &range, sizeof(range));
    }
  }
  return false;
}

static bool audio_feature_unit_set_request(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t const *buf) {
  (void) rhport;
  uint8_t const ctrl_sel = TU_U16_HIGH(p_request->wValue);
  uint8_t const ch       = TU_U16_LOW(p_request->wValue);
  if (ch >= TU_ARRAY_SIZE(fu_mute) || p_request->bRequest != AUDIO20_CS_REQ_CUR) return false;

  if (ctrl_sel == AUDIO20_FU_CTRL_MUTE) {
    TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_1_t));
    fu_mute[ch] = ((audio20_control_cur_1_t const *) buf)->bCur;
    return true;
  } else if (ctrl_sel == AUDIO20_FU_CTRL_VOLUME) {
    TU_VERIFY(p_request->wLength == sizeof(audio20_control_cur_2_t));
    fu_volume_db256[ch] = ((audio20_control_cur_2_t const *) buf)->bCur;
    return true;
  }
  return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  uint8_t const entity_id = TU_U16_HIGH(p_request->wIndex);
  if (entity_id == UAC2_ENTITY_CLOCK) return audio_clock_get_request(rhport, p_request);
  if (entity_id == UAC2_ENTITY_FEATURE_UNIT) return audio_feature_unit_get_request(rhport, p_request);
  return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
  uint8_t const entity_id = TU_U16_HIGH(p_request->wIndex);
  if (entity_id == UAC2_ENTITY_CLOCK) return audio_clock_set_request(rhport, p_request, buf);
  if (entity_id == UAC2_ENTITY_FEATURE_UNIT) return audio_feature_unit_set_request(rhport, p_request, buf);
  return false;
}

// Invoked when the host selects an AS alternate setting. alt 1 == host has
// opened the capture stream (matches PipeWire/ALSA opening the capture
// device); alt 0 == host closed it. main.c's usb_audio_stream_* module
// reads this via usb_audio_stream_is_active() to decide whether to bother
// packing/pushing samples at all (no reason to burn cycles filling a FIFO
// nobody's draining).
static volatile bool s_audio_streaming_active = false;

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  uint8_t const itf = tu_u16_low(p_request->wIndex);
  uint8_t const alt = tu_u16_low(p_request->wValue);

  if (itf == ITF_NUM_AUDIO_STREAMING) {
    s_audio_streaming_active = (alt != 0);
    if (alt == 0) {
      tud_audio_clear_ep_in_ff();  // drop any stale audio on stream close
    }
  }
  return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
  (void) rhport;
  uint8_t const itf = tu_u16_low(p_request->wIndex);
  if (itf == ITF_NUM_AUDIO_STREAMING) {
    s_audio_streaming_active = false;
  }
  return true;
}

bool usb_audio_stream_is_active(void) {
  return s_audio_streaming_active;
}
#endif  // CFG_TUD_AUDIO
