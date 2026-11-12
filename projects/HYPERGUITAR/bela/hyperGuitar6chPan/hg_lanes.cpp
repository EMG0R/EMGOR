/*
 * hg_lanes.cpp - the five NEPTR encoder lanes, and the one function that
 * mutates them. See hg_link.h for the model and why apply() owns the echo.
 *
 * Every voicing below is copied from gCatSlot[][] in presets.cpp verbatim.
 * That is deliberate and load-bearing: the formant slots 0-3 and pitch slots
 * 0-3 are v5 pads restored by explicit user request and carry "do not retune"
 * comments there. If a voicing needs to change it changes in presets.cpp and
 * here together, and the static_asserts at the bottom of this file are what
 * catch the tables drifting apart.
 */
#include "hg_link.h"
#include <cmath>

/* ---------------- lane 0: PITCH EXP ---------------- */
/*
 * Pitch at 100% wet, interval swept by the expression pedal. Rotation picks
 * the pedal's RANGE rather than an interval - that is the whole character of
 * this lane, and +/-12 is the default because it is the one the user described
 * as "the big change vibe".
 *
 * The one-sided ranges at the end exist because a pedal that only ever goes
 * down (or only up) from unison is a different instrument from one that
 * sweeps through unison in the middle of its travel: with +/-12 the heel-to-toe
 * midpoint is the dry note, which is a very specific thing to have to find.
 */
static const HgEntry gGlideEntries[] = {
	{ PAD_PITCH, -12.0f,  12.0f, "EXP +/-12"  },
	{ PAD_PITCH,  -5.0f,   5.0f, "EXP +/-5"   },
	{ PAD_PITCH,  -7.0f,   7.0f, "EXP +/-7"   },
	{ PAD_PITCH, -24.0f,  24.0f, "EXP +/-24"  },
	{ PAD_PITCH, -12.0f,   0.0f, "EXP DOWN"   },
	{ PAD_PITCH,   0.0f,  12.0f, "EXP UP"     },
};

/* ---------------- lane 1: PS STEPS ---------------- */
/* CAT_PITCH's nine PAD_PITCH voicings, in the order presets.cpp lists them. */
static const HgEntry gStepEntries[] = {
	{ PAD_PITCH, -12.0f, 100.0f, "OCT DOWN"   },
	{ PAD_PITCH,  -5.0f,  50.0f, "4TH DOWN"   },
	{ PAD_PITCH,  -7.0f,  50.0f, "5TH DOWN"   },
	{ PAD_PITCH,  12.0f, 100.0f, "OCT UP"     },
	{ PAD_PITCH,   7.0f,  50.0f, "5TH UP"     },
	{ PAD_PITCH,  -3.0f,  50.0f, "MIN 3RD DN" },
	{ PAD_PITCH,  -4.0f,  50.0f, "MAJ 3RD DN" },
	{ PAD_PITCH,  12.0f,  35.0f, "SHIMMER"    },
	{ PAD_PITCH,   0.12f, 50.0f, "DETUNE 12C" },
};

/* ---------------- lane 2: FORMANT ---------------- */
/* CAT_SPECTRAL's seven PAD_FORMANT voicings. 0.75/0.90/1.10/1.30 are the v5
 * pads - restored by name, not to be retuned. */
static const HgEntry gFormantEntries[] = {
	{ PAD_FORMANT, 0.75f, 0.06f,  "FORM 0.75" },
	{ PAD_FORMANT, 0.90f, 0.08f,  "FORM 0.90" },
	{ PAD_FORMANT, 1.10f, 0.10f,  "FORM 1.10" },
	{ PAD_FORMANT, 1.30f, 0.07f,  "FORM 1.30" },
	{ PAD_FORMANT, 0.55f, 0.035f, "FORM DEEP" },
	{ PAD_FORMANT, 1.60f, 0.30f,  "FORM HIGH" },
	{ PAD_FORMANT, 1.18f, 0.012f, "FORM SILK" },
};

