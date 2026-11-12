/// pmor.core~ : the PMOR engines (pmor_core) as a Max signal object.
///
/// One core per Max process (pmor_core is global state, as on the firmware). Any number of pmor.core~
/// objects (e.g. one per lane device) share it: the core renders once, every instance reads the same
/// stereo output from a shared ring at its own cursor. The core runs at 48 kHz; at any other Max sample
/// rate each instance resamples (linear) from the ring.
///
/// Messages:  param <id> <value>   set a schema parameter (clamped; applied on the audio thread)
///            midi <status> <d1> <d2>  PMOR's MIDI (PROTOCOL section 7): drums/synth/sampler notes, sensors, seed CC110..113, fx engage
///            drum <pad> <vel>, smp <slot> <vel>, fx <id> <0|1>   direct triggers
///            slot <n> [<path> [<crc32hex|-> [<rate> [<ch>]]]]  load a cached PMOR sample (.aud int16 mono 48k) into sampler slot n (no path or a bad file = empty slot); file read on the message thread
///            reseed <S>           pmorReseed(S), 28-bit seed, 0 = original fixed seeds (PROTOCOL section 7)
/// Outlets:   2 signal (L, R).

#include "c74_min.h"
#include <atomic>
#include <string.h>

#include "config.h"
#include "shared_state.h"
#include "clock.h"
#include "sequencer.h"
#include "scale.h"
#include "synth.h"
#include "drums.h"
#include "sampler.h"
#include "fx.h"
#include "output.h"
#include "metro.h"
#include "params.h"
#include "render.h"
#include "pmor_share.h"
#include "pmor_notepad.h"
#include "pmor_hostctl.h"
#include "pmor_slotload.h"
#include <math.h>

using namespace c74::min;

namespace {

constexpr size_t PQ = 256;                  // param queue
using ParamEv = pmorhc::Ev;

struct SharedCore {
	std::atomic<int>      users {0};
	bool                  booted = false;
	pmorshare::Ring       ring;
	// control -> audio hand-off (message thread produces, whoever renders consumes under the ring lock)
	ParamEv               pq[PQ];
	std::atomic<size_t>   pw {0}, pr {0};
	std::atomic<int64_t>  reseed {-1};
	pmorhc::State         hc;                          // render thread only: ch2 pitch bend + note <-> pad voice tags (max/pmor_hostctl.h)
	pmorslot::Mailbox     slots;                       // sampler slot loads: decoded on the message thread, copied in at the top of the next block


	void boot() {                                      // same order as hw/boot.cpp and tests/scenario.cpp
		hc.reset();
		memset(&g, 0, sizeof(g));
		clockInit(120.f);
		seqInit(); scaleInit(); synthInit(); drumsInit(); samplerInit(); fxInit(); outputInit(); metroInit();
		g_render_notes_only = true;          // EMGOR: generation happens ONLY on PMOR; Live plays the notes PMOR sends
		pmorSchemaInit();
		booted = true;
	}
	void apply(const ParamEv& e) { pmorhc::apply(hc, e); }
	void renderBlock(float* l, float* rr, size_t n) {   // called by Reader::pull with the ring lock held
		int64_t rs = reseed.exchange(-1, std::memory_order_acq_rel);
		if (rs >= 0) pmorReseed((uint32_t)rs);
		slots.apply();
		size_t r = pr.load(std::memory_order_relaxed); const size_t w = pw.load(std::memory_order_acquire);
		while (r != w) { apply(pq[r]); r = (r + 1) % PQ; }
		pr.store(r, std::memory_order_release);
		float in0[BLOCK] = {0};
		metro.level = 0.f;                       // EMGOR: PMOR's record metronome never sounds through the Live device (Live has its own); PMOR keeps its METRO value
		pmorRenderBlock(in0, l, rr, n);
	}
};

SharedCore& core() { static SharedCore c; return c; }

}  // namespace

class pmor_core_tilde : public object<pmor_core_tilde>, public vector_operator<> {
public:
	MIN_DESCRIPTION {"PMOR synth/drums/sampler/FX core (shared, any sample rate)"};
	MIN_TAGS {"audio, synth"};
	MIN_AUTHOR {"EMGOR"};
	MIN_RELATED {"pmor.proto"};

	inlet<>  in {this, "(message) param <id> <value>, reseed <S>"};
	outlet<> out_l {this, "(signal) left", "signal"};
	outlet<> out_r {this, "(signal) right", "signal"};

	pmor_core_tilde(const atoms& = {}) {
		SharedCore& c = core();
		c.ring.acquire();
		if (c.users.fetch_add(1) == 0 && !c.booted) c.boot();
		m_rd.join(c.ring);                             // join at the live head
		c.ring.release();
	}
	~pmor_core_tilde() override { core().users.fetch_sub(1); }

	bool queue(uint8_t t, uint16_t id, float v) {
		SharedCore& c = core();
		size_t w = c.pw.load(std::memory_order_relaxed), r = c.pr.load(std::memory_order_acquire);
		if (((w + 1) % PQ) == r) { cerr << "pmor.core~: event queue full" << endl; return false; }
		c.pq[w] = { t, id, v }; c.pw.store((w + 1) % PQ, std::memory_order_release); return true;
	}

