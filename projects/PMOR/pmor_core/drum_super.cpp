// drum_super.cpp -- the drum synth engine: one layered "super voice" per pad. Layers are verbatim ports of the old per-kind drum routines,
// each gated by a level (0 = skipped = free). A preset (SoundDef) is just a way to fill a SuperDef (superFromDef); the pad then owns the params.
#pragma GCC optimize ("O2")   // the firmware builds -Os (out-of-line env/fastExp/Svf per call); this is the hot per-sample loop of every drum voice
#include <string.h>
#include "drum_super.h"
#include "shared_state.h"

// ================================================================= trigger
void SuperVoice::trigger(const SuperDef& din, const DrumPadCfg& cfg, float vel, uint8_t grp, uint8_t idxGrp, uint32_t& seed) {
    (void)idxGrp;
    if (&din != &dd) dd = din;
    const SuperDef& d = dd;
    def = &dd; group = grp; age = 0;
    lenRatio = fastPow2((clampf(cfg.length, 0.f, 1.f) - 0.5f) * 4.f);
    lenMul = 1.f + d.lenTrack * (lenRatio - 1.f);
    reverse = cfg.reverse;
    accent = 0.5f + 0.5f * clampf(vel, 0.f, 1.f);
    tone = clampf(cfg.tone, 0.f, 1.f);
    pRatio = fastPow2(clampf(cfg.pitch, -1.f, 1.f));

    // ---- base pitch (draw order identical to DrumVoice::trigger: degree, octJit | blip | genfm rolls | pan)
    float fKey = scaleHz(0, 0) * 0.25f * pRatio * d.tune;
    float fTom = 0.f, fFix = d.f0 * pRatio;
    if (d.pTom > 0.f) { int degree = scaleTomDegree(lcg01(seed)); int octJit = (int)(lcg(seed) & 1u); fTom = scaleHz(degree, (int)d.tomOct + octJit) * pRatio; }
    if (d.rndMix > 0.f) { float r = (300.f + (float)(lcg(seed) % 2100u)) * pRatio; fFix = mixf(fFix, r, d.rndMix); }
    if (d.pKey >= 1.f) fBase = fKey; else if (d.pTom >= 1.f) fBase = fTom; else if (d.pFix >= 1.f) fBase = fFix;
    else {
        float l = 0.f;
        if (d.pKey > 0.f) l += d.pKey * logf(fKey);
        if (d.pTom > 0.f) l += d.pTom * logf(fTom);
        if (d.pFix > 0.f) l += d.pFix * logf(fFix > 1.f ? fFix : 1.f);
        fBase = expf(l);
    }
    if (fBase < 10.f) fBase = 10.f;

    toneHz = 400.f * fastPow2(tone * 5.129283f);

    // ---- FM effective values (nominal blended with the per-hit roll)
    float nomLen = d.lenNom;
    eFc = fBase; eRatio = d.fRatio; eIdx0 = d.fIdx0; eIdx1 = d.fIdx1; eMs = d.fMs; eT60 = d.fT60; eBump = d.fBump; eClick = d.fClickMs;
    if (d.fGen > 0.f) {
        static const float kRatios[8] = { 1.f, 1.41421f, 1.5f, 2.f, 2.41421f, 3.f, 3.73205f, 5.19615f };
        float gfRatio = kRatios[lcg(seed) & 7u];
        float gfFc = fBase * fastPow2((lcg01(seed) * 2.f - 1.f) * d.fChaosP * 1.5f);
        float gfIdx = d.fIdx0 * (1.f - 0.6f * d.fChaosI + 0.8f * d.fChaosI * lcg01(seed));
        float gfDec = d.fT60 * fastPow2((lcg01(seed) * 2.f - 1.f) * d.fChaosD);
        float gfDrop = 1.f + d.fChaosDrop * 3.f * lcg01(seed);
        float m = d.fGen;
        eRatio = mixf(d.fRatio, gfRatio, m); eFc = mixf(fBase, gfFc, m); eIdx0 = mixf(d.fIdx0, gfIdx, m); eIdx1 = mixf(d.fIdx1, gfIdx * 0.1f, m);
        eMs = mixf(d.fMs, gfDec * 500.f, m); eT60 = mixf(d.fT60, gfDec, m); eBump = mixf(d.fBump, gfDrop, m); eClick = mixf(d.fClickMs, 30.f, m);
        nomLen = mixf(d.lenNom, gfDec + 0.01f, m);
    }
    if (nomLen < 0.02f) nomLen = 0.02f;
    totalLen = nomLen * lenRatio;

    float p = d.pan; if (d.panRnd > 0.f) p += d.panRnd * lcg01(seed);
    panL = cosf(p * 1.57079633f); panR = sinf(p * 1.57079633f);

    for (int i = 0; i < 4; i++) { ry1[i] = ry2[i] = 0.f; rR[i] = 0.f; rCw[i] = 1.f; rNorm[i] = 1.f; }
    quietRun = 0; ph0 = ph1 = mph0 = mph1 = fph0 = fph1 = 0.f; iph0 = cph0 = cph1 = 0u; ampCache = 0.f; hpLp = 0.f;
    svfN = svfCl = svfM1 = svfM2 = svfMH = svfCb1 = svfCb2 = svfV = svfL = svfT = SvfC();
    for (int i = 0; i < 4; i++) { rLastF[i] = -1.f; rLastT[i] = -1.f; }
    dc = DcBlock(); crushHold = 0.f; crushPhase = 0.f; crushLevels = fastPow2(d.crushBits);

    {
        const DrumEngCfg& ec = drums.eng[grp < DG_COUNT ? grp : 0];
        xSnap = clampf(d.xSnap + ec.x[0], 0.f, 1.f); xBody = clampf(d.xBody + ec.x[1], 0.f, 1.f);
        xDrive = clampf(d.xDrive + ec.x[2], 0.f, 1.f); xCrush = clampf(d.xCrush + ec.x[3], 0.f, 1.f);
        xOn = (xSnap > 0.f) || (xBody > 0.f) || (xDrive > 0.f) || (xCrush > 0.f);
        xSnapEnv = 1.f; xSnapLp = 0.f; xBodyPh = 0.f; xHold = 0.f; xHoldN = 0u;
        xSnapCoef = fastExp(-1.f / (d.xSnapMs * 0.001f * SR));
        xBodyHz = d.xBodyFix > 0.f ? d.xBodyFix * pRatio : fBase * d.xBodyRatio;
        xHoldLen = 1u + (uint32_t)(xCrush * 14.f);
    }
    state = VS_ACTIVE;
}