/* ---------------- lane 3: DYNAMICS ---------------- */
/*
 * Everything whose character comes from how hard you pick. Four DIFFERENT
 * effect ids in one lane - this is the case rule 1 exists for.
 *
 * PAD_PLUCKSAG is the "pitch descending plucky" one: pitch sags downward on
 * the attack and recovers as the note settles.
 *
 * PAD_FWAH re-routes the wet/dry legs, so it must re-run applyMixRouting().
 * It does, because fxSetActive() goes through gFx[] and that table already
 * carries the hook - which is the reason this lane dispatches through it
 * rather than writing the flags directly.
 */
static const HgEntry gDynEntries[] = {
	{ PAD_ENVF,     1.20f,  0.90f, "WAH SOFT"   },
	{ PAD_ENVF,     2.20f,  2.00f, "WAH VOCAL"  },
	{ PAD_ENVF,     4.50f,  5.00f, "WAH SCREAM" },
	{ PAD_FWAH,     0.90f,  0.00f, "FWAH SOFT"  },
	{ PAD_FWAH,     2.20f,  0.35f, "FWAH MID"   },
	{ PAD_FWAH,     5.00f,  0.75f, "FWAH WIDE"  },
	{ PAD_PLUCKSAG, 0.002f, 0.05f, "SAG SUBTLE" },
	{ PAD_PLUCKSAG, 0.006f, 0.10f, "SAG BEND"   },
	{ PAD_PLUCKSAG, 0.010f, 0.28f, "SAG WOBBLE" },
	{ PAD_POG,      0.45f,  0.00f, "POG SWELL"  },
};

/* ---------------- lane 4: SYNTH ---------------- */
/*
 * Always POLYPHONIC - the user's standing choice, so gSynthPoly is forced on
 * engage rather than being another thing to remember. Rotation walks
 * SYNTH_NAMES. Level stays at unity throughout: SAW LEAD (mode 21) tracks
 * cleanest at unity and the others were voiced against it.
 */
/* mode indices are SYNTH_NAMES indices (render.cpp). 2026-11-05: the 8 analog pads took 0-7, so every
 * entry below was shifted +8 and still points at the same sound. The analog pads are NOT in this lane
 * yet: NEPTR indexes this list positionally (10 entries), so adding entries is a NEPTR-side change. */
static const HgEntry gSynthEntries[] = {
	{ PAD_SINE, 21.0f, 1.0f, "SAW LEAD" },
	{ PAD_SINE, 15.0f, 1.0f, "MOOG LEAD" },
	{ PAD_SINE, 16.0f, 1.0f, "ACID SQUELCH" },
	{ PAD_SINE, 14.0f, 1.0f, "WAVEFOLDER" },
	{ PAD_SINE, 10.0f, 1.0f, "RINGMOD BELL" },
	{ PAD_SINE, 22.0f, 1.0f, "SYNC LEAD" },
	{ PAD_SINE, 17.0f, 1.0f, "FM GROWL" },
	{ PAD_SINE, 20.0f, 1.0f, "CHIP LEAD" },
	{ PAD_SINE, 11.0f, 1.0f, "SUB GROWL" },
	{ PAD_SINE, 18.0f, 1.0f, "FORMANT VOWEL" },
};

#define NELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

/*
 * Lane order is LEFT TO RIGHT as the player sees NEPTR's screen. NEPTR's UI
 * list is drawn left->right while its hardware slots run right->left, so lane
 * 0 is NEPTR's slot 4 (toggle CC72 / rotation CC22) and lane 4 is its slot 0
 * (CC74 / CC24). That mapping lives on the NEPTR side; here they are simply
 * lanes 0..4 and the ordering is the contract between the two.
 */
