# Vendored third-party repos (cloned 2026-11-03, read-only; vendor/ is gitignored, re-clone from these hashes)

| dir | origin | branch | commit | licence | ship code? |
|---|---|---|---|---|---|
| vendor/op1-sdk | github.com/BertBrandstifter/op1-sdk | main | ae1945a9c61468f0e0d7cfdd5dd60c2cf9731ff1 (2026-10-21) | MIT ("Copyright (c) 2026 unknown") | yes. Note its m1 sound library (features/m1/build) is NOT MIT, see its NOTICE |
| vendor/op1r-src | github.com/joseph-holland/op1r-src | main | 7cf11afc2bade31f0fb08d3af877b9bb04f970bd (2026-10-18, "op1r 40") | GPL-3.0-or-later + s.7 additional permission to combine with OP-1 firmware (NOTICE) | yes, but derived work must be GPL with source |
| vendor/pcppcp-op1r-src | github.com/pcppcp/op1r-src | main | 805c67de3ae4ef5af769dfffafe6436548a56bb1 (2026-10-29, "op1r 48") | same GPL-3.0+ / NOTICE | as above. DIFFERS from joseph-holland (that is an older fork, op1r 40). Treat pcppcp as upstream |
| vendor/te-op1-cfw | github.com/coldsoldr/te-op1-cfw | dev | aab7a2770996736902a3bb1149b492e8812ceffd (2026-10-30) | NO LICENCE at repo root (only third_party/op1repacker and third_party/kennel carry their own). UNVERIFIED whether README states one | NO: read only, do not ship its code |
| vendor/op1hacks-docs | github.com/op1hacks/docs | master | 05e13a28859bd021fe660245c731b3ad786911bd (2022-09-06) | NO LICENCE file | NO: read only (docs) |
| vendor/op1repacker | github.com/op1hacks/op1repacker | master | 390b18e4193a3af4f44e8f89b5f6c017a71ddf96 (2026-05-05) | MIT (Copyright 2016 op1hacks) | yes |

Not in git here: TE's firmware (stock/op1_246.op1) and anything derived from it. Never commit or redistribute it.
Licence note: op1r (GPL) code and op1-sdk (MIT) code can be combined in a GPL result; our own overlay should pick MIT or GPL deliberately before mixing.

Platform layer note (2026-11-04): src/platform/ofp_probe uses address and struct-offset FACTS read from pcppcp-op1r-src `screens/sysmon.c` (idle-thread stats at thread+0x34 / +0x20, VDK heap descriptor walk, yaffs free-space calls, volume knob byte `0x018b62a4`). No code was copied; the implementation is written from scratch and is MIT (ours). No GPL code is linked into any image.
