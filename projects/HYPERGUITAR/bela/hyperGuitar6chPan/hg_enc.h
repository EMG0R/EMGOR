/*
 * hg_enc.h - 2026-11-01 enc: header-only quadrature decoder (host-testable).
 *
 * 4-state Gray-code transition table, +-1 per valid edge, 0 on no-change,
 * 0 + invalid-count on a skipped state (bounce / missed edge). No time-based
 * lockout. Edges accumulate; one step is emitted per ENC_EDGES_PER_DETENT
 * edges and the remainder is KEPT. A settle rule (no edge for the settle
 * window) rounds a leftover >= half a detent to one step and drops a smaller
 * leftover, so a lost edge cannot leave the accumulator stranded 1-3 edges
 * short ("takes a few turns").
 */
#ifndef HG_ENC_H
#define HG_ENC_H

#ifndef ENC_EDGES_PER_DETENT
#define ENC_EDGES_PER_DETENT 4   /* == the former ENC_DETENT; KY-040 style */
#endif
/* Flip to -1 if rotation direction reads backwards after this change. */
#ifndef ENC_DIR
#define ENC_DIR 1
#endif

struct HgEnc {
	int st;        /* last (A<<1)|B, -1 = not primed */
	int acc;       /* signed edge remainder */
	int idle;      /* frames since last valid edge */
	unsigned invalid;
	unsigned steps;
	HgEnc() : st(-1), acc(0), idle(0), invalid(0), steps(0) {}
};

/* old<<2|new -> -1/0/+1 ; +1 = 00>10>11>01>00 (A leads B) */
static const signed char HG_ENC_TAB[16] = {
	/*old00*/  0, -1, +1,  2,
	/*old01*/ +1,  0,  2, -1,
	/*old10*/ -1,  2,  0, +1,
	/*old11*/  2, +1, -1,  0
};   /* 2 = invalid (two bits changed) */

/* One sample. Returns whole UI steps completed this frame (-1/0/+1, sign
 * already multiplied by ENC_DIR). settleFrames: idle window for the settle rule. */
static inline int hgEncFeed(HgEnc &e, int a, int b, int settleFrames)
{
	int ns = ((a ? 1 : 0) << 1) | (b ? 1 : 0);
	int out = 0;
	if(e.st < 0) { e.st = ns; return 0; }
	if(ns != e.st) {
		int t = HG_ENC_TAB[(e.st << 2) | ns];
		e.st = ns;
		if(t == 2) { e.invalid++; }
		else {
			e.acc += t * ENC_DIR;
			e.idle = 0;
			while(e.acc >= ENC_EDGES_PER_DETENT)  { e.acc -= ENC_EDGES_PER_DETENT; out++; }
			while(e.acc <= -ENC_EDGES_PER_DETENT) { e.acc += ENC_EDGES_PER_DETENT; out--; }
		}
	} else if(e.acc != 0 && ++e.idle >= settleFrames) {
		int h = ENC_EDGES_PER_DETENT / 2;
		if(e.acc >= h) out++; else if(e.acc <= -h) out--;
		e.acc = 0; e.idle = 0;
	}
	e.steps += (unsigned)(out < 0 ? -out : out);
	return out;
}
#endif