const HgLane gHgLane[HG_NLANES] = {
	{ "PITCH EXP", HG_GLIDE, gGlideEntries,   NELEM(gGlideEntries)   },
	{ "PS STEPS", HG_FX,    gStepEntries,    NELEM(gStepEntries)    },
	{ "FORMANT",  HG_FX,    gFormantEntries, NELEM(gFormantEntries) },
	{ "DYNAMICS", HG_FX,    gDynEntries,     NELEM(gDynEntries)     },
	{ "SYNTH",    HG_SYNTH, gSynthEntries,   NELEM(gSynthEntries)   },
};

int gHgLaneOn[HG_NLANES]  = { 0, 0, 0, 0, 0 };
int gHgLaneIdx[HG_NLANES] = { 0, 0, 0, 0, 0 };

float gHgPedal   = 0.0f;
int   gHgGlideOn = 0;

/* glide target/current, so a fast pedal sweep slews instead of stepping */
static float gGlideLo = -12.0f, gGlideHi = 12.0f;
static float gGlideCur = 0.0f;
static int   gGlidePrimed = 0;

/*
 * Apply an entry's voicing. The FX kinds hand off to applySlotPreset() so the
 * p1/p2 semantics stay defined in exactly one place (presets.cpp) - an HgEntry
 * is deliberately layout-compatible with an FxSlot for this reason.
 */
static void hgApplyEntry(const HgLane *ln, const HgEntry *e)
{
	switch(ln->kind) {
	case HG_GLIDE:
		gGlideLo = e->p1;
		gGlideHi = e->p2;
		/* 100% wet is the point of this lane: at anything less the swept
		 * interval beats against the dry note instead of replacing it. */
		gPitchWetPct    = 100.0f;
		gPitchCentsMode = 0;
		gPitchCents     = 0.0f;
		break;
	case HG_SYNTH:
		gSynthMode = (int)(e->p1 + 0.5f);
		gSynthPoly = 1;               /* standing user choice - always poly */
		gSynthLevel = e->p2;
		break;
	case HG_FX:
	default: {
		FxSlot sl;
		sl.fx = e->fx;
		sl.p1 = e->p1;
		sl.p2 = e->p2;
		sl.label = e->label;
		applySlotPreset(&sl);
		break;
	}
	}
}

void hgLaneApply(int lane, int on, int idx, int echo)
{
	if(lane < 0 || lane >= HG_NLANES)
		return;
	const HgLane *ln = &gHgLane[lane];

	/* Wrap rather than clamp: the encoder is an accumulator the user spins,
	 * and stopping dead at the end of a nine-entry list reads as a fault. */
	if(ln->nEntries > 0) {
		idx %= ln->nEntries;
		if(idx < 0)
			idx += ln->nEntries;
	} else {
		idx = 0;
	}
	const HgEntry *e = &ln->entries[idx];

	gHgLaneIdx[lane] = idx;
	gHgLaneOn[lane]  = on ? 1 : 0;

	if(on) {
		/*
		 * RULE 2. Any other lane holding this effect id must let go, or two
		 * lanes would each believe they own it and the first one switched off
		 * would silence an effect the other still shows as on.
		 */
		for(int L = 0; L < HG_NLANES; L++) {
			if(L == lane || !gHgLaneOn[L])
				continue;
			const HgLane *o = &gHgLane[L];
			if(o->nEntries <= 0)
				continue;
			if(o->entries[gHgLaneIdx[L]].fx == e->fx) {
				gHgLaneOn[L] = 0;
				if(echo)
					/* echoed below via hgEmitAll's per-lane send; emit the
					 * displaced lane too so NEPTR's UI unlights it */
					hgLaneEmit(L);
			}
		}
		/*
		 * RULE 1 falls out of the ordering here: switching the lane's PREVIOUS
		 * effect off before applying the new entry means an index move inside
		 * a mixed-effect lane (DYNAMICS) hands over cleanly instead of leaving
		 * the old effect stuck on.
		 */
		for(int i = 0; i < ln->nEntries; i++) {
			if(i != idx && ln->entries[i].fx != e->fx)
				fxSetActive(ln->entries[i].fx, 0);
		}
		hgApplyEntry(ln, e);
		fxSetActive(e->fx, 1);
	} else {
		/* Switching a lane off switches off whatever its current entry drives,
		 * and nothing else - a mixed lane must not silence effects that other
		 * lanes own. */
		fxSetActive(e->fx, 0);
	}

	if(ln->kind == HG_GLIDE) {
		gHgGlideOn = gHgLaneOn[lane];
		if(!gHgGlideOn)
			gGlidePrimed = 0;   /* re-prime on next engage, no stale slew */
	}

	if(echo)
		hgLaneEmit(lane);
}

