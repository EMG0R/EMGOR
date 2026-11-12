// drum_voice_ref.cpp -- the ORIGINAL per-kind drum voice (moved out of pmor_core/drum_sounds.cpp when the synth engine replaced it). Test-only: tests/test_drumsynth
// renders every preset through this and through the synth engine and compares (SNR table). Do not use in firmware.
#include <string.h>
#include "drum_voice_ref.h"
#include "fx.h"
#include "shared_state.h"
static inline float fxTanhL(float z) { if (z > 3.f) return 1.f; if (z < -3.f) return -1.f; float z2 = z * z; return z * (27.f + z2) / (27.f + 9.f * z2); }

// ==================================================================== DrumVoice

void RefVoice::trigger(const SoundDef& d, const DrumPadCfg& cfg, float vel,
                         uint8_t grp, uint8_t idxGrp, uint8_t sIdx, uint32_t& seed) {
    def = &d;
    group = grp; idxInGrp = idxGrp; soundIdx = sIdx;
    age = 0;
    lenRatio = fastPow2((clampf(cfg.length, 0.f, 1.f) - 0.5f) * 4.f);
    reverse  = cfg.reverse || d.forceReverse;
    accent   = 0.5f + 0.5f * clampf(vel, 0.f, 1.f);
    tone     = clampf(cfg.tone, 0.f, 1.f);

    float pRatio = fastPow2(clampf(cfg.pitch, -1.f, 1.f));
    if (d.pmode == PM_KEYROOT) {
        // KICK ROOT = F1 (43.65 Hz) shifted by the global key — exactly v2's drumPadHit().
        // (scaleHz(0,0) is the synth's F3 root: using it put every kick at 175 Hz = a tom.)
        fBase = scaleHz(0, 0) * 0.25f * pRatio;
        if (d.kind == VK_LEGACY) fBase *= (d.f0 / 52.f);   // v2: each kick's nominal/52 ratio
    } else if (d.pmode == PM_TOMDEG) {
        int degree = scaleTomDegree(lcg01(seed));
        int octJit = (int)(lcg(seed) & 1u);
        fBase = scaleHz(degree, (int)d.tomOct + octJit) * pRatio;
    } else {
        fBase = d.f0 * pRatio;
        if (d.kind == VK_LEGACY && d.f0 <= 1.f)               // v2 "modular blip": random pitch every hit
            fBase = (300.f + (float)(lcg(seed) % 2100u)) * pRatio;
    }
    if (fBase < 10.f) fBase = 10.f;

    // universal TONE colour sweep target (see drums_priv.h comment)
    toneHp = (grp == DG_HAT);
    static constexpr float kLoF = 400.f;          // ..14000 Hz sweep (log2(14000/400) ~= 5.129283)
    toneHz = kLoF * fastPow2(tone * 5.129283f);

    // nominal total length (pre length-ratio) per kind, then scale
    float nomLen;
    switch (d.kind) {
        case VK_RESON: {
            nomLen = d.t60_0;
            if (d.f1r > 0.f && d.t60_1 > nomLen) nomLen = d.t60_1;
            if (d.f2r > 0.f && d.t60_2 > nomLen) nomLen = d.t60_2;
            if (d.nLvl > 0.f && d.nT60 > nomLen) nomLen = d.nT60;
            break;
        }
        case VK_DUAL: {
            nomLen = d.t60_0;
            if (d.t60_1 > nomLen) nomLen = d.t60_1;
            float nl = d.nT60 + d.nHoldMs * 0.001f;
            if (nl > nomLen) nomLen = nl;
            break;
        }
        case VK_SWEEP: {
            nomLen = d.t60_0;
            float sw = d.sweepMs * 0.001f;
            if (sw > nomLen) nomLen = sw;
            break;
        }
        case VK_CLAP:    nomLen = d.tailT60 + d.bN * d.bGapMs * 0.001f; break;
        case VK_NOISE:   nomLen = d.nT60 + d.nHoldMs * 0.001f; break;
        case VK_METAL:   nomLen = d.t60_0 > d.t60_1 ? d.t60_0 : d.t60_1; break;
        case VK_RING:    nomLen = d.t60_0; break;
        case VK_FM:      nomLen = d.fmMs * 0.001f + d.t60_0; break;
        case VK_COWBELL: nomLen = d.t60_1; break;
        case VK_GLITCH:  nomLen = d.t60_0 + d.retrigN * d.retrigGapMs * 0.001f; break;
        case VK_LEGACY:  nomLen = logf(0.0008f) / logf(d.lDec) * DRUM_FS_INV; break;   // v2's env > 0.0008 cutoff
        case VK_GENFM: {
            // per-hit roll from the voice seed (drawn only for this kind: the classic kit's draw order is untouched)
            static const float kRatios[8] = { 1.f, 1.41421f, 1.5f, 2.f, 2.41421f, 3.f, 3.73205f, 5.19615f };
            float c = clampf(d.gfChaos, 0.f, 1.f);
            gfRatio = kRatios[lcg(seed) & 7u];
            gfFc    = fBase * fastPow2((lcg01(seed) * 2.f - 1.f) * c * 1.5f);
            gfIdx   = d.fmIdx0 * (1.f - 0.6f * c + 0.8f * c * lcg01(seed));
            gfDec   = d.t60_0 * fastPow2((lcg01(seed) * 2.f - 1.f) * c);
            gfDrop  = 1.f + c * 3.f * lcg01(seed);
            nomLen  = gfDec + 0.01f;
            break;
        }
        default:         nomLen = 0.3f; break;
    }
    if (nomLen < 0.02f) nomLen = 0.02f;
    totalLen = nomLen * lenRatio;

    // pan (§ task spec): kick/snare centre; hats 0/1 hard L/R-ish, 2/3 centre;
    // percussion gets a fresh random pan every hit.
    float p;
    if (grp == DG_KICK || grp == DG_SNARE) {
        p = 0.5f;
    } else if (grp == DG_HAT) {
        p = (idxGrp == 0) ? 0.25f : (idxGrp == 1) ? 0.75f : 0.5f;
    } else {
        p = 0.25f + 0.5f * lcg01(seed);
    }
    panL = cosf(p * 1.57079633f);
    panR = sinf(p * 1.57079633f);

    // fresh DSP state so an old tail can never bleed into the new hit
    for (int i = 0; i < 3; i++) { ry1[i] = ry2[i] = 0.f; rR[i] = 0.f; rCw[i] = 1.f; rNorm[i] = 1.f; }
    quietRun = 0;
    ph0 = ph1 = ph2 = 0.f; iph0 = iph1 = iph2 = 0u;
    ampCache = 0.f;
    svfA = Svf(); svfB = Svf(); svfC = Svf(); svfD = Svf();
    dc = DcBlock();
    crushHold = 0.f; crushPhase = 0.f;
    crushLevels = fastPow2(d.crushBits);

    // engine depth stage: preset value + the engine's live knob. All-zero => xOn false => process() never enters it.
    {
        const DrumEngCfg& ec = drums.eng[grp < DG_COUNT ? grp : 0];
        xSnap  = clampf(d.xSnap  + ec.x[0], 0.f, 1.f);
        xBody  = clampf(d.xBody  + ec.x[1], 0.f, 1.f);
        xDrive = clampf(d.xDrive + ec.x[2], 0.f, 1.f);
        xCrush = clampf(d.xCrush + ec.x[3], 0.f, 1.f);
        xOn = (xSnap > 0.f) || (xBody > 0.f) || (xDrive > 0.f) || (xCrush > 0.f);
        xSnapEnv = 1.f; xSnapLp = 0.f; xBodyPh = 0.f; xHold = 0.f; xHoldN = 0u;
        static const float kSnapMs[DG_COUNT] = { 3.f, 8.f, 2.f, 5.f };
        xSnapCoef = fastExp(-1.f / (kSnapMs[grp < DG_COUNT ? grp : 0] * 0.001f * SR));
        // body layer pitch: kick/snare/perc = sub-harmonic of the voice pitch, hat = fixed inharmonic ping
        xBodyHz = (grp == DG_HAT) ? 4200.f * pRatio : fBase * 0.5f;
        xHoldLen = 1u + (uint32_t)(xCrush * 14.f);
    }

    state = VS_ACTIVE;
}