// ================================================================= layers
float SuperVoice::noiseLayer(float noise) {
    const SuperDef* d = def;
    if (d->nLvl <= 0.f) return 0.f;
    float holdS = d->nHold * 0.001f;
    float a = env(d->nT60, holdS);
    float atkS = d->nAtk * 0.001f; float tNow = (float)age * DRUM_FS_INV;
    if (!reverse && atkS > 0.f && tNow < atkS) a *= (tNow / atkS);
    float vf = mixf(1.f, accent, d->nVel);
    float x = noise;
    if (d->nHp1 > 0.f) { hpLp += (x - hpLp) * d->nHp1; x -= hpLp; }
    if (d->nF <= 0.f) return x * a * d->nLvl * vf;
    float fc = d->nF;
    if (d->nSweepTo > 0.f) fc = genExp(d->nF, d->nSweepTo, d->nSweepMs);
    if (age == 0u || (d->nSweepTo > 0.f && (age & 7u) == 0u)) svfN.set(fc, d->nQ);
    float lp, bp, hp; svfN.tick(x, lp, bp, hp);
    float band = (d->nBp >= 1.f) ? bp : (d->nBp <= 0.f) ? hp : mixf(hp, bp, d->nBp);
    return band * a * d->nLvl * vf;
}

float SuperVoice::render(float metal, float noise) {
    const SuperDef* d = def;
    tNowS = (float)age * DRUM_FS_INV; tEffS = reverse ? (totalLen - tNowS) : tNowS; if (tEffS < 0.f) tEffS = 0.f;
    float out = 0.f;

    // ---------------- RES (resonators 0..2)
    if (d->rG[0] > 0.f) {
        float freq0 = fBase;
        if (d->rClickMs > 0.f) freq0 = genExp(fBase * d->rBump, fBase, d->rClickMs);
        float freq0n = freq0;                                   // click-following base for res1 (DUAL ping)
        if (d->rSelf > 0.f) freq0 *= (1.f + d->rSelf * 0.08f * ampCache);
        setReson(0, freq0, d->rT[0]);
        float exc = pingExc(accent);
        float y0 = tickReson(0, exc);
        float a0 = env(d->rT[0]); ampCache = a0;
        out += y0 * a0 * d->rG[0];
        if (d->rG[1] > 0.f) {
            float b = mixf(fBase, freq0n, d->rFollow);
            setReson(1, b * d->rF1, d->rT[1]);
            out += tickReson(1, exc) * env(d->rT[1]) * d->rG[1];
        }
        if (d->rG[2] > 0.f) {
            setReson(2, fBase * d->rF2, d->rT[2]);
            out += tickReson(2, exc) * env(d->rT[2]) * d->rG[2];
        }
    }

    // ---------------- GLITCH
    if (d->gG > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        float gap = d->gGapMs * 0.001f * lenRatio;
        uint8_t n = d->gN >= 1.f ? (uint8_t)d->gN : 1;
        uint8_t idx = (uint8_t)(tNow / gap); if (idx >= n) idx = n - 1;
        float tLocal = tNow - idx * gap;
        float mul = 1.f; for (uint8_t i = 0; i < idx; i++) mul *= d->gMul;
        float o;
        if (d->gNoise > 0.5f) { float e = fastExp(-tLocal / (0.006f * lenRatio)); o = noise * e; }
        else {
            setReson(3, fBase, d->gT);
            float exc = (tLocal < 0.0008f) ? accent * fastExp(-tLocal / 0.0003f) : 0.f;
            float y = tickReson(3, exc);
            float e = fastExp(-tLocal / (d->gT * lenRatio * 0.166096f));
            o = y * e;
        }
        out += o * mul * d->gG; ampCache = mul;
    }

    // ---------------- OSC
    if (d->oG0 > 0.f) {
        float freq;
        if (d->oLog <= 0.f) freq = genExp(fBase * d->oStartR, fBase, d->oMs);
        else if (d->oLog >= 1.f) freq = genLog(fBase * d->oStartR, fBase, d->oMs);
        else freq = mixf(genExp(fBase * d->oStartR, fBase, d->oMs), genLog(fBase * d->oStartR, fBase, d->oMs), d->oLog);
        if (d->oVibHz > 0.f) { float tNow = (float)age * DRUM_FS_INV; freq *= (1.f + d->oVibDepth * sinf(DRUM_TWOPI * d->oVibHz * tNow)); }
        float a = env(d->oT0); if (d->rG[0] <= 0.f) ampCache = a;
        float wSine = 1.f - d->oRound - d->oSquare;
        float y = 0.f;
        if (wSine > 0.f || d->oRound > 0.f) {
            ph0 += DRUM_TWOPI * freq * DRUM_FS_INV; if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
            float s = sinf(ph0);
            if (d->oPartMix > 0.f) { float s2 = 0.6f * s + 0.45f * sinf(ph0 * d->oPart); s = mixf(s, s2, d->oPartMix); }
            y += wSine * s;
            if (d->oRound > 0.f) y += d->oRound * (2.f * s / (1.f + fabsf(s)));
        }
        if (d->oSquare > 0.f) {
            uint32_t inc = (uint32_t)((freq * DRUM_FS_INV) * 4294967296.f); iph0 += inc;
            y += d->oSquare * ((iph0 & 0x80000000u) ? -1.f : 1.f);
        }
        if (d->oAmHz > 0.f) { float tNow = (float)age * DRUM_FS_INV; y *= 1.f - d->oAmDepth * (0.5f - 0.5f * sinf(DRUM_TWOPI * d->oAmHz * tNow)); }
        float o = y * a * d->oG0;
        if (d->oVel > 0.f) o *= mixf(1.f, accent, d->oVel);
        if (d->oG1 > 0.f) {
            float f1 = genExp(fBase * d->oBStart, fBase * d->oF1r, d->oBMs);
            ph1 += DRUM_TWOPI * f1 * DRUM_FS_INV; if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
            float s = sinf(ph1);
            float yb = d->oBRound > 0.f ? mixf(s, 2.f * s / (1.f + fabsf(s)), d->oBRound) : s;
            o += yb * env(d->oT1) * d->oG1;
        }
        if (d->vcfMix > 0.f) {
            float fc = genExp(d->vcfStart, d->vcfEnd, d->oMs);
            if (age == 0u || (age & 7u) == 0u) svfV.set(fc, d->vcfQ);
            float lp, bp, hp; svfV.tick(o, lp, bp, hp); o = mixf(o, lp, d->vcfMix);
        }
        if (d->lpMix > 0.f) {
            float fc = freq * d->lpMult;
            if (age == 0u || (age & 7u) == 0u) svfL.set(fc, 0.9f);
            float lp, bp, hp; svfL.tick(o, lp, bp, hp); o = mixf(o, lp, d->lpMix);
        }
        out += o;
    }

    // ---------------- FM
    if (d->fG > 0.f) {
        float carrier = eFc;
        if (eClick > 0.f) carrier = genExp(eFc * eBump, eFc, eClick);
        float idx = genExp(eIdx0, eIdx1, eMs) * (0.5f + 1.f * tone);
        fph1 += DRUM_TWOPI * (carrier * eRatio) * DRUM_FS_INV; if (fph1 > DRUM_TWOPI) fph1 -= DRUM_TWOPI;
        float mod = sinf(fph1);
        fph0 += DRUM_TWOPI * carrier * DRUM_FS_INV; if (fph0 > DRUM_TWOPI) fph0 -= DRUM_TWOPI;
        float y = sinf(fph0 + idx * mod);
        float a = env(eT60); ampCache = a;
        out += y * a * d->fG;
    }

    // ---------------- NOISE
    if (d->nLvl > 0.f) {
        float nz = noiseLayer(noise);
        if (d->nJit > 0.f) nz *= mixf(1.f, 0.9f + 0.2f * lcg01(rnd), d->nJit);
        out += nz;
    }

    // ---------------- CLAP
    if (d->cG > 0.f) {
        float tNow = (float)age * DRUM_FS_INV; float tRef = reverse ? (totalLen - tNow) : tNow; if (tRef < 0.f) tRef = 0.f;
        if (age == 0u) svfCl.set(d->cF, d->cQ);
        float lp, bp, hp; svfCl.tick(noise, lp, bp, hp); float band = bp;
        uint8_t n = d->cN >= 1.f ? (uint8_t)d->cN : 1;
        float burstTau = d->cDur * 0.001f * 0.35f; float burstEnv = 0.f;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t bi = reverse ? (uint8_t)(n - 1 - i) : i;
            float dt = tRef - bi * (d->cGap * 0.001f);
            if (dt >= 0.f) { float e = fastExp(-dt / burstTau); if (e > burstEnv) burstEnv = e; }
        }
        if (reverse) { float remain = totalLen - tNow; if (remain < 0.005f) { float gr = remain > 0.f ? remain / 0.005f : 0.f; burstEnv *= 0.5f - 0.5f * cosf(DRUM_PI * gr); } }   // REVERSE ends: same 5 ms release as env()
    float tail = env(d->cTailT); float atk = d->cTailAtk * 0.001f;
        if (!reverse && atk > 0.f && tNow < atk) tail *= (tNow / atk);
        out += d->cG * (band * (burstEnv + tail * d->cTailLvl));
    }

    // ---------------- COWBELL
    if (d->bG > 0.f) {
        float f1 = fBase, f2 = fBase * d->bF2r;
        cph0 += (uint32_t)((f1 * DRUM_FS_INV) * 4294967296.f); cph1 += (uint32_t)((f2 * DRUM_FS_INV) * 4294967296.f);
        float sum = (((cph0 & 0x80000000u) ? -1.f : 1.f) + ((cph1 & 0x80000000u) ? -1.f : 1.f)) * 0.5f;
        float q = d->bQ * (0.5f + 1.f * tone);
        if (age == 0u) { svfCb1.set(f1, q); svfCb2.set(f2, q); }
        float lp, bp, hp; svfCb1.tick(sum, lp, bp, hp); float b1 = bp; svfCb2.tick(sum, lp, bp, hp); float b2 = bp;
        float band = (b1 + b2) * 0.5f;
        float envv = 0.6f * env(d->bTA) + 0.4f * env(d->bTB);
        out += drumSwing(band, d->bGain) * envv;
    }

    // ---------------- METAL
    if (d->mG > 0.f) {
        float sig = metal; if (d->mNoiseMix > 0.f) sig = sig * (1.f - d->mNoiseMix) + noise * d->mNoiseMix;
        float lp, bp, hp;
        if (age == 0u) svfM1.set(d->mBp1, d->mBp1Q);
        svfM1.tick(sig, lp, bp, hp); float band1 = bp;
        if (age == 0u) svfM2.set(d->mBp2, d->mBp2Q);
        svfM2.tick(sig, lp, bp, hp); float band2 = bp;
        float hpCut = (d->mTone >= 1.f) ? d->mHpF * (0.4f + 1.2f * tone) : d->mHpF * (1.f + d->mTone * (1.2f * tone - 0.6f));   // mTone = how much TONE moves the HP corner (1 = old)
        if (age == 0u || (age & 7u) == 0u) svfMH.set(hpCut, d->mHpQ);
        float src = mixf(band1 + band2, band2, d->mTwo);
        svfMH.tick(src, lp, bp, hp); float hiPart = hp;
        float envA = env(d->mTA); ampCache = envA;
        float o;
        if (d->mTwo >= 1.f) {
            float envB = env(d->mTB > 0.f ? d->mTB : d->mTA * 0.6f);
            o = drumSwing(band1, d->mgA) * envA + drumSwing(hiPart, d->mgB) * envB;
        } else if (d->mTwo <= 0.f) {
            o = drumSwing(hiPart, d->mgA) * envA;
        } else {
            float envB = env(d->mTB > 0.f ? d->mTB : d->mTA * 0.6f);
            o = mixf(drumSwing(hiPart, d->mgA) * envA, drumSwing(band1, d->mgA) * envA + drumSwing(hiPart, d->mgB) * envB, d->mTwo);
        }
        if (d->pingLvl > 0.f) {
            mph0 += DRUM_TWOPI * d->pingF0 * DRUM_FS_INV; if (mph0 > DRUM_TWOPI) mph0 -= DRUM_TWOPI;
            mph1 += DRUM_TWOPI * d->pingF1 * DRUM_FS_INV; if (mph1 > DRUM_TWOPI) mph1 -= DRUM_TWOPI;
            o += (sinf(mph0) + sinf(mph1)) * 0.5f * env(d->pingT) * d->pingLvl;
        }
        if (d->d2Ms > 0.f) {
            float tNow = (float)age * DRUM_FS_INV; float t0 = d->d2Ms * 0.001f;
            if (tNow >= t0) { float e2 = fastExp(-(tNow - t0) / (d->mTA * 0.166096f)); o += hiPart * e2 * d->d2Lvl; }
        }
        out += o * d->mG;
    }

    // ================= output stage
    float u = out;
    if (d->crushMix > 0.f) {
        crushPhase += d->crushHz * DRUM_FS_INV;
        if (crushPhase >= 1.f) { crushPhase -= 1.f; crushHold = floorf(u * crushLevels + 0.5f) / crushLevels; }
        u = mixf(u, crushHold, d->crushMix);
    }
    if (d->driveMix > 0.f) u = mixf(u, drumSwing(u, 1.f + d->drive), d->driveMix);
    if (d->foldMix > 0.f) {
        float y = u;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        u = 0.7f * d->foldMix * y + (1.f - 0.7f * d->foldMix) * u;
    }
    if (d->dcR < 1.f) {
        float y = u - dc.x1 + d->dcR * dc.y1; dc.x1 = u; dc.y1 = flushDenorm(y); u = dc.y1;
    }
    if (d->toneMix > 0.f) {
        if (age == 0u) svfT.set(toneHz, 0.7f);
        float lp, bp, hp; svfT.tick(u, lp, bp, hp);
        float t = mixf(lp, hp, d->toneHp);
        u = mixf(u, t, d->toneMix);
    }
    if (d->satMix > 0.f) u = mixf(u, sTanh(u * d->satIn), d->satMix);
    return flushBad(u * d->gain);
}