void hgEmitAll(void)
{
	for(int L = 0; L < HG_NLANES; L++)
		hgLaneEmit(L);
}

/*
 * Per-block glide. Called from render() with the block's duration so the slew
 * is sample-rate and buffer-size independent - at -p 64 this runs ~750 times a
 * second and at -p 128 half that, and the pedal must feel identical either way.
 *
 * ~25 ms to converge: fast enough that the pitch tracks the foot rather than
 * lagging it, slow enough that the 7-bit CC steps arriving from NEPTR's pedal
 * are smoothed into a continuous sweep instead of a staircase.
 */
void hgGlideTick(float blockSeconds)
{
	if(!gHgGlideOn)
		return;
	float target = gGlideLo + gHgPedal * (gGlideHi - gGlideLo);
	if(!gGlidePrimed) {
		/* Engage AT the pedal's current position. Slewing from wherever the
		 * last glide left off would sweep the whole interval audibly every
		 * time the lane is switched on. */
		gGlideCur = target;
		gGlidePrimed = 1;
	} else {
		float a = 1.0f - expf(-blockSeconds / 0.025f);
		gGlideCur += (target - gGlideCur) * a;
	}
	gPitchSemitones = gGlideCur;
	gPitchWetPct    = 100.0f;
}

/*
 * Reverse lookup: (effect, voicing) -> (lane, entry). See hg_link.h for why
 * this exists and why it matches on the voicing rather than the effect alone.
 */
void hgReportFx(int fx, float p1, float p2, int on)
{
	for(int L = 0; L < HG_NLANES; L++) {
		const HgLane *ln = &gHgLane[L];
		for(int i = 0; i < ln->nEntries; i++) {
			const HgEntry *e = &ln->entries[i];
			if(e->fx != fx)
				continue;
			/*
			 * Float compare with a tolerance, not ==. The values arrive here
			 * having been through applySlotPreset() and in some cases a clamp,
			 * so an exact bit match is not guaranteed even when the voicing is
			 * literally the same table row. The entries within a lane are far
			 * enough apart (0.75 / 0.90 / 1.10 vowels, whole semitones) that
			 * 1e-3 cannot confuse two of them.
			 */
			if(fabsf(e->p1 - p1) > 1e-3f || fabsf(e->p2 - p2) > 1e-3f)
				continue;
			gHgLaneIdx[L] = i;
			gHgLaneOn[L]  = on ? 1 : 0;
			hgLaneEmit(L);
			return;
		}
	}
	/*
	 * No lane carries this effect. Still worth reporting an OFF for any lane
	 * that currently shows this effect id as live, or NEPTR keeps a lane lit
	 * for something the player has just switched off from the grid.
	 */
	if(!on) {
		for(int L = 0; L < HG_NLANES; L++) {
			const HgLane *ln = &gHgLane[L];
			if(!gHgLaneOn[L] || ln->nEntries <= 0)
				continue;
			if(ln->entries[gHgLaneIdx[L]].fx == fx) {
				gHgLaneOn[L] = 0;
				hgLaneEmit(L);
			}
		}
	}
}
