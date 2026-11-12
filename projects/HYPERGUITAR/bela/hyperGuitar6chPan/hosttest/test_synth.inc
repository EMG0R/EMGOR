/*
 * test_synth.inc - synth dynamics / sustain / ghost + double-trigger tests.
 * #included by test_main.cpp (after synth.cpp, so the file-static engine
 * state can be reset). Drives all six real voices (onset detector, tracker,
 * shifter, synth) with synthetic decaying-harmonic plucks, exactly the
 * per-sample cadence render.cpp uses, and reads v->pAmp (the synth amp).
 */
struct Pluck { int s; float t0, amp, dbps; };
struct Mute  { int s; float t0; };           /* string -30 dB in 20 ms at t0 */
struct SynEv { int s; float tSec; int code, why, dtN; float lvl, oth; };
struct RigOut {
	std::vector<float> amp[NUM_STRINGS];
	std::vector<SynEv> ev;
	int nOns[NUM_STRINGS];
	RigOut() { for(int i = 0; i < NUM_STRINGS; i++) nOns[i] = 0; }
	int accepted(int s) const { int n = 0; for(size_t i = 0; i < ev.size(); i++) if(ev[i].s == s && ev[i].code == SHDIAG_SYN_ON && ev[i].why == SYN_W_ACCEPTED) n++; return n; }
	int why(int s, int w) const { int n = 0; for(size_t i = 0; i < ev.size(); i++) if(ev[i].s == s && ev[i].code == SHDIAG_SYN_ON && ev[i].why == w) n++; return n; }
	int offs(int s) const { int n = 0; for(size_t i = 0; i < ev.size(); i++) if(ev[i].s == s && ev[i].code == SHDIAG_SYN_OFF) n++; return n; }
	float firstOn(int s) const { for(size_t i = 0; i < ev.size(); i++) if(ev[i].s == s && ev[i].code == SHDIAG_SYN_ON && ev[i].why == SYN_W_ACCEPTED) return ev[i].tSec; return -1.0f; }
};

static void genString(std::vector<float> &o, int s, float seconds, const std::vector<Pluck> &pl,
		const std::vector<Mute> &mu, float hz)
{
	int n = (int)(seconds * SR);
	o.assign(n, 0.0f);
	for(size_t k = 0; k < pl.size(); k++) {
		if(pl[k].s != s) continue;
		int i0 = (int)(pl[k].t0 * SR);
		float kdec = pl[k].dbps / 8.685889f;                 /* dB/s -> nepers/s */
		/* a re-pick of the same string stops the previous vibration (2 ms) */
		int iEnd = n;
		for(size_t j = 0; j < pl.size(); j++)
			if(pl[j].s == s && pl[j].t0 > pl[k].t0 && (int)(pl[j].t0 * SR) < iEnd) iEnd = (int)(pl[j].t0 * SR);
		unsigned int rng = 12345u + (unsigned int)(s * 977 + k * 31);
		for(int i = i0; i < n; i++) {
			float t = (float)(i - i0) / SR;
			float e = pl[k].amp * expf(-kdec * t);
			if(e < 1e-6f) break;
			if(t < 0.002f) e *= t / 0.002f;
			if(i >= iEnd) { float f = 1.0f - (float)(i - iEnd) / (0.002f * SR); if(f <= 0.0f) break; e *= f; }
			float x = 0.0f;
			for(int h = 1; h <= 6; h++)
				x += sinf(2.0f * (float)M_PI * hz * (float)h * t) / (float)h;
			/* pick click: broadband noise burst, 3x the pluck, 1.5 ms decay */
			rng = rng * 1664525u + 1013904223u;
			float nz = ((float)(rng >> 8) / 8388608.0f - 1.0f);
			float click = t < 0.006f ? 3.0f * pl[k].amp * nz * expf(-t / 0.0015f) : 0.0f;
			o[i] += e * x + click;
		}
	}
	for(size_t k = 0; k < mu.size(); k++) {
		if(mu[k].s != s) continue;
		int i0 = (int)(mu[k].t0 * SR), ramp = (int)(0.020f * SR);
		float gend = powf(10.0f, -30.0f / 20.0f);
		for(int i = i0; i < n; i++) {
			float g = i - i0 < ramp ? powf(10.0f, -30.0f * (float)(i - i0) / (float)ramp / 20.0f) : gend;
			o[i] *= g;
		}
	}
}