void RefVoice::startChoke(uint32_t ms) {
    chokeTotal = (uint32_t)(ms * 0.001f * SR);
    if (chokeTotal < 8u) chokeTotal = 8u;
    chokeLeft = chokeTotal;
    state = VS_CHOKE;
}

// ---- shared per-sample helpers ---------------------------------------------

float RefVoice::env(float t60, float holdS) const {
    if (t60 < 0.003f) t60 = 0.003f;
    t60 *= lenRatio; holdS *= lenRatio;       // LENGTH scales every kind's decay (x1.f at the default: bit-identical)
    float tau = t60 * (1.f / 6.9078f);
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    float e = (tEff <= holdS) ? 1.f : fastExp(-(tEff - holdS) / tau);
    if (!reverse) {
        const float atk = 0.0004f;
        if (tNow < atk) e *= 0.5f - 0.5f * cosf(DRUM_PI * (tNow / atk));
    } else {
        const float rel = 0.005f;
        float remain = totalLen - tNow;
        if (remain < rel) {
            float g = remain > 0.f ? remain / rel : 0.f;
            e *= 0.5f - 0.5f * cosf(DRUM_PI * g);
        }
    }
    return e;
}

float RefVoice::genExp(float start, float end, float Tms) const {
    if (Tms < 0.5f) Tms = 0.5f;
    Tms *= lenRatio;
    float tau = (Tms * 0.001f) / 5.f;
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    return end + (start - end) * fastExp(-tEff / tau);
}