	message<> param {this, "param", "param <id> <value>: set a schema parameter (clamped)",
		MIN_FUNCTION {
			if (args.size() < 2) { cerr << "pmor.core~: param <id> <value>" << endl; return {}; }
			queue(pmorhc::HE_PARAM, (uint16_t)(int)args[0], (float)(double)args[1]);
			return {};
		}
	};

	// Raw bytes of what PMOR sends over USB-MIDI (PROTOCOL section 7), so recorded takes play back through the core.
	// Notes: ch1 drums 36+pad, ch3 sampler 36+slot, ch2 synth (exact pitch incl. ch2 bend, any octave), ch4 CC1..6 sensors + CC110..113 seed, ch5 CC = fx id engage,
	// ch6 FX / perform state (max/pmor_hostctl.h has the CC map). Other channels are ignored.
	message<> midi {this, "midi", "midi <status> <data1> <data2>: a MIDI message from PMOR",
		MIN_FUNCTION {
			if (args.size() < 2) return {};
			const int st = (int)args[0], d1 = (int)args[1], d2 = args.size() > 2 ? (int)args[2] : 0;
			pmorhc::Ev e;
			if (pmorhc::decode(st, d1, d2, e)) queue(e.t, e.id, e.v);
			else if ((st & 0xF0) == 0xB0 && (st & 0x0F) == 3 && d1 >= 110 && d1 <= 113) {     // ch4 seed CCs: 28-bit seed on the last one
				m_seed[d1 - 110] = (uint32_t)d2;
				if (d1 == 113) core().reseed.store((int64_t)(((m_seed[0] << 21) | (m_seed[1] << 14) | (m_seed[2] << 7) | m_seed[3]) & 0x0FFFFFFFu), std::memory_order_release);
			}
			return {};
		}
	};

	// direct triggers (same events, no MIDI framing): drum <pad> <vel>, smp <slot> <vel>, fx <id> <0|1>
	message<> drum {this, "drum", MIN_FUNCTION { if (args.size() >= 2) queue(pmorhc::HE_DRUM, (uint16_t)(int)args[0], (float)(double)args[1]); return {}; }};
	message<> smp {this, "smp", MIN_FUNCTION { if (args.size() >= 2) queue(pmorhc::HE_SMP, (uint16_t)(int)args[0], (float)(double)args[1]); return {}; }};
	message<> fx {this, "fx", MIN_FUNCTION { if (args.size() >= 2) queue(((int)args[1]) ? pmorhc::HE_FX_ON : pmorhc::HE_FX_OFF, (uint16_t)(int)args[0], 0.f); return {}; }};

	message<> slot {this, "slot", "slot <n> [<path> [<crc32hex|-> [<rate> [<ch>]]]]: load a cached sample into sampler slot n (empty/invalid = silent slot)",
		MIN_FUNCTION {
			if (args.empty()) { cerr << "pmor.core~: slot <n> [<path> [<crc32hex|-> [<rate> [<ch>]]]]" << endl; return {}; }
			std::string path = args.size() > 1 ? std::string(symbol(args[1])) : std::string();
			std::string cs = args.size() > 2 ? std::string(symbol(args[2])) : std::string("-"); bool hc = cs != "-"; uint32_t crc = hc ? (uint32_t)strtoul(cs.c_str(), nullptr, 16) : 0;
			uint32_t rate = args.size() > 3 ? (uint32_t)(int)args[3] : 48000, chn = args.size() > 4 ? (uint32_t)(int)args[4] : 1;
			auto r = core().slots.post((int)args[0], path, hc, crc, rate, chn);
			if (r != pmorslot::OK) cerr << "pmor.core~: slot " << (int)args[0] << " sample not loaded (" << path << ") code " << (int)r << endl;
			return {};
		}
	};

	message<> reseed {this, "reseed", "reseed <S>: restart every random stream from a 28-bit seed (0 = original seeds); applied at the next core block",
		MIN_FUNCTION {
			if (args.empty()) { cerr << "pmor.core~: reseed <S>" << endl; return {}; }
			core().reseed.store((int64_t)((uint32_t)(double)args[0] & 0x0FFFFFFFu), std::memory_order_release);
			return {};
		}
	};

	message<> dspsetup {this, "dspsetup",
		MIN_FUNCTION {
			m_rd.setRate(samplerate());                // resample 48 kHz core -> host rate
			SharedCore& c = core(); c.ring.acquire(); m_rd.join(c.ring); c.ring.release();
			return {};
		}
	};

	void operator()(audio_bundle input, audio_bundle output) {
		(void)input;
		const size_t n = output.frame_count();
		double* L = output.samples(0);
		double* R = output.samples(1);
		SharedCore& c = core();
		m_rd.pull(c.ring, [&c](float* l, float* r, size_t k) { c.renderBlock(l, r, k); }, L, R, n);
	}

private:
	pmorshare::Reader m_rd;
	uint32_t m_seed[4] {0, 0, 0, 0};
};

MIN_EXTERNAL(pmor_core_tilde);