float SuperVoice::applyExtras(float raw, float noise) {
    float out = raw;
    if (xSnap > 0.f) { xSnapLp += 0.35f * (noise - xSnapLp); out += xSnap * 0.6f * xSnapEnv * (noise - xSnapLp) * accent; xSnapEnv *= xSnapCoef; }
    if (xBody > 0.f) {
        xBodyPh += DRUM_TWOPI * xBodyHz * DRUM_FS_INV; if (xBodyPh > DRUM_TWOPI) xBodyPh -= DRUM_TWOPI;
        float tau = totalLen * 0.7f; if (tau < 0.02f) tau = 0.02f;
        float e = fastExp(-6.9078f * ((float)age * DRUM_FS_INV) / tau);
        out += xBody * 0.7f * sinf(xBodyPh) * e * accent;
    }
    if (xDrive > 0.f) out = sTanh(out * (1.f + 6.f * xDrive)) * (1.f / (1.f + 0.5f * xDrive));
    if (xCrush > 0.f) {
        if (xHoldN == 0u) { float L = fastPow2(12.f - 8.f * xCrush); xHold = floorf(out * L + 0.5f) / L; }
        if (++xHoldN >= xHoldLen) xHoldN = 0u;
        out = xHold;
    }
    return flushBad(out);
}