static void synReset6(void)
{
	for(int s = 0; s < NUM_STRINGS; s++) { sStale[s] = 1; sSt[s] = SynStr(); gOnsetStamp[s] = 0; }
	memset(gBandLvl, 0, sizeof(gBandLvl));
	memset(gBandHist, 0, sizeof(gBandHist));
}

/* coupling: adjacent -23 dB, next-but-one -37 dB, further -43 dB */
static void runRig(float seconds, const std::vector<Pluck> &pl, const std::vector<Mute> &mu,
		float sustain, int bleed, int useOtherMax, RigOut &out, int forceStr = -1)
{
	int n = (int)(seconds * SR);
	std::vector<float> clean[NUM_STRINGS], in[NUM_STRINGS];
	for(int s = 0; s < NUM_STRINGS; s++) genString(clean[s], s, seconds, pl, mu, OPEN_HZ[s]);
	for(int s = 0; s < NUM_STRINGS; s++) {
		in[s] = clean[s];
		if(!bleed) continue;
		for(int o = 0; o < NUM_STRINGS; o++) {
			if(o == s) continue;
			int d = o > s ? o - s : s - o;
			float c = d == 1 ? 0.0708f : (d == 2 ? 0.0141f : 0.0071f);
			for(int i = 0; i < n; i++) in[s][i] += c * clean[o][i];
		}
	}
	resetGlobals();
	synReset6();
	gSampleStamp = 0;
	gSineAmtS = 1.0f; gSynthPoly = 1; gSynthMode = 1; gSynthGateThr = 0.010f; gSynthLevel = 1.0f;
	gSynSustain = sustain;
	gWetEngineActive = 1;
	for(int s = 0; s < NUM_STRINGS; s++) {
		voiceSetup(&gV[s], s);
		gV[s].hopCountdown = (s & 1) * 64 + (s >> 1) * 20 + 1;
		out.amp[s].assign(n, 0.0f);
	}
	out.ev.clear();
	for(int i = 0; i < NUM_STRINGS; i++) out.nOns[i] = 0;
	ShDiagEv e;
	while(shDiagPop(&e)) {}
	for(int i = 0; i < n; i++) {
		for(int s = 0; s < NUM_STRINGS; s++) {
			Voice *v = &gV[s];
			if(--v->hopCountdown <= 0) {
				trackPitch(v, s);
				updateModulation(v, s);
				updateLP(v);
				v->hopCountdown = HOP;
			}
			float wet, dry;
			voiceProcess(v, in[s][i], 1, &wet, &dry);
			if(s == forceStr) {      /* perfect-detector emulation for the string under test */
				for(size_t k = 0; k < pl.size(); k++)
					if(pl[k].s == s && i == (int)(pl[k].t0 * SR) + (int)(0.003f * SR) && v->sh.onsetRefract == 0) {
						shifterOnset(v, s);
						v->sh.onsetRefract = (int)(0.025f * v->srate);
					}
			}
			float mixed = 0.0f;
			runSynthPoly(v, s, &mixed);
			out.amp[s][i] = v->pAmp;
		}
		gSampleStamp++;
		if((i & 63) == 0 && useOtherMax) {
			for(int s = 0; s < NUM_STRINGS; s++) {
				float m = 0.0f;
				for(int o = 0; o < NUM_STRINGS; o++) if(o != s && gV[o].env > m) m = gV[o].env;
				gOtherMax[s] = m;
			}
		}
		if((i & 127) == 0) {
			while(shDiagPop(&e)) {
				if(e.code == SHDIAG_ONSET) out.nOns[e.s]++;
				if(e.code != SHDIAG_SYN_ON && e.code != SHDIAG_SYN_OFF) continue;
				SynEv x; x.s = e.s; x.tSec = (float)i / SR; x.code = e.code; x.why = e.i[0];
				x.dtN = e.i[1]; x.lvl = e.f[2]; x.oth = e.f[3];
				out.ev.push_back(x);
			}
		}
	}
	while(shDiagPop(&e)) {
		if(e.code != SHDIAG_SYN_ON && e.code != SHDIAG_SYN_OFF) continue;
		SynEv x; x.s = e.s; x.tSec = seconds; x.code = e.code; x.why = e.i[0];
		x.dtN = e.i[1]; x.lvl = e.f[2]; x.oth = e.f[3];
		out.ev.push_back(x);
	}
}

