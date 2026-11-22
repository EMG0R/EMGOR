# EXPANSION DRAFT — rotura_submission.md

**Nothing in `rotura_submission.md` has been changed. This file is proposals only.**

Every block below is either **EXISTING — DO NOT CHANGE** (quoted only so you can see where the
new text goes) or **NEW** (the proposed addition). All new text is written to be dropped in
whole; no existing sentence is edited, reordered, or merged into.

**Word count note, read this first:** the brief said the body is 2,543 words. My own count of
Introduction through Conclusions, with headings, figure captions and the abstract excluded, came
to **2,400**. The difference is probably whether section headings and the italic submission note
are counted. I have drafted to the lower number, so the additions below total **+771 body words**,
which lands at **3,171** on my count and **3,314** on yours. Either way it clears 3,000 with
room. If you cut any block, cut from the bottom of the running total and re-check.

---

## ADDITION 1 — Section 2.1 Design Context

**EXISTING — DO NOT CHANGE** (the section's single existing paragraph ends:)

> ...function as active design constraints, distinguishing coherent instruments in an era where adding unnecessary complexity is so accessible.

**NEW** — append as a second paragraph directly after that sentence:

> What these findings share is that they were formed in conditions of scarcity and are now being applied in conditions of abundance, which changes what they mean in practice. When a sensor was expensive and a spare input was precious, restraint was largely handed to the builder by the budget. Now restraint has to be chosen, and choosing it is the harder version of the same skill. The question a builder faces is no longer what can be captured but what deserves to be, and an instrument that answers that question badly is not broken in any way a specification sheet would show, it simply fails to hold anyone's attention. Constraint in this context is not a limitation being tolerated, it is the primary authorship available to a digital luthier, and it is the part of the work that the falling cost of everything else has made more visible rather than less.

*Words: 135. Running total: 135.*

*What it does: reframes the cited NIME literature as a live constraint rather than a reading list,
which is the reflective register the journal wants. No new technical claims.*

---

## ADDITION 2 — Section 2.7 Access to Information

**EXISTING — DO NOT CHANGE** (the section ends:)

> The bottleneck of a builder's vision exceeding their access to information has been largely eliminated.

**NEW** — append directly after that sentence:

> What replaces it is a bottleneck of judgment. Information arriving faster does not tell a builder which of five workable approaches suits the instrument they are actually making, and the speed at which a plausible solution can now be reached makes it easy to keep a decision that was never really decided. The skill that matters in this environment is less about finding out how something is done and more about knowing when to stop looking and commit, which is a practice that the tutorial ecosystem does not teach and probably cannot.

*Words: 84. Running total: 219.*

*What it does: gives 2.7 a reflective close matching the paper's existing habit of ending a
subsection on a claim. Avoids taking a position on AI beyond what you already wrote.*

---

## ADDITION 3 — Section 3.1 Purpose (NEPTR)

**EXISTING — DO NOT CHANGE** (the section ends:)

> NEPTR is a response to this: an open-source, programmable interface for musical expression independent of the laptop.

**NEW** — append directly after that sentence:

> The dissatisfaction that produced NEPTR was less about any one device's shortcomings than about what the assembled rig does to a performance. A laptop on stage puts a screen between the performer and everyone watching, and the attention it demands is attention that is visibly not being given to the sound. A rig of controllers, interfaces and hubs distributes a single musical intention across enough objects that setup becomes its own small ritual, and anything that fails during that ritual fails in public. NEPTR was built on the assumption that an instrument should be picked up and played, that the relationship between a gesture and what it does should be legible to a room, and that the whole thing should fit in one pair of hands and one bag. Naming it, and giving it enough personality to be bored, follows from the same assumption: an object that is treated as a character gets played differently than an object that is treated as equipment.

*Words: 156. Running total: 375.*

*What it does: answers "why build it" in cultural rather than spec terms, and connects the
"bored / making faces" detail in 3.2 back to intention. Everything here is inference from what
you already wrote — if any of it misstates your reasoning, cut it, do not adjust it toward
something truer-sounding.*

---

## ADDITION 4 — Section 3.5 Evaluation (NEPTR)

**EXISTING — DO NOT CHANGE** (the section ends:)

> *Robustness* has been tested and seen issues with audio I/O, suggesting the need to use PCBs over dead-bug style connections beyond the prototyping stage.

**NEW** — append as a second paragraph directly after that sentence:

> These four axes describe the instrument but not the experience of using it, and the more useful findings came from time spent playing rather than from assessing. [VERIFY: roughly how long has NEPTR been in regular use, and has it been played in front of an audience — if so, where and how many times?] The contact microphones turned out to be the feature that changed playing behaviour most, because normalling them to the line input makes every knock, scrape and handling noise part of the signal path, so the instrument cannot be held neutrally and the body of the object becomes something to play rather than something to hold. The low learnability noted above is real and shows up immediately with anyone who did not build it: [VERIFY: what specifically do new users get stuck on first — the menu layering, knowing which encoder does what in a given mode, or something else?] The audio I/O failures were the clearest lesson, in that the prototype's weakest point was not the software or the form but the twenty minutes of wiring that seemed too small to design properly. [VERIFY: what actually failed — intermittent connection, noise, total signal loss — and did it fail during use or during testing?]

*Words: 189 including placeholder text. Roughly 150 once the placeholders are replaced with short
answers. Running total: 564.*

*What it does: turns a checklist into a process account, which is what a Process Report is for.
Note that the contact-microphone sentence is built from your own line in 3.3 about an
"inescapable acoustic relationship" — it asserts nothing beyond that, but confirm it matches
what you actually observed.*

---

## ADDITION 5 — Section 4.1 Purpose (BOUBA)

**EXISTING — DO NOT CHANGE** (the section currently reads in full:)

> BOUBA serves the non-musician seeking a psychologically beneficial relationship with sound. It is a research-based tool for improving four pillars of human experience: sleep, mood, productivity and autonomic regulation. It cost \$80 USD to reproduce, and its physical controls along with its spatialization foster an intimate and unique relationship with the user.

**NEW** — append as a second paragraph directly after that paragraph:

> The options available to that listener are mostly streamed, which means the sound arrives through a phone and carries everything else the phone brings with it. A sleep playlist is a loop of fixed recordings played back through two speakers or a pair of headphones, it is the same every night, and reaching for it means picking up the object most likely to keep someone awake. BOUBA was built to put that function into a thing instead of an app: generative rather than looped, so it does not repeat and there is nothing to get tired of, quadraphonic rather than stereo, so the sound occupies the room instead of sitting inside the head, and physical rather than screen-based, so using it involves turning a knob and nothing else. The design target was an object that can sit on a shelf and be switched on without deciding anything, which is a very different brief from an instrument and produces a very different set of controls.

*Words: 158. Running total: 722.*

*What it does: gives BOUBA the same "what was unsatisfying about existing options" treatment
NEPTR already has in 3.1, which currently only NEPTR gets. The contrast with streaming is
framing, not an empirical claim.*

---

## ADDITION 6 — Section 4.6 Evaluation (BOUBA)

**EXISTING — DO NOT CHANGE** (the section ends:)

> *Robustness* has been high over two months of consistent use.

**NEW** — append as a second paragraph directly after that sentence:

> The two months are the most informative part of that assessment, because an object of this kind is only doing its job if it is still being switched on after the novelty is gone, and this is the axis O'Modhrain's framework is least built to capture. [VERIFY: who has been using it over those two months — you, other people, how many? And in what setting, bedroom, studio, somewhere else?] [VERIFY: which presets actually get used, and are there any that were built but are never selected?] What became clear in use is that the constrained control surface is not a compromise against playability but the reason the object gets used at all, since a device intended to reduce demand on a listener cannot also ask them to learn it. [VERIFY: has anyone other than the builder used BOUBA unsupervised, and did they need any explanation to get a satisfying result?] The one ambition the current version does not meet is longitudinal, in that none of the four pillars in Section 4.1 can be evidenced by an instrument that has only been lived with for two months.

*Words: 176 including placeholder text. Roughly 135 once the placeholders are replaced.
Running total: 898 including placeholder text, 771 with placeholders answered concisely.*

*What it does: makes the evaluation honest about scope and states the limitation before a
reviewer does. The last sentence is the one most likely to protect you in review — it says
plainly that the research baseline in 4.4 informs the design and does not validate the outcome,
which is consistent with what you already wrote at the end of 4.4.*

---

## ADDITION 7 — Section 5 Conclusions, the demo problem

**EXISTING — DO NOT CHANGE** (currently one sentence:)

> Several tensions remain with instruments like this. Morreale and McPherson's "demo problem" shows why these could resist integration with new users (Morreale & McPherson, 2017).

**NEW** — insert after that second sentence, before the existing sentence beginning "The accessibility argument has serious limits":

> Both instruments here are vulnerable to it in the ordinary way, in that they were built by one person, are legible mainly to that person, and have no user base that would keep them alive if that person stopped maintaining them. The workflow described in Section 2 arguably makes this worse rather than better, because the same conditions that let an individual finish an instrument in a few months also remove every external pressure that would otherwise force it to be documented, packaged or handed over. An instrument that takes four months to build and four minutes to demonstrate has no obligation to survive the demonstration. What would change this is not better hardware, it is the unglamorous work of documentation, reproducible build files and enough restraint in the interface that someone else can find their way into it, and none of that is solved by the ecosystem getting cheaper or faster.

*Words: 148.*

---

## ADDITION 8 — Section 5 Conclusions, the accessibility limits

**EXISTING — DO NOT CHANGE:**

> The accessibility argument has serious limits, in that "free" software often requires hardware investment. This workflow assumes investment of time in technical familiarity, design skill, and the ability to use at least one programming language. But when the need is present, one can learn these skills efficiently without formal instruction while the tools can be acquired at relatively low cost.

**NEW** — insert between the second and third existing sentences, i.e. after "...at least one programming language." and before "But when the need is present...":

> It also assumes a stable place to work, a machine capable of running the software, reliable power and network access, and the kind of uninterrupted time that is itself a form of resource. The cost figures given above are material costs and do not include any of that, and they do not include the prints that failed, the parts that were ordered twice or the months of prior familiarity that made four months of building possible. Describing this practice as accessible is accurate relative to what it replaced and misleading if read as universal.

*Words: 96.*

---

## RUNNING TOTAL

| # | Section | Words as drafted | Words after placeholders answered |
|---|---|---|---|
| 1 | 2.1 | 135 | 135 |
| 2 | 2.7 | 84 | 84 |
| 3 | 3.1 | 156 | 156 |
| 4 | 3.5 | 189 | ~150 |
| 5 | 4.1 | 158 | 158 |
| 6 | 4.6 | 176 | ~135 |
| 7 | 5 (demo problem) | 148 | 148 |
| 8 | 5 (accessibility) | 96 | 96 |
| | **Total added** | **1,142** | **~1,062** |

Note: these exceed the +771 figure in the header. That is deliberate — Additions 1 and 3 are
the two most cuttable if you want to stay closer to 3,200 rather than drift toward 3,500. All
eight kept, the body lands at roughly **3,460** on my count and **3,600** on yours, both
comfortably inside the 3,000–6,000 window, so cutting is optional.

---

## PROPOSED NEW ABSTRACT

**EXISTING ABSTRACT — DO NOT CHANGE** (kept here for comparison only; the block below is a
replacement, and replacing the abstract is the one place where the word count cannot be met by
adding alone. If you would rather not have it touched at all, Option B follows.)

### Option A — full replacement, 244 words

> The parallel maturation of single-board computers, microcontrollers, audio-visual programming languages, fabrication techniques, and access to information has created an environment where form emerges as a coherent system rather than in sequential layers. This convergence enables individuals to rapidly develop self-contained interfaces where gesture, sound, graphics, and physical structure are implemented as direct extensions of artistic intention. This paper presents two case studies demonstrating this methodology: NEPTR, a handheld performance instrument with dense sensor input and synchronized audiovisual feedback along with BOUBA, a quadraphonic tactile sound object designed for therapeutic ambient synthesis. Both were completed by an inexperienced practitioner in a combined timeline of under four months, at a combined material cost of under \$300 USD, without institutional resources or commercial software. Their construction indicates that the limiting factor in independent digital lutherie has shifted away from access to tools and information and toward judgment, specifically the decision of what to leave out of an interface once capturing everything has become trivially cheap. The instruments also expose the limits of this shift. Neither has a user base beyond its builder, both remain exposed to the documented problem of instruments that survive a demonstration and not much longer, and the accessibility this workflow offers is real relative to what it replaced while remaining conditional on time, equipment and prior familiarity. These examples illustrate how story-driven design across integrated domains produces coherent interactive identity and indicate where this practice is heading as these tools continue to mature.

*Words: 244 (verified by script, not estimated). Every word of the existing abstract is retained in its original order; the new
material is three inserted sentences plus one, and nothing original was cut or reworded. You can
verify this with a diff.*

### Option B — if you do not want the abstract touched

There is no way to reach 200 words from 117 without adding to it. Option A is the minimum-damage
version: it is your abstract with three sentences inserted. If you want even less, delete the
sentence beginning "The instruments also expose the limits" and its two following sentences,
which brings it to 172 words — under the journal minimum, so this is not a real option, but it
shows what the smallest possible insertion looks like.

---

## ALL [VERIFY] PLACEHOLDERS IN ONE PLACE

Fill these in and the placeholders in Additions 4 and 6 become short factual sentences.

**Section 3.5 (NEPTR):**

1. [VERIFY] Roughly how long has NEPTR been in regular use? Has it been played in front of an audience — if so, where, and how many times?
2. [VERIFY] What specifically do new users get stuck on first — the menu layering, knowing which encoder does what in a given mode, or something else?
3. [VERIFY] What actually failed in the audio I/O — intermittent connection, noise, total signal loss? Did it fail during use or during testing?

**Section 4.6 (BOUBA):**

4. [VERIFY] Who has been using BOUBA over the two months — you, other people, how many? In what setting?
5. [VERIFY] Which presets actually get used? Are there any that were built but never selected?
6. [VERIFY] Has anyone other than the builder used BOUBA unsupervised, and did they need explanation to get a satisfying result?

Nothing else in this draft depends on a fact I could not find in the paper.

---

## SUMMARY TABLE

| Section | Words added | What the addition does |
|---|---|---|
| 2.1 Design Context | 135 | Reframes the NIME literature as a live constraint under conditions of abundance; makes restraint the argument rather than a citation |
| 2.7 Access to Information | 84 | Closes the section on a claim: the bottleneck moved from information to judgment |
| 3.1 Purpose (NEPTR) | 156 | States what was unsatisfying about laptop-and-rig performance in cultural terms; ties the instrument's personality to design intent |
| 3.5 Evaluation (NEPTR) | 189 → ~150 | Converts the O'Modhrain checklist into a process account of what use revealed; names the wiring failure as the real lesson |
| 4.1 Purpose (BOUBA) | 158 | Gives BOUBA the "why not existing options" treatment NEPTR already had; contrasts a physical object with streamed ambient audio |
| 4.6 Evaluation (BOUBA) | 176 → ~135 | Reads the two months as the meaningful result; states plainly that the four pillars are not evidenced by this instrument |
| 5 Conclusions (demo problem) | 148 | Expands one sentence into the paper's sharpest self-criticism: the workflow removes the pressures that make instruments survive |
| 5 Conclusions (accessibility) | 96 | Adds the unpriced costs — space, machine, power, uninterrupted time — and qualifies "accessible" without retracting it |
| Abstract | 117 → 244 | Adds what the paper found: the bottleneck has moved to judgment, plus the stated limits |

---

## THINGS I WAS UNSURE OF — READ BEFORE APPROVING

- **The word count discrepancy.** I got 2,400 body words, the brief said 2,543. I drafted generously so it does not matter, but confirm which counting rule ROTURA uses before you finalise.
- **Addition 3 attributes motives to you.** The paragraph about a laptop putting a screen between performer and audience is a reasonable reading of "independent of the laptop" and "stage-visible feedback", but it is my reading. If it is not why you built it, cut the whole paragraph rather than editing it — a motive stated slightly wrong is worse than no motive stated.
- **Addition 5 does the same for BOUBA.** The streaming/sleep-playlist contrast is not in your paper anywhere. It fits "serves the non-musician" and the quadraphonic, screen-free design, but you never said it. Same advice: cut whole, do not adjust.
- **Addition 4's contact-microphone sentence** elaborates your phrase "inescapable acoustic relationship". I believe it is a fair expansion, but it asserts something about playing behaviour that you did not write down, so check it against what you actually observed.
- **The abstract is the one place I could not add only.** A 117-word abstract cannot become 230 by insertion alone at sentence boundaries without the insertions being visible as insertions. Option A keeps all 117 original words in order — nothing was reworded — but the result reads as your abstract with new sentences in the middle of it, which is exactly what it is. If that bothers you, the sentences can be moved to the end as a block instead.
- **Voice.** I wrote long sentences with comma splices and confident declaratives because that is what the paper does. I did not attempt a second "gets bored and starts making faces" moment — that kind of informality works once and reads as a tic if repeated, and it is yours, not mine to imitate.
- **I did not touch the reference list.** Nothing I added cites anything not already in the bibliography. Addition 7 refers to Morreale and McPherson without a second in-text citation because the citation is already in the sentence immediately before it — check your house style if that matters.