float SuperVoice::process(float metal, float noise) {
    if (state == VS_IDLE) return 0.f;
    if (state == VS_CHOKE) {
        float raw = render(metal, noise);
        if (xOn) raw = applyExtras(raw, noise);
        float g = (chokeTotal > 0u) ? (0.5f - 0.5f * cosf(DRUM_PI * ((float)chokeLeft / (float)chokeTotal))) : 0.f;
        float out = raw * g; age++;
        if (chokeLeft > 0u) chokeLeft--;
        if (chokeLeft == 0u) state = VS_IDLE;
        return out;
    }
    float raw = render(metal, noise);
    if (xOn) raw = applyExtras(raw, noise);
    age++;
    if (fabsf(raw) < 0.0002f) { if (++quietRun > 2400u) state = VS_IDLE; } else quietRun = 0;
    return raw;
}

// ================================================================= SoundDef -> SuperDef (the exact-reproduction mapping)
void superFromDef(SuperDef& out, const SoundDef& d, int grp, int idxInGrp) {
    SuperDef s;
    const bool legacy = d.kind == VK_LEGACY, metal = d.kind == VK_METAL;
    auto g0e = [&](float g) { return g > 0.f ? g : 1.f; };
    s.f0 = d.f0;
    // pitch routing
    s.pKey = d.pmode == PM_KEYROOT ? 1.f : 0.f; s.pTom = d.pmode == PM_TOMDEG ? 1.f : 0.f; s.pFix = d.pmode == PM_FIXED ? 1.f : 0.f;
    s.tomOct = d.tomOct;
    if (legacy && d.pmode == PM_KEYROOT) s.tune = d.f0 / 52.f;
    if (legacy && d.pmode == PM_FIXED && d.f0 <= 1.f) s.rndMix = 1.f;
    // nominal length: the old per-kind rule, kept as data
    {
        float nom;
        switch (d.kind) {
            case VK_RESON: nom = d.t60_0; if (d.f1r > 0.f && d.t60_1 > nom) nom = d.t60_1; if (d.f2r > 0.f && d.t60_2 > nom) nom = d.t60_2; if (d.nLvl > 0.f && d.nT60 > nom) nom = d.nT60; break;
            case VK_DUAL: nom = d.t60_0; if (d.t60_1 > nom) nom = d.t60_1; { float nl = d.nT60 + d.nHoldMs * 0.001f; if (nl > nom) nom = nl; } break;
            case VK_SWEEP: nom = d.t60_0; if (d.sweepMs * 0.001f > nom) nom = d.sweepMs * 0.001f; break;
            case VK_CLAP: nom = d.tailT60 + d.bN * d.bGapMs * 0.001f; break;
            case VK_NOISE: nom = d.nT60 + d.nHoldMs * 0.001f; break;
            case VK_METAL: nom = d.t60_0 > d.t60_1 ? d.t60_0 : d.t60_1; break;
            case VK_RING: nom = d.t60_0; break;
            case VK_FM: nom = d.fmMs * 0.001f + d.t60_0; break;
            case VK_COWBELL: nom = d.t60_1; break;
            case VK_GLITCH: nom = d.t60_0 + d.retrigN * d.retrigGapMs * 0.001f; break;
            case VK_LEGACY: nom = logf(0.0008f) / logf(d.lDec) * DRUM_FS_INV; break;
            default: nom = 0.3f; break;
        }
        s.lenNom = nom;
    }
    // output stage
    s.gain = d.gain;
    s.dcR = legacy ? 1.f : 0.995f;
    if (!legacy && !metal) { s.toneMix = 1.f; s.toneHp = grp == DG_HAT ? 1.f : 0.f; if (d.satDrive > 0.f) { s.satMix = 1.f; s.satIn = d.satDrive; } }
    if (legacy) { s.satMix = 1.f; s.satIn = 3.f; }
    if (!legacy && !metal) { if (d.drive > 0.f) { s.driveMix = 1.f; s.drive = d.drive; } if (d.fold) s.foldMix = 1.f; }
    if ((d.kind == VK_SWEEP || metal) && d.crush) { s.crushMix = 1.f; s.crushBits = d.crushBits; s.crushHz = d.crushHz; }
    s.xSnap = d.xSnap; s.xBody = d.xBody; s.xDrive = d.xDrive; s.xCrush = d.xCrush;
    { static const float kSnapMs[DG_COUNT] = { 3.f, 8.f, 2.f, 5.f }; s.xSnapMs = kSnapMs[grp]; if (grp == DG_HAT) s.xBodyFix = 4200.f; }
    // pad pan (was hardcoded by group; hat by idxInGrp is applied by the caller)
    s.pan = 0.5f; if (grp == DG_PERC) { s.pan = 0.25f; s.panRnd = 0.5f; }
    if (grp == DG_HAT) s.pan = (idxInGrp == 0) ? 0.25f : (idxInGrp == 1) ? 0.75f : 0.5f;
    // noise layer fields common to every kind that uses noiseLayer()
    auto noiseFromDef = [&]() { s.nLvl = d.nLvl; s.nF = d.nF; s.nQ = d.nQ; s.nBp = d.nBp ? 1.f : 0.f; s.nSweepTo = d.nSweepTo; s.nSweepMs = d.sweepMs > 0.f ? d.sweepMs : 60.f; s.nT60 = d.nT60; s.nHold = d.nHoldMs; s.nAtk = d.nAtkMs; };
    switch (d.kind) {
        case VK_RESON:
            s.rG[0] = g0e(d.g0); s.rT[0] = d.t60_0; s.rClickMs = d.clickMs; s.rBump = d.clickBump; s.rSelf = d.selfFm;
            if (d.f1r > 0.f) { s.rF1 = d.f1r; s.rG[1] = d.g1; s.rT[1] = d.t60_1; }
            if (d.f2r > 0.f) { s.rF2 = d.f2r; s.rG[2] = d.g2; s.rT[2] = d.t60_2; }
            noiseFromDef(); break;
        case VK_DUAL:
            if (d.useOsc) {
                s.oG0 = g0e(d.g0); s.oT0 = d.t60_0; s.oRound = 1.f; s.oStartR = d.clickMs > 0.f ? d.clickBump : 1.f; s.oMs = d.clickMs > 0.f ? d.clickMs : 1.f;
                if (d.f1r > 0.f) { s.oG1 = d.g1; s.oT1 = d.t60_1; s.oF1r = d.f1r; s.oBStart = d.f1r * s.oStartR; s.oBMs = s.oMs; s.oBRound = 1.f; }
            } else {
                s.rG[0] = g0e(d.g0); s.rT[0] = d.t60_0; s.rClickMs = d.clickMs; s.rBump = d.clickBump; s.rFollow = 1.f;
                if (d.f1r > 0.f) { s.rF1 = d.f1r; s.rG[1] = d.g1; s.rT[1] = d.t60_1; }
            }
            noiseFromDef(); break;
        case VK_SWEEP:
            s.oG0 = g0e(d.g0); s.oT0 = d.t60_0; s.oStartR = d.sweepStartR; s.oMs = d.sweepMs; s.oLog = d.sweepLog ? 1.f : 0.f; s.oSquare = d.useSquare ? 1.f : 0.f;
            s.oAmHz = d.amHz; s.oAmDepth = d.amDepth; s.oVibHz = d.vibHz; s.oVibDepth = d.vibDepth;
            if (d.f1r > 0.f) { s.oG1 = d.g1; s.oT1 = d.t60_1; s.oF1r = d.f1r; s.oBStart = d.sweepStartR * 0.8f; s.oBMs = d.sweepMs * 0.8f; }
            if (d.vcf) { s.vcfMix = 1.f; s.vcfStart = d.vcfStartHz; s.vcfEnd = d.vcfEndHz; s.vcfQ = d.vcfQ; }
            if (d.lpTrackMult > 0.f) { s.lpMix = 1.f; s.lpMult = d.lpTrackMult; }
            noiseFromDef(); break;
        case VK_CLAP:
            s.cG = 1.f; s.cN = d.bN > 0 ? d.bN : 1; s.cGap = d.bGapMs; s.cDur = d.bDurMs; s.cF = d.nF; s.cQ = d.nQ; s.cTailT = d.tailT60; s.cTailAtk = d.tailAtkMs; s.cTailLvl = d.tailLvl; break;
        case VK_NOISE: noiseFromDef(); s.nJit = 1.f; break;
        case VK_METAL:
            s.mG = 1.f; s.mTA = d.t60_0; s.mTB = d.t60_1; s.mgA = d.g0; s.mgB = d.g1; s.mTwo = d.twoBand ? 1.f : 0.f;
            s.mBp1 = d.bp1; s.mBp1Q = d.bp1Q; s.mBp2 = d.bp2; s.mBp2Q = d.bp2Q; s.mHpF = d.hpF; s.mHpQ = d.hpQ; s.mNoiseMix = d.noiseMix;
            s.pingF0 = d.pingF0; s.pingF1 = d.pingF1; s.pingT = d.pingT60; s.pingLvl = d.pingLvl; s.d2Ms = d.delay2Ms; s.d2Lvl = d.delay2Lvl; break;
        case VK_FM:
            s.fG = g0e(d.g0); s.fRatio = d.fmRatio; s.fIdx0 = d.fmIdx0; s.fIdx1 = d.fmIdx1; s.fMs = d.fmMs; s.fT60 = d.t60_0; s.fBump = d.clickBump; s.fClickMs = d.clickMs;
            noiseFromDef(); break;
        case VK_GENFM:
            s.fG = g0e(d.g0); s.fIdx0 = d.fmIdx0; s.fT60 = d.t60_0; s.fGen = 1.f; s.fChaosP = s.fChaosI = s.fChaosD = s.fChaosDrop = d.gfChaos; break;
        case VK_COWBELL:
            s.bG = 1.f; s.bF2r = d.f1r > 0.f ? d.f1r : 1.481f; s.bQ = d.nQ > 0.f ? d.nQ : 5.f; s.bTA = d.t60_0; s.bTB = d.t60_1; s.bGain = d.g0 > 0.f ? d.g0 : 1.5f; break;
        case VK_GLITCH:
            s.gG = 1.f; s.gN = d.retrigN > 0 ? d.retrigN : 1; s.gGapMs = d.retrigGapMs; s.gMul = d.retrigMul; s.gNoise = d.retrigNoise ? 1.f : 0.f; s.gT = d.t60_0; break;
        case VK_LEGACY: {
            float tau = -1.f / (logf(d.lDec) * SR);            // seconds
            float t60 = 6.9078f * tau;
            s.lenTrack = 1.f; s.atkLin = 1.f;
            s.oG0 = (1.f - d.lNoiz) * d.lAmp; s.oT0 = t60; s.oStartR = d.lDrop; s.oMs = 86.805556f; s.oVel = 1.f;
            if (d.lPart > 0.f) { s.oPartMix = 1.f; s.oPart = d.lPart; }
            s.nLvl = d.lNoiz * d.lAmp; s.nF = 0.f; s.nT60 = t60; s.nAtk = 0.f; s.nVel = 1.f; s.nHp1 = d.lNoiz > 0.97f ? 0.12f : 0.f;
            break;
        }
        default: break;
    }
    s.lenTrack = 1.f;   // LENGTH scales every kind's decay (63ea3f9)
    out = s;
}

