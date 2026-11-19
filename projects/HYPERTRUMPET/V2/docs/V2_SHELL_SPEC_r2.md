# HYPERTRUMPET V2 shell spec, round 2 (research only, no CAD)

2026-11-14. Supersedes nothing in `V2_SHELL_AUDIT_AND_SPEC.md`; read that for the v1 joint analysis. Tags: **[W]** web source, **[M]** measured by me on the v1 files, **[G]** general knowledge or rule of thumb, unverified, **[I]** my inference. Model frame: x = horn axis (player end 620 -> bell 1150), y = lateral (pod on +y), z = valve axis (up). mm.

## 0. EMGOR's round-2 inputs applied
- Weight is not a constraint (the 250 g target and the weight budget are dropped). Comfort during long sets is the objective.
- v1 arm geometry is kept. v2 = same arms, refit to standard dimensions, with: 0.4 mm rigid clearance, the two interfering spots moved, R>=3 root fillets, PETG with TPU liners.
- Cells live in the palm rest behind a tool-free door.
- Given measurement: "valves are 18 mm on top".

## 1. Standard Bb trumpet dimensions v2 builds to

Search reality: manufacturer pages give bore, bell, and "weight class" only. Casing OD, slide-tube OD, valve pitch, leadpipe-to-bell spacing and brace positions are **not published** anywhere I could reach (Yamaha, Bach, Thomann, Kanstul, parts sellers; Yamaha/Kanstul PDFs were unreadable). Those rows are therefore [G]/[M-model] and flagged.