float RefVoice::genLog(float start, float end, float Tms) const {
    if (Tms < 0.5f) Tms = 0.5f;
    Tms *= lenRatio;
    float T = Tms * 0.001f;
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    if (tEff > T) tEff = T;
    float u = 1.f - tEff / T;
    if (end < 1.f) end = 1.f;
    if (start < 1.f) start = 1.f;
    return end * fastExp(u * logf(start / end));
}

void RefVoice::setReson(int slot, float freq, float t60) {
    if (age != 0u && (age & 7u) != 0u) return;    // recompute at ~6 kHz control-rate, not every sample
    if (freq < 20.f) freq = 20.f;
    if (freq > SR * 0.45f) freq = SR * 0.45f;
    if (t60 < 0.005f) t60 = 0.005f;
    // The amplitude envelope env(t60) shapes the hit (and makes REVERSE work); the
    // resonator itself rings ~4x longer so it never fights the envelope. Q from
    // t60*4: Q = T60*f/2.2 (§0 "bridged-T resonator").
    float Q = (t60 * 4.f * freq) * 0.4545455f;
    if (Q < 0.5f) Q = 0.5f;
    float w = DRUM_TWOPI * freq / SR;
    rR[slot]  = fastExp(-DRUM_PI * freq / (Q * SR));
    rCw[slot] = cosf(w);
    // 2-pole impulse peak ~= 1/sin(w); the exciter pingExc() sums to ~14.4 impulse-units.
    rNorm[slot] = sinf(w) * (1.f / 14.4f);
}

float RefVoice::tickReson(int slot, float x) {
    float y = 2.f * rR[slot] * rCw[slot] * ry1[slot] - rR[slot] * rR[slot] * ry2[slot] + x;
    ry2[slot] = flushDenorm(ry1[slot]);
    ry1[slot] = flushDenorm(y);
    return y * rNorm[slot];
}

// short exponential click, stands in for the doc's "1 ms pulse, low-passed"
// exciter (§0) — pings the resonator without a discontinuity.
float RefVoice::pingExc(float amp) const {
    if (age > 32u) return 0.f;
    float t = (float)age * DRUM_FS_INV;
    return amp * fastExp(-t / 0.0003f);
}

float RefVoice::shape(float x, const SoundDef* d) const {
    if (d->drive > 0.f) x = drumSwing(x, 1.f + d->drive);
    if (d->fold) {
        float y = x;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        x = 0.7f * y + 0.3f * x;
    }
    return x;
}