// ================================================================= parameter table (every SuperDef field, in schema order: idx = k within the pad's 256-id slot)
const char* const SDEF_LAYER_NAMES[SL_COUNT] = { "PITCH", "TIME", "RES", "GLITCH", "OSC", "NOISE", "CLAP", "METAL", "COWBELL", "FM", "OUT", "DEPTH", "PAN" };
#define SP(nm, fld, mn, mx, fl, L) { nm, (uint16_t)(__builtin_offsetof(SuperDef, fld) / sizeof(float)), mn, mx, (uint8_t)(fl), (uint8_t)(L) }
const SdParamInfo SDEF_PARAMS[] = {
    SP("F0", f0, 0.f, 12000.f, 0, SL_PITCH), SP("PKEY", pKey, 0.f, 1.f, 0, SL_PITCH), SP("PTOM", pTom, 0.f, 1.f, 0, SL_PITCH), SP("PFIX", pFix, 0.f, 1.f, 0, SL_PITCH),
    SP("TOMOCT", tomOct, 0.f, 4.f, PF_QUANT, SL_PITCH), SP("TUNE", tune, 0.25f, 8.f, PF_LOG, SL_PITCH), SP("RNDPITCH", rndMix, 0.f, 1.f, 0, SL_PITCH),
    SP("ATKMS", atkMs, 0.f, 50.f, 0, SL_TIME), SP("ATKLIN", atkLin, 0.f, 1.f, 0, SL_TIME), SP("LENTRK", lenTrack, 0.f, 1.f, 0, SL_TIME), SP("LENNOM", lenNom, 0.02f, 8.f, PF_LOG, SL_TIME),
    SP("RG0", rG[0], 0.f, 2.f, 0, SL_RES), SP("RG1", rG[1], 0.f, 2.f, 0, SL_RES), SP("RG2", rG[2], 0.f, 2.f, 0, SL_RES),
    SP("RT0", rT[0], 0.005f, 4.f, PF_LOG, SL_RES), SP("RT1", rT[1], 0.005f, 4.f, PF_LOG, SL_RES), SP("RT2", rT[2], 0.005f, 4.f, PF_LOG, SL_RES),
    SP("RF1", rF1, 0.f, 8.f, 0, SL_RES), SP("RF2", rF2, 0.f, 8.f, 0, SL_RES), SP("RCLICKMS", rClickMs, 0.f, 200.f, 0, SL_RES), SP("RBUMP", rBump, 1.f, 8.f, 0, SL_RES),
    SP("RSELF", rSelf, 0.f, 1.f, 0, SL_RES), SP("RFOLLOW", rFollow, 0.f, 1.f, 0, SL_RES),
    SP("GG", gG, 0.f, 2.f, 0, SL_GLITCH), SP("GN", gN, 1.f, 16.f, PF_QUANT, SL_GLITCH), SP("GGAPMS", gGapMs, 1.f, 100.f, 0, SL_GLITCH), SP("GMUL", gMul, 0.f, 1.f, 0, SL_GLITCH),
    SP("GNOISE", gNoise, 0.f, 1.f, 0, SL_GLITCH), SP("GT", gT, 0.01f, 2.f, PF_LOG, SL_GLITCH),
    SP("OG0", oG0, 0.f, 2.f, 0, SL_OSC), SP("OG1", oG1, 0.f, 2.f, 0, SL_OSC), SP("OT0", oT0, 0.005f, 4.f, PF_LOG, SL_OSC), SP("OT1", oT1, 0.005f, 4.f, PF_LOG, SL_OSC),
    SP("OF1R", oF1r, 0.f, 8.f, 0, SL_OSC), SP("OSTARTR", oStartR, 0.25f, 32.f, PF_LOG, SL_OSC), SP("OMS", oMs, 0.5f, 500.f, PF_LOG, SL_OSC), SP("OLOG", oLog, 0.f, 1.f, 0, SL_OSC),
    SP("OROUND", oRound, 0.f, 1.f, 0, SL_OSC), SP("OSQUARE", oSquare, 0.f, 1.f, 0, SL_OSC), SP("OAMHZ", oAmHz, 0.f, 60.f, 0, SL_OSC), SP("OAMDEP", oAmDepth, 0.f, 1.f, 0, SL_OSC),
    SP("OVIBHZ", oVibHz, 0.f, 30.f, 0, SL_OSC), SP("OVIBDEP", oVibDepth, 0.f, 0.5f, 0, SL_OSC), SP("OBSTART", oBStart, 0.25f, 16.f, PF_LOG, SL_OSC), SP("OBMS", oBMs, 0.5f, 500.f, PF_LOG, SL_OSC),
    SP("OBROUND", oBRound, 0.f, 1.f, 0, SL_OSC), SP("OVEL", oVel, 0.f, 1.f, 0, SL_OSC), SP("OPARTMX", oPartMix, 0.f, 1.f, 0, SL_OSC), SP("OPART", oPart, 0.f, 8.f, 0, SL_OSC),
    SP("VCFMIX", vcfMix, 0.f, 1.f, 0, SL_OSC), SP("VCFSTART", vcfStart, 50.f, 16000.f, PF_LOG, SL_OSC), SP("VCFEND", vcfEnd, 50.f, 16000.f, PF_LOG, SL_OSC), SP("VCFQ", vcfQ, 0.3f, 8.f, 0, SL_OSC),
    SP("LPMIX", lpMix, 0.f, 1.f, 0, SL_OSC), SP("LPMULT", lpMult, 0.f, 16.f, 0, SL_OSC),
    SP("NLVL", nLvl, 0.f, 2.f, 0, SL_NOISE), SP("NF", nF, 0.f, 16000.f, 0, SL_NOISE), SP("NQ", nQ, 0.3f, 20.f, 0, SL_NOISE), SP("NBP", nBp, 0.f, 1.f, 0, SL_NOISE),
    SP("NSWEEP", nSweepTo, 0.f, 16000.f, 0, SL_NOISE), SP("NSWMS", nSweepMs, 1.f, 500.f, PF_LOG, SL_NOISE), SP("NT60", nT60, 0.003f, 4.f, PF_LOG, SL_NOISE), SP("NHOLD", nHold, 0.f, 500.f, 0, SL_NOISE),
    SP("NATK", nAtk, 0.f, 50.f, 0, SL_NOISE), SP("NHP1", nHp1, 0.f, 1.f, 0, SL_NOISE), SP("NVEL", nVel, 0.f, 1.f, 0, SL_NOISE), SP("NJIT", nJit, 0.f, 1.f, 0, SL_NOISE),
    SP("CG", cG, 0.f, 2.f, 0, SL_CLAP), SP("CN", cN, 1.f, 12.f, PF_QUANT, SL_CLAP), SP("CGAP", cGap, 1.f, 60.f, 0, SL_CLAP), SP("CDUR", cDur, 1.f, 30.f, 0, SL_CLAP),
    SP("CF", cF, 200.f, 12000.f, PF_LOG, SL_CLAP), SP("CQ", cQ, 0.3f, 10.f, 0, SL_CLAP), SP("CTAILT", cTailT, 0.01f, 2.f, PF_LOG, SL_CLAP), SP("CTAILATK", cTailAtk, 0.f, 50.f, 0, SL_CLAP),
    SP("CTAILLVL", cTailLvl, 0.f, 2.f, 0, SL_CLAP),
    SP("MG", mG, 0.f, 2.f, 0, SL_METAL), SP("MTA", mTA, 0.01f, 4.f, PF_LOG, SL_METAL), SP("MTB", mTB, 0.f, 4.f, 0, SL_METAL), SP("MGA", mgA, 0.f, 4.f, 0, SL_METAL), SP("MGB", mgB, 0.f, 4.f, 0, SL_METAL),
    SP("MTWO", mTwo, 0.f, 1.f, 0, SL_METAL), SP("MBP1", mBp1, 200.f, 16000.f, PF_LOG, SL_METAL), SP("MBP1Q", mBp1Q, 0.3f, 10.f, 0, SL_METAL), SP("MBP2", mBp2, 200.f, 16000.f, PF_LOG, SL_METAL),
    SP("MBP2Q", mBp2Q, 0.3f, 10.f, 0, SL_METAL), SP("MHPF", mHpF, 500.f, 16000.f, PF_LOG, SL_METAL), SP("MHPQ", mHpQ, 0.3f, 5.f, 0, SL_METAL), SP("MNOISE", mNoiseMix, 0.f, 1.f, 0, SL_METAL),
    SP("MTONE", mTone, 0.f, 1.f, 0, SL_METAL), SP("PINGF0", pingF0, 0.f, 16000.f, 0, SL_METAL), SP("PINGF1", pingF1, 0.f, 16000.f, 0, SL_METAL), SP("PINGT", pingT, 0.01f, 4.f, PF_LOG, SL_METAL),
    SP("PINGLVL", pingLvl, 0.f, 2.f, 0, SL_METAL), SP("D2MS", d2Ms, 0.f, 100.f, 0, SL_METAL), SP("D2LVL", d2Lvl, 0.f, 2.f, 0, SL_METAL),
    SP("BG", bG, 0.f, 2.f, 0, SL_COWBELL), SP("BF2R", bF2r, 0.5f, 4.f, 0, SL_COWBELL), SP("BQ", bQ, 0.5f, 20.f, 0, SL_COWBELL), SP("BTA", bTA, 0.005f, 2.f, PF_LOG, SL_COWBELL),
    SP("BTB", bTB, 0.005f, 2.f, PF_LOG, SL_COWBELL), SP("BGAIN", bGain, 0.f, 4.f, 0, SL_COWBELL),
    SP("FG", fG, 0.f, 2.f, 0, SL_FM), SP("FRATIO", fRatio, 0.25f, 16.f, PF_LOG, SL_FM), SP("FIDX0", fIdx0, 0.f, 24.f, 0, SL_FM), SP("FIDX1", fIdx1, 0.f, 24.f, 0, SL_FM),
    SP("FMS", fMs, 1.f, 1000.f, PF_LOG, SL_FM), SP("FT60", fT60, 0.01f, 4.f, PF_LOG, SL_FM), SP("FBUMP", fBump, 1.f, 8.f, 0, SL_FM), SP("FCLICKMS", fClickMs, 0.f, 200.f, 0, SL_FM),
    SP("FGEN", fGen, 0.f, 1.f, 0, SL_FM), SP("FCHAOSP", fChaosP, 0.f, 1.f, 0, SL_FM), SP("FCHAOSI", fChaosI, 0.f, 1.f, 0, SL_FM), SP("FCHAOSD", fChaosD, 0.f, 1.f, 0, SL_FM), SP("FCHAOSR", fChaosDrop, 0.f, 1.f, 0, SL_FM),
    SP("CRUSHMIX", crushMix, 0.f, 1.f, 0, SL_OUT), SP("CRBITS", crushBits, 1.f, 16.f, 0, SL_OUT), SP("CRHZ", crushHz, 1000.f, 48000.f, PF_LOG, SL_OUT), SP("DRIVE", drive, 0.f, 10.f, 0, SL_OUT),
    SP("DRVMIX", driveMix, 0.f, 1.f, 0, SL_OUT), SP("FOLDMIX", foldMix, 0.f, 1.f, 0, SL_OUT), SP("DCR", dcR, 0.9f, 1.f, 0, SL_OUT), SP("TONEMIX", toneMix, 0.f, 1.f, 0, SL_OUT),
    SP("TONEHP", toneHp, 0.f, 1.f, 0, SL_OUT), SP("SATMIX", satMix, 0.f, 1.f, 0, SL_OUT), SP("SATIN", satIn, 0.25f, 10.f, PF_LOG, SL_OUT), SP("GAIN", gain, 0.f, 16.f, 0, SL_OUT),
    SP("XSNAP", xSnap, 0.f, 1.f, 0, SL_DEPTH), SP("XBODY", xBody, 0.f, 1.f, 0, SL_DEPTH), SP("XDRIVE", xDrive, 0.f, 1.f, 0, SL_DEPTH), SP("XCRUSH", xCrush, 0.f, 1.f, 0, SL_DEPTH),
    SP("XSNAPMS", xSnapMs, 0.5f, 30.f, 0, SL_DEPTH), SP("XBODYFX", xBodyFix, 0.f, 16000.f, 0, SL_DEPTH), SP("XBODYRT", xBodyRatio, 0.1f, 4.f, PF_LOG, SL_DEPTH),
    SP("PAN", pan, 0.f, 1.f, 0, SL_PAN), SP("PANRND", panRnd, 0.f, 1.f, 0, SL_PAN),
};
#undef SP
const size_t SDEF_NPARAMS = sizeof(SDEF_PARAMS) / sizeof(SDEF_PARAMS[0]);
static_assert(sizeof(SDEF_PARAMS) / sizeof(SDEF_PARAMS[0]) == sizeof(SuperDef) / sizeof(float), "every SuperDef float must have a SdParamInfo (add it to SDEF_PARAMS)");