| Dimension | Build value | Source / status |
|---|---|---|
| Bore | 11.66 mm (.459"), range 11.65-11.73 | [W] Bach selection guide (L Bb .459, bore measured as ID of the 2nd valve slide); Yamaha YTR-2330/8335 11.65 mm |
| Piston OD | 16.5-17.0 mm (.649"-.6695") | [W] Getzen comparison test; Holton parts list .661-.665" |
| Valve bore (Bach Strad nominal) | 16.87 mm (.664") | [W] machining forum |
| **Valve casing OD** | **18.0 mm** (EMGOR) | EMGOR measurement. Consistent with 16.87 mm bore + ~0.55 mm wall [I]. See interpretation below |
| Valve cap OD | 21.0 (range 20.3-22) | [M] Sketchfab model caps 20.3-21.2; cap must be > casing top; [G] otherwise |
| Valve pitch (centre to centre) | 21.5 | [I] model pitch 22.3 minus the (19.2 - 18.0) casing change; casings are nearly touching. Hook design tolerates +-1 via liners |
| Casing height / with buttons | 104 / ~141 | [M] model (typical range [G] 100-110 / 145-160) |
| Bell rim OD | 122.2 mm (Bach #37, 4 13/16") | [W]; other models 123 (Yamaha), 125, 127 [W]. Spiral built for 122-127 |
| Bell flare profile (s mm from rim : radius mm) | 0:61.6, 10:44.4, 20:32.3, 34:24.6, 50:19.5, 65:16.2, 83:14.0 | [M] Sketchfab model; rim agrees with [W]; shape is plausible, not verified against a real bell |
| Slide fixed (outer) tube OD | 13.4 | [M] model; [G] real ~13.3-14 |
| Slide U-leg (inner) tube OD | 12.5-12.7 | [G]: bore 11.66 + wall ~0.4-0.5. The model's 10.1 mm legs are undersized (a 10.1 mm OD cannot hold an 11.66 mm bore) [M logic] |
| Leadpipe OD | 12.7 | [G] |
| Leadpipe-axis to bell-axis offset | 31.2 | [M] model only (y 494 vs 525.2); no published figure found. Treated as the best available, flagged |
| Bell brace | rods x ~993, y ~494.5, z 37-100 | [M] model only |
| Horn mass | 0.97-1.08 kg | [W] listings for Bach Strad models |
| Overall length | ~530 | [M] model; [G] 480-530 |

**Interpretation of "valves are 18 mm on top".** Candidates: (a) casing body OD, (b) valve top cap OD, (c) finger-button. The piston is 16.5-17.0 mm [W], so the casing cannot be below ~17.5 mm OD; the cap has to cover the casing top, so cap OD > casing OD; the finger button is ~12-14 mm [G]. Only (a) is physically consistent with 18 mm. **I build casing OD 18.0 mm (measured at the top, below the cap) and cap OD 21.0.** If EMGOR meant the cap, the casing would be ~15-16 mm, which contradicts the piston size, so I regard (a) as safe. Residual ambiguity: the casing may be wider lower down (valve ports, brazed bands); hooks therefore use TPU liners that absorb +-1 mm.

## 2. v1 arms kept: the refits (this replaces change-list items 1-7 of the first spec where they conflict)

Arms (all [M], v1): top blades 3.6 mm * 82 / 40 mm wide at z 117-124 (x 726-808 and 895-940), reach to y ~495; lower blades 2.9-5.2 mm * 54-72 mm wide at z 24-47 (x 726-808), reach to y ~498; right vertical blade 7.7 mm * 84 mm at x 888-895 (y reach 505); casing-end posts 8-9 * 16 at x 807-822 and 888-896 (z 6-22 and 78-98). EMGOR likes them; geometry stays. Changes only:

| Item | v1 value | v2 change | Why |
|---|---|---|---|
| Rigid clearance to horn | 0.0-0.6 mm | +0.4 mm per side on every rigid face; TPU liner faces 0.2 interference | 180 interpenetrating samples in v1 [M] |
| Top-right blade tip vs thumb hook `Torus.001` | 0.7 mm interference at (903-907, 493-496, 120-124) | shorten the tip 4 mm and relieve a 6 * 4 mm notch; keep blade thickness | worst measured interference [M] |
| Right vertical blade vs 1st-slide tube exits | 0.34 mm at x 888-889 (model legs 10.1 mm) | move the blade inner face +3.0 mm in x (x 891), add a 1.5 mm TPU liner | real U-legs are ~1.2 mm larger in radius than the model's [G], so the real interference is ~1.5 mm; the blade must clear x 888 + 6.35 radius pad |
| Casing-end posts | sized for 19.2 mm casings | inner faces 0.6 mm inboard each side + 1.6 mm TPU liner (net 0.4 mm squeeze) | casing 18 vs 19.2, pitch 21.5 vs 22.3 |
| Root fillets | sharp | R >= 3 at every arm root and post root; 6 mm ribs on blades > 40 mm | stress concentration [G] |
| Material | PLA | PETG; 1.2 mm TPU 95A liners in 1.0 mm recesses with 2 mm retaining lips; 4 wall loops | creep and brittleness in PLA |
| Support | blades printed flat with support under | print blades with the mount standing as in v1, but add 0.4 mm support interface gap and a 45-deg chamfer under each blade so supports break away clean | support removal can crack 3.6 mm plates [I] |
| 3rd-slide leg plane | blades at z 24-47 sit in y 486-520 | unchanged geometry (EMGOR likes it) but verify with a coupon that slide travel and the ring (x 782-820, y 494-502, z 51-83) are not touched | keep-out is a hard requirement |

No joint blocks are mentioned here: the peg/cup joint is replaced by the dovetail rail (first spec section 4 and 5 stand).

## 3. Spiral: C-coil vs start further out (decided with the standard spacing)

Standard-spacing data: leadpipe axis is 31.2 mm from the bell axis [M model; no published value], so the leadpipe surface is 24.9 mm from the bell axis. Ribbon outer radius = bell radius + 11.5 mm [spec]. That equals the leadpipe surface (24.9) at **s ~ 85 mm** (r 13.9 + 11.5 = 25.4), and a full ring is closer than 0.5 mm to the leadpipe/tuning-slide region from s = 85 to 150 [calc on model]; the leadpipe leaves the bell-axis neighbourhood near x ~1050-1065 (s ~ 85-100) [M, model], so s < 85 is clear. Computed:

| Option | s span | Pitch | Turns | Strip path | Verdict |
|---|---|---|---|---|---|
| A: full ring, start further out | 10 -> 85 | 15 (ribbon 12 + 3 gap) | 5.0 | 942 mm | no leadpipe conflict, but rim bead only 10 mm away, gap 3 mm between turns (looks like a coil spring, hard to print clean), and a closed ring cannot be fitted over the 122 mm rim |
| B: **C-coil** | 20 -> 140 | 18.7 | 6.4 | 971 mm | needs a gap on the leadpipe side; fits the full 1 m strip; installs sideways |
| C: full ring 20 -> 140 | 20 -> 140 | 18.7 | 6.4 | 971 mm | collides with the leadpipe/brace region (< 0.5 mm at s 85-150) [M] |

**Decision: B, C-coil.** Gap 100 deg centred at 173 deg (the direction of the leadpipe from the bell axis: atan2(+3.8 up, -31 lateral)). PETG arcs cover ~260 deg per turn at the 11.5 mm ribbon height; the gap is bridged by a TPU 95A flexure at only 5 mm height (3 pad + 2 bridge), so its outer radius at s=100 is ~17.5 against a leadpipe surface at 24.9: **~7 mm clear** [calc]. This also solves installation (TPU bridges stretch ~25% to pass the 65 mm width at s = 20). The strip runs through the bridge uninterrupted (continuous 971 mm path). Weight is no longer a constraint, so the wall can go to 1.6 mm (4 loops) and standoff pads to 10 mm wide.

## 4. Anthropometry and the ergonomic layout

**Data used (mm):**

| Measure | 5th F | 50th | 95th M | Source |
|---|---|---|---|---|
| Hand length | 165 | 180 F / 193 M | 210 | [W] ANSUR II via madegooddesigns; NCSU civilian table agrees (F 5th 165.0, M 95th 210.6) |
| Hand breadth (across knuckles) | 72-73.4 | 78 F / 88 M | 96-97.6 | [W] ANSUR II and NCSU table |
| Palm length | 100 | 108 F / 116 M | 127 | [W] ANSUR II |
| Hand circumference | 172.5 | 187 F / 212 M | 230 | [W] |
| Thumb breadth | 18.6 | 20.7 F / 24.1 M | 26.5 | [W] NCSU table (F 5th 1.86 cm, M 95th 2.65 cm) |
| Power-handle grip diameter, comfortable | 32.5 | | 41.4 | [W] derived from ANSUR II (Kong and Lowe rule) |
| Max grip diameter (unverified handout) | 43 | 48 | 53 | [W-unverified] |
| Thumb length / index length (unverified handout) | 40 / 60 | 47 / 67 | 53 / 74 | [W-unverified]; treat as order-of-magnitude |
| Finger width at the pad | ~16 | ~18 | ~20.5 | [G]; I found no published table |

Not found: finger-length tables and fingertip-pad widths (Garrett 1969/1970 DTIC PDFs returned 403/404). Finger-width values above are [G].

**Standard left-hand grip [W]** (trumpet teaching sources): the left hand carries the horn; thumb in the 1st-slide saddle/ring; ring finger in the 3rd-slide ring; index and middle fingers wrap the casings; the casings sit in the palm; little finger free (or under the 3rd slide). Model positions: 3rd ring centre (x 800, z 67); 1st-slide thumb hook (x 910, z 104-129) [M].

**What follows [I]:**
1. **Thickness is the binding constraint.** For a power grip the comfortable diameter is 32.5-41.4 and the maximum is ~53. In v1 the distance from the casing's far face (y 500) to the Trellis face (y 576) was **76 mm**: not graspable by any percentile. The grip-side stack (casing 18 + whatever is between the palm and the fingertip face) must be <= 41 mm for the 5th female, <= 53 for the 95th male. **Therefore: Trellis face y <= 548-550 (rear of plate y 538 + 10 mm thin skin), pod depth beyond that goes elsewhere.** Bulk electronics move to the palm rest, the two ears (x 745-818 and 893-935, y 540-580), and the bottom; only a 10-12 mm skin sits on the grip face.
2. **Trellis placement.** Centre x 853.5 (over casing 2); rows at z 99.6 / 84.3 / 69.0 / 53.7 (pitch 15.3, board 61.7). These align (within 4 mm) with the finger stack the standard grip implies: index ~z 100, middle ~84, ring ~67 (the 3rd ring height), little ~50, at ~17 mm finger spacing [I]. Columns x 830.6 / 845.9 / 861.1 / 876.4. Pad pitch 15.3 is below the pad width of every hand except the 5th female (16): the 4x4 is 3 usable rows for most hands (index, middle, ring) with the little finger a supporter. Do not rely on 4 independent fingers.
3. **Thumb joystick (KY-023).** On the pod's front ear at x 912, z 112, mounted at the 1st-slide saddle position so the thumb sits where it already sits; stick cap top <= y 560, cap cup 28 mm across for the 95th thumb breadth (26.5 + 1.5). Distance to the nearest Trellis key (x 876, z 99.6): 36 mm. Stick body is ~25 mm tall: it sits on the ear (deeper pod), not in the grip skin.
4. **Palm rest.** Underside rest as before but sized to the data: 110 (x) * 85 (y) * 30 (z), x 800-910, y 490-575, z -4 to -34, longitudinal radius 90, transverse 55, 2 mm TPU skin (palm length 100-127, breadth 73-98: patch covers the 5th female entirely and the 95th male palm centre). Contact must not push the heel into the slide plane (all slide structure is at z >= 20).
5. **FSR.** On the rest's upper-front edge where the palm squeezes, x 853, ~z -15 (pressure rises with squeeze), or at z 30 on the pod face under the little finger as a second option.
6. **Faders/pots/encoders (right hand is busy on the valves).** Standard right hand: three fingers on valve buttons, thumb between casings 1 and 2 under the leadpipe, little finger hook [W]: **the right hand reaches nothing mid-phrase.** Therefore:
   - live (during playing): left thumb = joystick; left index/middle/ring = Trellis; left palm squeeze = FSR; right = valves (Halls);
   - set-up controls reached by the left fingers when pausing: 3 faders on the rear ear, vertical at x 770 / 790 / 810 (pitch 20, travel z 55-100), 24 mm to the first Trellis column, inside 60-80 mm finger reach [I]; 2 pots and 2 KY-040 encoders on the front ear at x 905 / 925, pots z 80, encoders z 50 (30-50 mm from Trellis column 4).
   - nothing is on the -y side (slides and rings keep their free plane y 486-520; the ring finger passes through the 3rd ring from the -y side and reaches only y ~513).
7. **Slide rings and slides.** Pod width stays x 745-935; rear ear (x 745-818) overlaps the 3rd-ring x range but at y >= 540, 40 mm from the ring plane (y 498); the 1st-slide thumb hook at x 903-918 is behind the joystick ear (y 540+ vs hook y 493-496). Travel zones unchanged (first spec section 9).

**What I cannot settle from references** and EMGOR must decide: whether the left hand's palm is on the underside rest with fingers wrapping onto the +y pod face (what he described), or in the standard casing-in-palm grip (palm on the casings' -y side); in the standard grip the ring finger sits in the 3rd ring and is not free for Trellis. The Trellis rows above are consistent with the standard grip; a foam mock-up at three offsets (y 544, 548, 552) settles it in minutes.

## 5. Battery door (tool-free) and 2-cell bay

**Proposal: sliding drawer cartridge, not a hinged lid.** Why: cells seat into spring holders outside the horn, one-handed swap, no hinge to fatigue in PETG, no seam under the palm.
- **Bay:** in the palm rest, cells side by side along x (axis x), cavity 47 (y) * 25 (z) * 82 (x): 2 * 21.9 + 1.0 divider + walls; 70.4 cell length + holder spring depth [holder length ~75-80, verify with the actual Bistook part]. Opens at the **+x (bell-side) end face** of the rest, so palm skin never meets the seam.
- **Cartridge:** one PETG tray holding both single-cell holders, wired in parallel with short equal leads to a 2-pin keyed connector (JST-PH or a 2-pin XT30) on the bay's rear wall that mates automatically as the drawer closes (blind-mate with 1 mm lead-in chamfers). Cartridge 46 * 24 * 80, outside TPU rail 0.4 mm clearance.
- **Retention:** two in-plane PETG flex bumps (1.5 thick, 14 long, 0.6 mm detent) = ~5-8 N pull-out force [I]; a 10 mm grosgrain ribbon pull loop captured under the drawer front plus a finger notch 18 * 8 * 4 mm deep. Palm pressure acts along z, the drawer axis is x, so squeezing does not eject it.
- **Polarity keying:** the cells are flat-top (BOM), which defeats the nub trick. Put a **keyed positive button on each cell** (glued 8 mm * 1.2 mm nickel disc) and recess each holder's positive contact 1.2 mm inside an 8.6 mm bore, with the surrounding wall proud so a flat negative end bottoms out before touching it; negative contacts are flush. A reversed cell cannot make contact. Marking: + end moulded with a raised bump on the tray, colour-coded ribbon. [I: untested; flat-top terminal geometry varies, test the bore on the real cells before committing].
- **Safety:** cells fully enclosed, 3 mm PETG + 2 mm TPU outer skin; 2 * 2 mm vent holes at the far (-x) end; BMS and boost stay in the fixed part of the rest (not in the cartridge); isolation barrier between cells and strip supply.
- **Alternatives rejected:** hinged door (PETG living hinge fatigue; seam in the palm zone), sliding flat lid in dovetail grooves (works but cells are seated blind in the bay).

## 6. Genuine decision points for EMGOR (research cannot answer these)

1. Palm posture: stay with the underside rest + fingers onto the pod face, or adopt the standard casing-in-palm grip with the ring finger in the 3rd ring? Recommend: foam mock-up at three pod offsets, go with the underside rest if the ring finger is not needed for the 3rd slide in your repertoire.
2. Thin Trellis skin (10 mm) on the grip face with bulk electronics in the ears and rest, versus keeping the v1 deep pod. Recommend: thin skin; the deep pod fails the 41-53 mm grip limit for every hand size.
3. Which controls must be playable mid-phrase (taste): recommend only joystick, Trellis rows 1-3, FSR; faders/pots/encoders as pause-time setup.
4. Spiral looks: 6.4-turn C-coil with a visible gap on the leadpipe side, versus a 5-turn flare-only coil without a gap. Recommend C-coil (needed anyway for installation).
5. Where on the bell the mic sits (taste/sound): recommend first turn at s=20, 12 mm behind the rim plane, top of the bell, aimed 15-20 deg inward; adjust by ear.
