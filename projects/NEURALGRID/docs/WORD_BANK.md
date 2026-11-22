# Word bank for gesture mapping

## What the research says

AAC (augmentative communication) is the field that already solves this
exact problem: let someone say anything through a limited set of
selections.

- **~50 words ≈ 40–50%** of everything said.
- **~200–400 words ≈ 80%** of everything said, out of ~250,000 in English.
- **Only ~10% of the first 200 core words are nouns**, and those are
  generic (*person*, *place*, *thing*), never specific (*porcupine*).
- Core vocabulary is dominated by pronouns, verbs, prepositions,
  determiners, auxiliaries, negation and question words — and it changes
  very little between people or situations.

**Consequence for gesture mapping: do not spend gestures on nouns.** Nouns
are "fringe" vocabulary — infinite, situation-specific, and the worst
possible return per gesture. Spend the inventory on function words and
generic verbs, which are finite, stable, and reusable in every sentence.

## The combinatorics that make this tractable

The bank does not need one gesture per word. With `n` distinguishable bols
in sequences of length `L`, the addressable space is `n^L`:

| bols | ×1 | ×2 | ×3 |
|---|---|---|---|
| 6 | 6 | 36 | 216 |
| 8 | 8 | 64 | 512 |
| 10 | 10 | 100 | 1000 |
| 12 | 12 | 144 | 1728 |

**8 reliably separable bols in pairs already covers a 64-word bank.** That
is a far easier hardware ask than 90 distinct gestures, and it is why the
inventory size question is not the same as the vocabulary size question.

The predictive English model is what makes short sequences safe: it prunes
collisions from context, the way T9 resolved 3 letters per key. A bol pair
that is ambiguous in isolation is usually unambiguous in a sentence.

---

## The bank

### 0 — Frame (11) — guaranteed

```
who   what   when   where   why   how
yes   no   not   is/are   the
```

*Notes:* `is/are` is one entry — one copula, no conjugation. `the` and not
`a`: definiteness is droppable, and `the` is the single most frequent word
in English. Question words are the highest-leverage entries in the whole
bank, because a question turns a limited vocabulary into a way of getting
information you don't have words for.

### 1 — People and pointing (10)

```
I   you   we   it   this   that   here   there   my   your
```

*The grid can do some of this work without words.* 64 pads are a natural
pointing device — "that one" can be a pad, not a bol. Worth deciding
before spending gestures on deixis.

### 2 — Verbs (16)

```
go   come   get   give   make   do   have   want   need
like   see   say   know   can   stop   help
```

*Generic verbs, no tense.* Tense via tier 5 time words if needed at all.

### 3 — Relations (10)

```
in   on   out   up   down   to   from   with   for   of
```

### 4 — Amount and degree (10)

```
more   less   all   some   none   again   very   too   big   little
```

### 5 — Time (8)

```
now   then   before   after   always   never   today   soon
```

### 6 — Joining (6)

```
and   or   but   if   because   so
```

### 7 — Social (6)

```
please   thanks   hello   bye   sorry   good
```

### 8 — Generic nouns (8)

```
thing   person   place   time   way   part   one   other
```

**Total: 85.** Comparable to Toki Pona (~120 words, designed for general
conversation) and far below Ogden's Basic English (850). At 85 words the
bank sits above the ~50-word / 40–50% mark and below the 200-word / 80%
mark — the right place to start, and extendable by tier.

---

## Open decisions

1. **Does the grid handle pointing instead of bols?** If pads carry deixis
   and object reference, tier 1 shrinks and the bol inventory gets cheaper.
2. **Sequence length.** 1 bol per word needs ~85 separable gestures and is
   almost certainly not achievable. 2 bols per word needs ~10. Pick after
   separability is measured.
3. **Which tiers ship first.** Tier 0 + 2 (frame + verbs, 27 words) is
   already a usable speaking system: questions, negation, affirmation and
   action.
4. **Do bols map to words, or to phonemes?** Words gives meaning
   immediately. Phonemes gives unlimited vocabulary but no semantics and a
   much longer sequence per utterance. The word bank assumes words.

---

Sources:
- [Vocabulary Selection in AAC: Application of Core Vocabulary (KSHA)](https://ksha.org/docs/autism_intervention_handout_3.pdf)
- [Teaching with core words — AssistiveWare](https://assistiveware.com/blog/teaching-core-words-building-blocks-communication-and-curriculum)
- [Core Vocabulary in AAC: An SLP Guide to First Words](https://quicktalkerfreestyle.com/blog/aac-core-vocabulary-slp-guide/)
- [Core Vocabulary Classroom Word List — Van Tatenhove](http://www.vantatenhove.com/files/papers/VocabularyLists/CoreVocabularyClassroomChecklist.pdf)
- [Semantic compaction (Wikipedia)](https://en.wikipedia.org/wiki/Semantic_compaction)