// ================================================================= engines: per-group parameter sets + names
const char* const SDEF_ENGINE_NAMES[4] = { "KICK", "SNARE", "HAT", "PERC" };
struct SdRename { uint8_t grp; const char* field; const char* name; };
static const SdRename kRename[] = {
    // KICK: pitch sweep, click, body, sub
    {0,"F0","PITCH"},{0,"TUNE","KEYTUNE"},{0,"OSTARTR","OSCSWEEP"},{0,"OMS","OSCSWPMS"},{0,"OLOG","SWEEPLOG"},{0,"OT0","OSCDEC"},{0,"OG0","OSCBODY"},{0,"OG1","SUB"},{0,"OT1","SUBDEC"},{0,"OF1R","SUBRATIO"},
    {0,"RG0","BODY"},{0,"RT0","BODYDEC"},{0,"RG1","BODY2"},{0,"RT1","BODY2DEC"},{0,"RF1","BODY2RAT"},{0,"RCLICKMS","SWEEPMS"},{0,"RBUMP","SWEEP"},{0,"RSELF","SIGH"},{0,"RFOLLOW","FOLLOW"},
    {0,"NLVL","CLICK"},{0,"NF","CLICKFRQ"},{0,"NT60","CLICKDEC"},{0,"NQ","CLICKQ"},{0,"XSNAP","XCLICK"},{0,"XBODY","XSUB"},
    // SNARE: shell tone, snares (noise), clap bursts
    {1,"F0","SHELLHZ"},{1,"OT0","OSCSHLDC"},{1,"OG0","OSCSHELL"},{1,"OSTARTR","TONESWP"},{1,"OMS","SWEEPMS"},{1,"RG0","SHELL"},{1,"RT0","SHELLDEC"},{1,"RG1","SHELL2"},{1,"RT1","SHELL2DC"},
    {1,"NLVL","SNARES"},{1,"NF","SNRFREQ"},{1,"NQ","SNRQ"},{1,"NT60","SNRDEC"},{1,"NBP","SNRBAND"},{1,"NHOLD","SNRHOLD"},{1,"CG","CLAP"},{1,"CN","CLAPS"},{1,"CGAP","CLAPGAP"},{1,"CF","CLAPFRQ"},{1,"CTAILT","CLAPDEC"},{1,"XSNAP","XSNAP"},{1,"XBODY","XSHELL"},
    // HAT: metallic partials, bands, noise
    {2,"MG","METAL"},{2,"MTA","DECAY"},{2,"MTB","DECAY2"},{2,"MGA","LEVEL1"},{2,"MGB","LEVEL2"},{2,"MTWO","TWOBAND"},{2,"MBP1","BAND1"},{2,"MBP1Q","BAND1Q"},{2,"MBP2","BAND2"},{2,"MBP2Q","BAND2Q"},
    {2,"MHPF","HPFREQ"},{2,"MHPQ","HPQ"},{2,"MNOISE","NOISEMIX"},{2,"MTONE","TONEAMT"},{2,"PINGF0","PING1"},{2,"PINGF1","PING2"},{2,"PINGT","PINGDEC"},{2,"PINGLVL","PING"},{2,"D2MS","CHICKMS"},{2,"D2LVL","CHICK"},
    {2,"NLVL","NOISE"},{2,"NF","NZFREQ"},{2,"NT60","NZDEC"},{2,"XSNAP","XTICK"},{2,"XBODY","XRING"},
    // PERC: tuned body, FM, bell
    {3,"F0","PITCH"},{3,"RG0","BODY"},{3,"RT0","DECAY"},{3,"RG1","PART2"},{3,"RT1","PART2DEC"},{3,"RF1","PART2RAT"},{3,"RG2","PART3"},{3,"RT2","PART3DEC"},{3,"RF2","PART3RAT"},{3,"RCLICKMS","KNOCKMS"},{3,"RBUMP","KNOCK"},
    {3,"OSTARTR","SWEEP"},{3,"OMS","SWEEPMS"},{3,"OT0","OSCDECAY"},{3,"OAMHZ","TREMHZ"},{3,"OAMDEP","TREMDEP"},{3,"OVIBHZ","VIBHZ"},{3,"OVIBDEP","VIBDEP"},
    {3,"BG","BELL"},{3,"BF2R","BELLRAT"},{3,"BQ","BELLQ"},{3,"BTA","BELLDEC"},{3,"BTB","BELLDEC2"},{3,"FG","FM"},{3,"FRATIO","FMRATIO"},{3,"FIDX0","FMIDX"},{3,"FIDX1","FMIDX2"},{3,"FMS","FMIDXDEC"},{3,"FT60","FMDECAY"},
    {3,"FGEN","GENFM"},{3,"FCHAOSP","CHAOSPCH"},{3,"FCHAOSI","CHAOSIDX"},{3,"FCHAOSD","CHAOSDEC"},{3,"FCHAOSR","CHAOSDRP"},{3,"XSNAP","XKNOCK"},{3,"XBODY","XBODY"},
};
static bool s_used[4][160];
static const char* s_names[4][160];
void sdefInitEngines() {
    static const char* const always[] = { "F0","TUNE","PKEY","PTOM","PFIX","ATKMS","LENNOM","LENTRK","DRIVE","DRVMIX","FOLDMIX","DCR","TONEMIX","TONEHP","SATMIX","SATIN","GAIN","CRUSHMIX","CRBITS","CRHZ","XSNAP","XBODY","XDRIVE","XCRUSH","XSNAPMS","XBODYFX","XBODYRT","PAN","PANRND" };
    SuperDef def0; const float* d0 = (const float*)&def0;
    for (int g = 0; g < 4; g++) for (size_t k = 0; k < SDEF_NPARAMS; k++) {
        s_used[g][k] = false; s_names[g][k] = SDEF_PARAMS[k].name;
        for (const char* a : always) if (!strcmp(a, SDEF_PARAMS[k].name)) s_used[g][k] = true;
        for (const SdRename& r : kRename) if (r.grp == g && !strcmp(r.field, SDEF_PARAMS[k].name)) { s_names[g][k] = r.name; s_used[g][k] = true; }
    }
    for (int g = 0; g < 4; g++) for (int s = 0; s < DRUM_SOUNDS; s++) for (int idx = 0; idx < 4; idx++) {
        SuperDef sd; superFromDef(sd, DRUM_DEFS[g][s], g, idx); const float* f = (const float*)&sd;
        for (size_t k = 0; k < SDEF_NPARAMS; k++) if (f[SDEF_PARAMS[k].off] != d0[SDEF_PARAMS[k].off]) s_used[g][k] = true;
    }
}
bool sdefUsed(int g, size_t k) { return g >= 0 && g < 4 && k < SDEF_NPARAMS && s_used[g][k]; }
const char* sdefName(int g, size_t k) { return (g >= 0 && g < 4 && k < SDEF_NPARAMS) ? s_names[g][k] : ""; }