float RefVoice::noiseLayer(float noiseSample, const SoundDef* d) {
    if (d->nLvl <= 0.f) return 0.f;
    float holdS = d->nHoldMs * 0.001f;
    float a = env(d->nT60, holdS);
    float atkS = d->nAtkMs * 0.001f;
    float tNow = (float)age * DRUM_FS_INV;
    if (!reverse && atkS > 0.f && tNow < atkS) a *= (tNow / atkS);

    if (d->nF <= 0.f) return noiseSample * a * d->nLvl;    // unfiltered white noise (S10)

    float fc = d->nF;
    if (d->nSweepTo > 0.f) fc = genExp(d->nF, d->nSweepTo, d->sweepMs > 0.f ? d->sweepMs : 60.f);
    if (age == 0u || (d->nSweepTo > 0.f && (age & 7u) == 0u)) svfB.setFc(fc, d->nQ);
    float lp, bp, hp;
    svfB.tick(noiseSample, lp, bp, hp);
    float band = d->nBp ? bp : hp;
    return band * a * d->nLvl;
}

// ---- per-kind engines --------------------------------------------------

float RefVoice::procReson(const SoundDef* d, float noise) {
    float freq0 = fBase;
    if (d->clickMs > 0.f) freq0 = genExp(fBase * d->clickBump, fBase, d->clickMs);
    if (d->selfFm > 0.f) freq0 *= (1.f + d->selfFm * 0.08f * ampCache);
    setReson(0, freq0, d->t60_0);
    float exc = pingExc(accent);
    float y0 = tickReson(0, exc);
    float a0 = env(d->t60_0);
    ampCache = a0;
    float out = y0 * a0 * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        setReson(1, fBase * d->f1r, d->t60_1);
        float y1 = tickReson(1, exc);
        out += y1 * env(d->t60_1) * d->g1;
    }
    if (d->f2r > 0.f) {
        setReson(2, fBase * d->f2r, d->t60_2);
        float y2 = tickReson(2, exc);
        out += y2 * env(d->t60_2) * d->g2;
    }
    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procDual(const SoundDef* d, float noise) {
    float exc = d->useOsc ? 0.f : pingExc(accent);

    float freq0 = fBase;
    if (d->clickMs > 0.f) freq0 = genExp(fBase * d->clickBump, fBase, d->clickMs);
    float a0 = env(d->t60_0);
    ampCache = a0;
    float y0;
    if (d->useOsc) {
        ph0 += DRUM_TWOPI * freq0 * DRUM_FS_INV;
        if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        float s = sinf(ph0);
        y0 = 2.f * s / (1.f + fabsf(s));        // "rounded" triangle-ish core
    } else {
        setReson(0, freq0, d->t60_0);
        y0 = tickReson(0, exc);
    }
    float out = y0 * a0 * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        float freq1 = freq0 * d->f1r;
        float a1 = env(d->t60_1);
        float y1;
        if (d->useOsc) {
            ph1 += DRUM_TWOPI * freq1 * DRUM_FS_INV;
            if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
            float s = sinf(ph1);
            y1 = 2.f * s / (1.f + fabsf(s));
        } else {
            setReson(1, freq1, d->t60_1);
            y1 = tickReson(1, exc);
        }
        out += y1 * a1 * d->g1;
    }

    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procSweep(const SoundDef* d, float noise) {
    float freq = d->sweepLog ? genLog(fBase * d->sweepStartR, fBase, d->sweepMs)
                              : genExp(fBase * d->sweepStartR, fBase, d->sweepMs);
    if (d->vibHz > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        freq *= (1.f + d->vibDepth * sinf(DRUM_TWOPI * d->vibHz * tNow));
    }
    float a = env(d->t60_0);
    ampCache = a;

    float y;
    if (d->useSquare) {
        uint32_t inc = (uint32_t)((freq * DRUM_FS_INV) * 4294967296.f);
        iph0 += inc;
        y = (iph0 & 0x80000000u) ? -1.f : 1.f;
    } else {
        ph0 += DRUM_TWOPI * freq * DRUM_FS_INV;
        if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        y = sinf(ph0);
    }
    if (d->amHz > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        float am = 1.f - d->amDepth * (0.5f - 0.5f * sinf(DRUM_TWOPI * d->amHz * tNow));
        y *= am;
    }

    float out = y * a * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        float f1 = genExp(fBase * d->sweepStartR * 0.8f, fBase * d->f1r, d->sweepMs * 0.8f);
        ph1 += DRUM_TWOPI * f1 * DRUM_FS_INV;
        if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
        out += sinf(ph1) * env(d->t60_1) * d->g1;
    }

    if (d->vcf) {
        float fc = genExp(d->vcfStartHz, d->vcfEndHz, d->sweepMs);
        if (age == 0u || (age & 7u) == 0u) svfA.setFc(fc, d->vcfQ);
        float lp, bp, hp; svfA.tick(out, lp, bp, hp);
        out = lp;
    }
    if (d->lpTrackMult > 0.f) {
        float fc = freq * d->lpTrackMult;
        if (age == 0u || (age & 7u) == 0u) svfC.setFc(fc, 0.9f);
        float lp, bp, hp; svfC.tick(out, lp, bp, hp);
        out = lp;
    }

    out += noiseLayer(noise, d);

    if (d->crush) {
        crushPhase += d->crushHz * DRUM_FS_INV;
        if (crushPhase >= 1.f) {
            crushPhase -= 1.f;
            crushHold = floorf(out * crushLevels + 0.5f) / crushLevels;
        }
        out = crushHold;
    }
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procClap(const SoundDef* d, float noise) {
    float tNow = (float)age * DRUM_FS_INV;
    float tRef = reverse ? (totalLen - tNow) : tNow;
    if (tRef < 0.f) tRef = 0.f;

    if (age == 0u) svfB.setFc(d->nF, d->nQ);
    float lp, bp, hp; svfB.tick(noise, lp, bp, hp);
    float band = bp;

    uint8_t n = d->bN > 0 ? d->bN : 1;
    float burstTau = d->bDurMs * 0.001f * 0.35f;
    float burstEnv = 0.f;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t bi = reverse ? (uint8_t)(n - 1 - i) : i;   // reverse: mirror burst order
        float dt = tRef - bi * (d->bGapMs * 0.001f);
        if (dt >= 0.f) {
            float e = fastExp(-dt / burstTau);
            if (e > burstEnv) burstEnv = e;
        }
    }
    if (reverse) {                      // the mirrored first burst sits at e=1 once tRef hits 0: end it with the same 5 ms release env() uses
        float remain = totalLen - tNow;
        if (remain < 0.005f) { float g = remain > 0.f ? remain / 0.005f : 0.f; burstEnv *= 0.5f - 0.5f * cosf(DRUM_PI * g); }
    }
    float tail = env(d->tailT60);
    float atk = d->tailAtkMs * 0.001f;
    if (!reverse && atk > 0.f && tNow < atk) tail *= (tNow / atk);

    ampCache = burstEnv > tail ? burstEnv : tail;
    float out = band * (burstEnv + tail * d->tailLvl);
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procNoise(const SoundDef* d, float noise) {
    float out = noiseLayer(noise, d);
    out *= (0.9f + 0.2f * lcg01(rnd));      // "random +-1.5 dB per hit" (§ several perc)
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procMetal(const SoundDef* d, float metal, float noise) {
    float sig = metal;
    if (d->noiseMix > 0.f) sig = sig * (1.f - d->noiseMix) + noise * d->noiseMix;

    float lp, bp, hp;
    if (age == 0u) svfA.setFc(d->bp1, d->bp1Q);
    svfA.tick(sig, lp, bp, hp);
    float band1 = bp;

    if (age == 0u) svfB.setFc(d->bp2, d->bp2Q);
    svfB.tick(sig, lp, bp, hp);
    float band2 = bp;

    float hpCut = d->hpF * (0.4f + 1.2f * tone);      // TONE brightens the final HP corner
    if (age == 0u || (age & 7u) == 0u) svfC.setFc(hpCut, d->hpQ);
    float src = d->twoBand ? band2 : (band1 + band2);
    svfC.tick(src, lp, bp, hp);
    float hiPart = hp;

    float envA = env(d->t60_0);
    ampCache = envA;
    float out;
    if (d->twoBand) {
        float envB = env(d->t60_1 > 0.f ? d->t60_1 : d->t60_0 * 0.6f);
        out = drumSwing(band1, d->g0) * envA + drumSwing(hiPart, d->g1) * envB;
    } else {
        out = drumSwing(hiPart, d->g0) * envA;
    }

    if (d->pingLvl > 0.f) {
        ph0 += DRUM_TWOPI * d->pingF0 * DRUM_FS_INV; if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        ph1 += DRUM_TWOPI * d->pingF1 * DRUM_FS_INV; if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
        float pingEnv = env(d->pingT60);
        out += (sinf(ph0) + sinf(ph1)) * 0.5f * pingEnv * d->pingLvl;
    }
    if (d->delay2Ms > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        float t0 = d->delay2Ms * 0.001f;
        if (tNow >= t0) {
            float e2 = fastExp(-(tNow - t0) / (d->t60_0 * 0.166096f));
            out += hiPart * e2 * d->delay2Lvl;
        }
    }
    if (d->crush) {
        crushPhase += d->crushHz * DRUM_FS_INV;
        if (crushPhase >= 1.f) {
            crushPhase -= 1.f;
            crushHold = floorf(out * crushLevels + 0.5f) / crushLevels;
        }
        out = crushHold;
    }
    return dc.process(out) * d->gain;
}

float RefVoice::procRing(const SoundDef* d) {
    ph0 += DRUM_TWOPI * (fBase * 0.667f) * DRUM_FS_INV; if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
    iph0 += (uint32_t)((fBase * 25.1f * DRUM_FS_INV) * 4294967296.f);
    float r0 = sinf(ph0) * ((iph0 & 0x80000000u) ? -1.f : 1.f);

    ph1 += DRUM_TWOPI * (fBase * 1.7f) * DRUM_FS_INV; if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
    iph1 += (uint32_t)((fBase * 26.9f * DRUM_FS_INV) * 4294967296.f);
    float r1 = sinf(ph1) * ((iph1 & 0x80000000u) ? -1.f : 1.f);

    ph2 += DRUM_TWOPI * (fBase * 2.43f) * DRUM_FS_INV; if (ph2 > DRUM_TWOPI) ph2 -= DRUM_TWOPI;
    iph2 += (uint32_t)((fBase * 35.0f * DRUM_FS_INV) * 4294967296.f);
    float r2 = sinf(ph2) * ((iph2 & 0x80000000u) ? -1.f : 1.f);

    float sum = (r0 + r1 + r2) * (1.f / 3.f);

    float fc = d->bp1 * (0.4f + 1.2f * tone);
    if (age == 0u || (age & 7u) == 0u) svfA.setFc(fc, d->bp1Q);
    float lp, bp, hp; svfA.tick(sum, lp, bp, hp);

    float a = env(d->t60_0);
    ampCache = a;
    float out = bp * a * (d->g0 > 0.f ? d->g0 : 1.f);
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procFm(const SoundDef* d, float noise) {
    float carrier = fBase;
    if (d->clickMs > 0.f) carrier = genExp(fBase * d->clickBump, fBase, d->clickMs);
    float idx = genExp(d->fmIdx0, d->fmIdx1, d->fmMs) * (0.5f + 1.f * tone);

    ph1 += DRUM_TWOPI * (carrier * d->fmRatio) * DRUM_FS_INV;
    if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
    float mod = sinf(ph1);

    ph0 += DRUM_TWOPI * carrier * DRUM_FS_INV;
    if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
    float y = sinf(ph0 + idx * mod);

    float a = env(d->t60_0);
    ampCache = a;
    float out = y * a * (d->g0 > 0.f ? d->g0 : 1.f);
    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procCowbell(const SoundDef* d) {
    float f1 = fBase;
    float f2 = fBase * (d->f1r > 0.f ? d->f1r : 1.481f);

    iph0 += (uint32_t)((f1 * DRUM_FS_INV) * 4294967296.f);
    iph1 += (uint32_t)((f2 * DRUM_FS_INV) * 4294967296.f);
    float sq1 = (iph0 & 0x80000000u) ? -1.f : 1.f;
    float sq2 = (iph1 & 0x80000000u) ? -1.f : 1.f;
    float sum = (sq1 + sq2) * 0.5f;

    float q = (d->nQ > 0.f ? d->nQ : 5.f) * (0.5f + 1.f * tone);
    if (age == 0u) { svfA.setFc(f1, q); svfB.setFc(f2, q); }
    float lp, bp, hp;
    svfA.tick(sum, lp, bp, hp); float b1 = bp;
    svfB.tick(sum, lp, bp, hp); float b2 = bp;
    float band = (b1 + b2) * 0.5f;

    float fast = env(d->t60_0);
    float tailE = env(d->t60_1);
    float envv = 0.6f * fast + 0.4f * tailE;
    ampCache = envv;

    float out = drumSwing(band, d->g0 > 0.f ? d->g0 : 1.5f) * envv;
    return dc.process(shape(out, d)) * d->gain;
}

float RefVoice::procGlitch(const SoundDef* d, float noise) {
    float tNow = (float)age * DRUM_FS_INV;
    float gap = d->retrigGapMs * 0.001f * lenRatio;
    uint8_t n = d->retrigN > 0 ? d->retrigN : 1;
    uint8_t idx = (uint8_t)(tNow / gap);
    if (idx >= n) idx = n - 1;
    float tLocal = tNow - idx * gap;

    float mul = 1.f;
    for (uint8_t i = 0; i < idx; i++) mul *= d->retrigMul;

    float out;
    if (d->retrigNoise) {
        float e = fastExp(-tLocal / (0.006f * lenRatio));
        out = noise * e;
    } else {
        setReson(0, fBase, d->t60_0);
        float exc = (tLocal < 0.0008f) ? accent * fastExp(-tLocal / 0.0003f) : 0.f;
        float y = tickReson(0, exc);
        float e = fastExp(-tLocal / (d->t60_0 * lenRatio * 0.166096f));
        out = y * e;
    }
    out *= mul;
    ampCache = mul;
    return dc.process(shape(out, d)) * d->gain;
}

// v2 drum engine (PMOR_V2.cpp L2456-2486), per voice. LENGTH scales the per-sample
// decay exponent; REVERSE plays the envelope and the pitch drop time-reversed.
float RefVoice::procLegacy(const SoundDef* d, float noise) {
    if (age == 0u) {
        legFt = fBase;
        if (legFt < 5.f) legFt = 5.f;
        if (legFt > 16000.f) legFt = 16000.f;
        legF = legFt * d->lDrop;
        if (legF > 20000.f) legF = 20000.f;
        legHatLp = 0.f;
        ph0 = 0.f;
    }
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    // envelope: dec^samples, with the length ratio applied to the exponent
    float envv = fastExp(logf(d->lDec) * (tEff * SR) / lenRatio);
    // pitch drop: one-pole 0.0012/sample from ft*drop toward ft (time-reversed when reversed)
    float f = legFt + (legFt * d->lDrop - legFt) * fastExp(-0.0012f * (tEff * SR) / lenRatio);
    ph0 += f * DRUM_FS_INV; if (ph0 >= 1.f) ph0 -= 1.f;
    float tone = sinf(6.2831853f * ph0);
    if (d->lPart > 0.f) tone = 0.6f * tone + 0.45f * sinf(6.2831853f * ph0 * d->lPart);
    float nz = noise;
    if (d->lNoiz > 0.97f) { legHatLp += (nz - legHatLp) * 0.12f; nz -= legHatLp; }   // hat HP
    float sd = (tone * (1.f - d->lNoiz) + nz * d->lNoiz) * envv * d->lAmp * accent;
    if (!reverse && tNow < 0.0004f) sd *= tNow / 0.0004f;                      // anti-click
    if (reverse) {                                                              // REVERSE ends at totalLen: 5 ms raised-cosine release, silence after (was: envelope clamped at 1 forever)
        float remain = totalLen - tNow;
        if (remain < 0.005f) { float g = remain > 0.f ? remain / 0.005f : 0.f; sd *= 0.5f - 0.5f * cosf(DRUM_PI * g); }
    }
    // v2 drum bus: x1.5 into tanh(x*2.0) — that saturation IS the beef
    return fxTanhL(sd * 3.0f) * d->gain;
}

float RefVoice::render(const SoundDef* d, float metal, float noise) {
    float raw;
    switch (d->kind) {
        case VK_RESON:   raw = procReson(d, noise);        break;
        case VK_DUAL:    raw = procDual(d, noise);          break;
        case VK_SWEEP:   raw = procSweep(d, noise);         break;
        case VK_CLAP:    raw = procClap(d, noise);          break;
        case VK_NOISE:   raw = procNoise(d, noise);         break;
        case VK_METAL:   raw = procMetal(d, metal, noise);  break;
        case VK_RING:    raw = procRing(d);                 break;
        case VK_FM:      raw = procFm(d, noise);            break;
        case VK_COWBELL: raw = procCowbell(d);              break;
        case VK_GLITCH:  raw = procGlitch(d, noise);        break;
        case VK_LEGACY:  raw = procLegacy(d, noise);        break;
        case VK_GENFM:   raw = procGenFm(d, noise);         break;
        default:         raw = 0.f;                         break;
    }
    if (d->kind != VK_METAL && d->kind != VK_LEGACY) {   // METAL/COWBELL/FM wire `tone` in directly; LEGACY = v2 verbatim (tone unused)
        if (age == 0u) svfD.setFc(toneHz, 0.7f);
        float lp, bp, hp; svfD.tick(raw, lp, bp, hp);
        raw = toneHp ? hp : lp;
        // uniform saturation (the v2 bus tanh that made the kicks beefy): gain is a static
        // trim applied AFTER it, so level-matching never changes the amount of grit.
        if (d->satDrive > 0.f) raw = fxTanhL((raw / d->gain) * d->satDrive) * d->gain;
    }
    return flushBad(raw);
}

float RefVoice::process(float metal, float noise) {
    if (state == VS_IDLE) return 0.f;

    if (state == VS_CHOKE) {
        float raw = def ? render(def, metal, noise) : 0.f;
        if (xOn && def) raw = applyExtras(raw, noise);
        float g = (chokeTotal > 0u) ? (0.5f - 0.5f * cosf(DRUM_PI * ((float)chokeLeft / (float)chokeTotal))) : 0.f;
        float out = raw * g;
        age++;
        if (chokeLeft > 0u) chokeLeft--;
        if (chokeLeft == 0u) state = VS_IDLE;
        return out;
    }

    float raw = render(def, metal, noise);
    if (xOn) raw = applyExtras(raw, noise);
    age++;
    // §6.4 idle: only after 50 ms of CONTINUOUS silence (a per-sample test would
    // kill a sine at its first zero crossing).
    if (fabsf(raw) < 0.0002f) { if (++quietRun > 2400u) state = VS_IDLE; }
    else quietRun = 0;
    return raw;
}

// ==================================================================== engine depth stage + generative FM
// applyExtras: SNAP (HP-noise transient) + BODY (sine layer) + DRIVE (saturation) + CRUSH (sample-hold + bit
// reduction). Runs ONLY when one of the four is > 0 (xOn): the classic kit never enters it.
float RefVoice::applyExtras(float raw, float noise) {
    float out = raw;
    if (xSnap > 0.f) {
        xSnapLp += 0.35f * (noise - xSnapLp);
        out += xSnap * 0.6f * xSnapEnv * (noise - xSnapLp) * accent;     // one-pole HP of the shared noise
        xSnapEnv *= xSnapCoef;
    }
    if (xBody > 0.f) {
        xBodyPh += DRUM_TWOPI * xBodyHz * DRUM_FS_INV;
        if (xBodyPh > DRUM_TWOPI) xBodyPh -= DRUM_TWOPI;
        float tau = totalLen * 0.7f; if (tau < 0.02f) tau = 0.02f;
        float e = fastExp(-6.9078f * ((float)age * DRUM_FS_INV) / tau);
        out += xBody * 0.7f * sinf(xBodyPh) * e * accent;
    }
    if (xDrive > 0.f) out = fxTanhL(out * (1.f + 6.f * xDrive)) * (1.f / (1.f + 0.5f * xDrive));
    if (xCrush > 0.f) {
        if (xHoldN == 0u) {
            float L = fastPow2(12.f - 8.f * xCrush);
            xHold = floorf(out * L + 0.5f) / L;
        }
        if (++xHoldN >= xHoldLen) xHoldN = 0u;
        out = xHold;
    }
    return flushBad(out);
}

float RefVoice::procGenFm(const SoundDef* d, float noise) {
    (void)noise;
    float carrier = genExp(gfFc * gfDrop, gfFc, 30.f);
    float idx = genExp(gfIdx, gfIdx * 0.1f, gfDec * 500.f) * (0.5f + 1.f * tone);
    ph1 += DRUM_TWOPI * (carrier * gfRatio) * DRUM_FS_INV;
    if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
    ph0 += DRUM_TWOPI * carrier * DRUM_FS_INV;
    if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
    float y = sinf(ph0 + idx * sinf(ph1));
    float a = env(gfDec);
    ampCache = a;
    return dc.process(shape(y * a * (d->g0 > 0.f ? d->g0 : 1.f), d)) * d->gain;
}