static float ampDb(float a) { return 20.0f * log10f(a > 1e-9f ? a : 1e-9f); }

/* ---------------- analog patch layer + shared filter (2026-11-05) ---------------- */
static void synRun(int mode, float f0, float lvl, float fmod, int n, std::vector<float> &out, int idx = 1,
		int stepAt = -1, float fmod2 = 0.0f)
{
	static AVoice av;
	memset(&av, 0, sizeof(av));
	float ph[6] = { 0, 0, 0, 0, 0, 0 }, filt[4] = { 0, 0, 0, 0 }, lph = 0.0f;
	float keep = gSynFmod;
	gSynFmod = fmod;
	out.assign(n, 0.0f);
	for(int i = 0; i < n; i++) {
		if(i == stepAt) gSynFmod = fmod2;
		lph += 0.35f / SR; if(lph >= 1.0f) lph -= 1.0f;
		out[i] = synthVoiceSample(av, mode, ph, filt, f0 / SR, sinf(2.0f * (float)M_PI * lph), SR, 1.0f, lvl, 0, idx);
	}
	gSynFmod = keep;
}
static void synOld(int oldMode, float f0, int n, std::vector<float> &out)
{
	float ph[6] = { 0, 0, 0, 0, 0, 0 }, filt[4] = { 0, 0, 0, 0 }, lph = 0.0f;
	out.assign(n, 0.0f);
	for(int i = 0; i < n; i++) {
		lph += 0.35f / SR; if(lph >= 1.0f) lph -= 1.0f;
		out[i] = synthOsc(oldMode, ph, f0 / SR, sinf(2.0f * (float)M_PI * lph), filt, SR, 1.0f);
	}
}
static double synRms(const std::vector<float> &y, int from)
{
	double e = 0; for(size_t i = from; i < y.size(); i++) e += (double)y[i] * y[i];
	return sqrt(e / (double)(y.size() - from));
}
static float synCentroid(const std::vector<float> &y, int from)
{
	const int N = 32768;
	std::vector<float> re(N, 0.0f), im(N, 0.0f);
	for(int k = 0; k < N && from + k < (int)y.size(); k++)
		re[k] = y[from + k] * (0.5f - 0.5f * cosf(2.0f * (float)M_PI * k / (float)(N - 1)));
	for(int i = 1, j = 0; i < N; i++) {
		int bit = N >> 1;
		for(; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if(i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
	}
	for(int len = 2; len <= N; len <<= 1) {
		float ang = -2.0f * (float)M_PI / (float)len;
		for(int i = 0; i < N; i += len)
			for(int k = 0; k < len / 2; k++) {
				float wr = cosf(ang * k), wi = sinf(ang * k);
				float xr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
				float xi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
				float ur = re[i + k], ui = im[i + k];
				re[i + k] = ur + xr; im[i + k] = ui + xi;
				re[i + k + len / 2] = ur - xr; im[i + k + len / 2] = ui - xi;
			}
	}
	double num = 0, den = 0;
	for(int k = 1; k < N / 2; k++) {
		double m = sqrt((double)re[k] * re[k] + (double)im[k] * im[k]);
		num += (double)k * SR / N * m; den += m;
	}
	return (float)(num / (den + 1e-12));
}

static void analogSynthTests(void)
{
	printf("\n--- analog synth patches + shared filter ---\n");
	const float F[2] = { 65.41f, 261.63f };      /* C2, C4 */
	const float L[2] = { 1.0f, 0.1f };
	/* (1) every patch: finite, non-silent, non-clipping at level 1 / 0.1 on C2 / C4 (default fmod 0.5) */
	{
		int bad = 0; float worstPk = 0, minRms = 1e9f; int worstM = -1;
		for(int m = 0; m < SYNTH_NMODES; m++)
			for(int fi = 0; fi < 2; fi++)
				for(int li = 0; li < 2; li++) {
					std::vector<float> y; synRun(m, F[fi], L[li], 0.5f, (int)(2.0f * SR), y);
					int from = (int)(1.0f * SR);
					float pk = 0; int fin = 1;
					for(size_t i = from; i < y.size(); i++) { if(!(y[i] == y[i])) fin = 0; if(fabsf(y[i]) > pk) pk = fabsf(y[i]); }
					/* final output = osc x pAmp(=level x AMP_MAX) x SINE_LEVEL (gSynthLevel 1) */
					float fin_pk = pk * L[li] * SYN_AMP_MAX * SINE_LEVEL;
					double r = synRms(y, from);
					if(!fin || r < 1e-3 || fin_pk >= 1.0f || (m < SYN_NNEW && pk > 1.5f)) { bad++; printf("   bad m%d f%.0f l%.1f fin%d rms %.5f pk %.2f\n", m, F[fi], L[li], fin, r, pk); }
					if(m < SYN_NNEW) { if(pk > worstPk) { worstPk = pk; worstM = m; } if((float)r < minRms) minRms = (float)r; }
				}
		CHECK("syn_h_all_patches_sane", bad == 0,
			"%d bad of %d (finite, rms>1e-3, no clip); new patches: worst osc peak %.2f (%s), min rms %.4f",
			bad, SYNTH_NMODES * 4, worstPk, worstM >= 0 ? SYNTH_NAMES[worstM] : "-", minRms);
	}
	/* (2) fmod 0: the 15 old patches are bit-identical to the old oscillator (RMS within 0.5 dB, here 0.00) */
	{
		float worst = 0; int worstM = -1;
		for(int om = 0; om < 15; om++)
			for(int fi = 0; fi < 2; fi++) {
				std::vector<float> a, b;
				synOld(om, F[fi], (int)(1.5f * SR), a);
				synRun(om + SYNTH_OLD_SHIFT, F[fi], 0.6f, 0.0f, (int)(1.5f * SR), b);
				float dB = fabsf(20.0f * log10f((float)(synRms(b, (int)(0.5f * SR)) / (synRms(a, (int)(0.5f * SR)) + 1e-12))));
				if(dB > worst) { worst = dB; worstM = om; }
			}
		CHECK("syn_i_fmod0_old_unchanged", worst < 0.5f, "worst |RMS change| %.3f dB over 15 old patches x C2/C4 (old %d)", worst, worstM);
	}
	/* (3) fmod > 0: raising the level raises the spectral centroid (all 8 new + bright old patches); never lowers it for any */
	{
		int fails = 0; float minRatio = 1e9f; int minM = -1; int anyDown = 0;
		for(int m = 0; m < SYNTH_NMODES; m++) {
			std::vector<float> lo, hi;
			synRun(m, 196.0f, 0.3f, 1.0f, (int)(2.0f * SR), lo);
			synRun(m, 196.0f, 1.0f, 1.0f, (int)(2.0f * SR), hi);
			float cl = synCentroid(lo, (int)(0.8f * SR)), ch = synCentroid(hi, (int)(0.8f * SR));
			int strict = (m < SYN_NNEW) || (m >= 8 + 11);   /* old SUPERSAW..SYNC LEAD carry real top end */
			if(ch < cl * 0.97f) { anyDown++; printf("   down: %s %.0f -> %.0f\n", SYNTH_NAMES[m], cl, ch); }
			if(strict) { if(ch / cl < minRatio) { minRatio = ch / cl; minM = m; } if(!(ch > cl * 1.05f)) fails++; }
		}
		CHECK("syn_j_level_opens_filter", fails == 0 && anyDown == 0,
			"fmod 1.0: centroid(level 1) / centroid(level 0.3) min %.2fx (%s) over strict set; %d patches went DOWN",
			minRatio, minM >= 0 ? SYNTH_NAMES[minM] : "-", anyDown);
	}
	/* (3b) fmod 0: level does not move the filter (centroid identical for new patches) */
	{
		float worst = 0;
		for(int m = 0; m < SYN_NNEW; m++) {
			std::vector<float> lo, hi;
			synRun(m, 196.0f, 0.3f, 0.0f, (int)(2.0f * SR), lo);
			synRun(m, 196.0f, 1.0f, 0.0f, (int)(2.0f * SR), hi);
			float r = fabsf(synCentroid(hi, (int)(0.8f * SR)) / synCentroid(lo, (int)(0.8f * SR)) - 1.0f);
			if(r > worst) worst = r;
		}
		/* swCut still lets the bloom (not the level) move the cutoff slightly; drift adds a little: 8 % bound */
		CHECK("syn_k_fmod0_static", worst < 0.08f, "fmod 0: new patches' centroid changes %.1f %% between level 0.3 and 1", worst * 100.0f);
	}
	/* (4) pot step 0 -> 1 mid-note: no zipper (largest sample step stays within 1.5x of steady state) */
	{
		float worst = 0; int worstM = -1;
		for(int m = 0; m < SYNTH_NMODES; m++) {
			std::vector<float> y; int st = (int)(1.5f * SR);
			synRun(m, 261.63f, 0.5f, 0.0f, (int)(2.0f * SR), y, 1, st, 1.0f);
			float a = 0, b = 0;
			for(int i = (int)(1.0f * SR); i < st; i++) a = fmaxf(a, fabsf(y[i] - y[i - 1]));
			for(int i = st; i < st + (int)(0.1f * SR); i++) b = fmaxf(b, fabsf(y[i] - y[i - 1]));
			float r = b / (a + 1e-9f);
			if(r > worst) { worst = r; worstM = m; }
		}
		CHECK("syn_l_pot_step_no_zipper", worst < 1.5f, "worst step ratio %.2f (%s)", worst, SYNTH_NAMES[worstM]);
	}
	/* (5) state migration: old saved index 5 (WARM TRI) -> 13; whole old table; new-format files untouched */
	{
		static const char *const OLD[15] = { "DRIFT PAD", "GLASS", "RINGMOD BELL", "SUB GROWL", "SOFT SQUARE", "WARM TRI",
			"WAVEFOLDER", "MOOG LEAD", "ACID SQUELCH", "FM GROWL", "FORMANT VOWEL", "SUPERSAW", "CHIP LEAD", "SAW LEAD", "SYNC LEAD" };
		int ok = synthModeMigrate(5, 0) == 13 && synthModeMigrate(5, SYNTH_STATE_VER) == 5
			&& synthModeMigrate(-1, 0) == 0 && synthModeMigrate(15, 0) == 0 && synthModeMigrate(99, SYNTH_STATE_VER) == 0;
		int names = 1;
		for(int m = 0; m < 15; m++) {
			int n = synthModeMigrate(m, 0);
			if(strcmp(SYNTH_NAMES[n], OLD[m]) != 0) names = 0;
		}
		CHECK("syn_m_state_migration", ok && names && !strcmp(SYNTH_NAMES[13], "WARM TRI") && !strcmp(SYNTH_NAMES[0], "JUNO PAD"),
			"old 5 -> %d (%s); all 15 old names map to the same sound: %d; ver-1 index untouched", synthModeMigrate(5, 0), SYNTH_NAMES[synthModeMigrate(5, 0)], names);
	}
}

static void synthTests(void)
{
	const float A = 0.15f;
	printf("\n--- synth engine tests (6 real voices, synthetic plucks) ---\n");
	/* (a) amplitude tracks the string's RMS envelope, sustain 0 */
	{
		std::vector<Pluck> pl; pl.push_back((Pluck){ 3, 0.30f, A, 25.0f });
		std::vector<Mute> mu; RigOut r;
		runRig(2.5f, pl, mu, 0.0f, 0, 1, r);
		float tOn = r.firstOn(3);
		const std::vector<float> &a = r.amp[3];
		double sd = 0, n = 0; std::vector<float> d, sa, sr;
		for(float t = tOn + 0.025f; t < 0.30f + 30.0f / 25.0f; t += 0.001f) {   /* down to -30 dB */
			float ref = ampDb(A * expf(-(t - 0.30f) * 25.0f / 8.685889f));
			float am = ampDb(a[(int)(t * SR)]);
			d.push_back(am - ref); sa.push_back(powf(10, am / 20)); sr.push_back(powf(10, ref / 20));
		}
		for(size_t i = 0; i < d.size(); i++) { sd += d[i]; n++; }
		float mean = (float)(sd / n), maxe = 0;
		for(size_t i = 0; i < d.size(); i++) if(fabsf(d[i] - mean) > maxe) maxe = fabsf(d[i] - mean);
		double ma = 0, mr = 0; for(size_t i = 0; i < d.size(); i++) { ma += sa[i]; mr += sr[i]; }
		ma /= n; mr /= n; double cov = 0, va = 0, vr = 0;
		for(size_t i = 0; i < d.size(); i++) { cov += (sa[i]-ma)*(sr[i]-mr); va += (sa[i]-ma)*(sa[i]-ma); vr += (sr[i]-mr)*(sr[i]-mr); }
		float corr = (float)(cov / sqrt(va * vr + 1e-30));
		CHECK("syn_a_tracks_rms", corr > 0.995f && maxe < 3.0f,
			"corr %.4f, max dB err %.2f (note-on %.0f ms after pick, window to -30 dB)", corr, maxe, (tOn - 0.30f) * 1000.0f);
	}
	/* (b) sustain 0.4: slower than the string, monotone, no step > 0.5 dB/ms, exponential end */
	{
		std::vector<Pluck> pl; pl.push_back((Pluck){ 3, 0.30f, A, 25.0f });
		std::vector<Mute> mu; RigOut r;
		runRig(7.0f, pl, mu, 0.4f, 0, 1, r);
		const std::vector<float> &a = r.amp[3];
		int n = (int)a.size(), ipk = 0;
		for(int i = 0; i < n; i++) if(a[i] > a[ipk]) ipk = i;
		int nonmono = 0; float worst = 0;
		int step = (int)(0.001f * SR);
		for(int i = ipk; i + step < n; i++) {
			if(a[i + 1] > a[i] * 1.0001f && a[i] > 1e-5f) nonmono++;
			if(a[i + step] > 0.0012f) {   /* above the note-end threshold */
				float dd = ampDb(a[i]) - ampDb(a[i + step]);
				if(dd > worst) worst = dd;
			}
		}
		float t1 = 1.0f, t2 = 2.0f, t3 = 3.0f, t4 = 4.0f;
		float s1 = ampDb(a[(int)(t1*SR)]) - ampDb(a[(int)(t2*SR)]);
		float s2 = ampDb(a[(int)(t3*SR)]) - ampDb(a[(int)(t4*SR)]);
		float strDrop = 25.0f;
		int off = r.offs(3);
		float rate = 60.0f * powf(1.5f / 60.0f, 0.4f);
		CHECK("syn_b_sustain0.4", nonmono == 0 && worst < 0.5f && s1 < strDrop * 0.8f
			&& fabsf(s1 - s2) < 0.05f * s1 && off == 1,
			"nonmono %d, worst %.3f dB/ms, decay %.1f then %.1f dB/s (string %.0f, cap %.1f), OFF events %d",
			nonmono, worst, s1, s2, strDrop, rate, off);
	}
	/* (c) palm mute: 30 dB in 20 ms -> graceful ~200 ms fade, no click (sustain 0.4) */
	{
		std::vector<Pluck> pl; pl.push_back((Pluck){ 3, 0.30f, A, 12.0f });
		std::vector<Mute> mu; mu.push_back((Mute){ 3, 0.80f });
		RigOut r;
		runRig(2.0f, pl, mu, 0.4f, 0, 1, r);
		const std::vector<float> &a = r.amp[3];
		int im = (int)(0.80f * SR);
		float a0 = a[im];
		int ie = -1, i20 = -1;
		for(int i = im; i < (int)a.size(); i++) {
			if(ie < 0 && a[i] < a0 * 0.3679f) ie = i;
			if(i20 < 0 && a[i] < a0 * 0.1f) { i20 = i; break; }
		}
		float maxStep = 0;
		for(int i = im; i + 1 < (int)a.size(); i++) { float d = fabsf(a[i+1] - a[i]); if(d > maxStep) maxStep = d; }
		float te = ie > 0 ? (float)(ie - im) / SR * 1000.0f : -1, t20 = i20 > 0 ? (float)(i20 - im) / SR * 1000.0f : -1;
		CHECK("syn_c_palm_mute", te > 100.0f && te < 350.0f && maxStep < 2e-3f && r.offs(3) <= 1,
			"1/e time %.0f ms, -20 dB time %.0f ms, max |step| %.5f per sample, OFF why %s",
			te, t20, maxStep, r.offs(3) ? "damp-or-floor" : "none-yet");
	}
	/* (d) neighbour pick bleeding -23 dB into silent strings: no note */
	{
		std::vector<Pluck> pl; pl.push_back((Pluck){ 3, 0.30f, 0.20f, 25.0f });
		std::vector<Mute> mu; RigOut r1, r2;
		runRig(1.5f, pl, mu, 0.0f, 1, 1, r1);
		runRig(1.5f, pl, mu, 0.0f, 1, 0, r2);        /* upstream relative onset gate disabled: synth gating alone */
		int bad1 = 0, bad2 = 0, gh2 = 0, ons2 = 0;
		for(int s = 0; s < NUM_STRINGS; s++) if(s != 3) { bad1 += r1.accepted(s); bad2 += r2.accepted(s); gh2 += r2.why(s, SYN_W_GHOST); ons2 += r2.why(s, SYN_W_GHOST) + r2.why(s, SYN_W_TIMEOUT) + r2.accepted(s); }
		CHECK("syn_d_no_ghost_note", bad1 == 0 && bad2 == 0,
			"full chain: %d bleed notes; upstream gate OFF: %d notes (%d onsets seen, %d rejected ghost)", bad1, bad2, ons2, gh2);
	}
	/* (e) soft real pick 12 dB under a hard adjacent pick DOES sound */
	{
		std::vector<Pluck> pl;
		pl.push_back((Pluck){ 3, 0.30f, 0.20f, 25.0f });
		pl.push_back((Pluck){ 4, 0.303f, 0.20f * 0.2512f, 25.0f });
		std::vector<Mute> mu; RigOut r;
		runRig(1.5f, pl, mu, 0.0f, 1, 1, r);
		CHECK("syn_e_soft_pick_sounds", r.accepted(4) == 1 && r.accepted(3) == 1,
			"s3 hard notes %d, s4 (-12 dB) notes %d (ghost %d, dbl %d, timeout %d)",
			r.accepted(3), r.accepted(4), r.why(4, SYN_W_GHOST), r.why(4, SYN_W_DBL), r.why(4, SYN_W_TIMEOUT));
	}
	/* (f) one low-C pluck -> exactly one note-on */
	{
		std::vector<Pluck> pl; pl.push_back((Pluck){ 0, 0.30f, A, 15.0f });
		std::vector<Mute> mu; RigOut r;
		runRig(2.5f, pl, mu, 0.0f, 1, 1, r);
		int tot = 0; for(int s = 0; s < NUM_STRINGS; s++) tot += r.accepted(s);
		CHECK("syn_f_single_low_c", r.accepted(0) == 1 && tot == 1,
			"s0 note-ons %d (all strings %d), dbl-rejects %d, on %.0f ms after pick",
			r.accepted(0), tot, r.why(0, SYN_W_DBL), (r.firstOn(0) - 0.30f) * 1000.0f);
	}
	/* (g) repeated picks at 94 ms each produce a note-on */
	{
		std::vector<Pluck> pl; for(int k = 0; k < 6; k++) pl.push_back((Pluck){ 2, 0.30f + 0.094f * (float)k, A, 45.0f });
		std::vector<Mute> mu; RigOut r;
		runRig(1.6f, pl, mu, 0.0f, 1, 1, r, 2);   /* the upstream ratio detector (envF > 2.2 envS) cannot see an equal-force re-pick over a ringing string, so its onsets are forced at the pick */
		/* each pick must be followed by an accept within 70 ms */
		int hit = 0;
		for(int k = 0; k < 6; k++) {
			float t0 = 0.30f + 0.094f * (float)k;
			for(size_t i = 0; i < r.ev.size(); i++)
				if(r.ev[i].s == 2 && r.ev[i].code == SHDIAG_SYN_ON && r.ev[i].why == SYN_W_ACCEPTED
				   && r.ev[i].tSec >= t0 && r.ev[i].tSec < t0 + 0.094f) { hit++; break; }
		}
		CHECK("syn_g_94ms_repeats", hit == 6 && r.accepted(2) == 6,
			"picks answered %d/6, note-ons %d, dbl-rejects %d, timeouts %d, detector onsets %d",
			hit, r.accepted(2), r.why(2, SYN_W_DBL), r.why(2, SYN_W_TIMEOUT), r.nOns[2]);
	}
	analogSynthTests();
}
