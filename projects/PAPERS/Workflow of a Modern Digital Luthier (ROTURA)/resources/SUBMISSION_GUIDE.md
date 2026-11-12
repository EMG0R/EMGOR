# ROTURA submission — guide and format comparison

Target: **Special Dossier "Advances in Digital and Interactive Arts —
Cultures, Communities and Territories of Media Art"**
Deadline **30 November 2026** · decision 28 Feb 2027 · publishes 30 Apr 2027
Guest editors: Alberto Sá (U. Minho); Adérito Fernandes-Marcos (U. São José, Macau)
Submit at https://publicacoes.ciac.pt/index.php/rotura/ (OJS — register first)

---

## 1. Does it need to be anonymized? YES

ROTURA runs **double-blind** review with two anonymous reviewers. Their
editorial norms are blunt about it:

> "Quaisquer referências no texto e **anexos** que possam identificar os autores
> poderão ter como consequência a sua **rejeição**."
> *(Any references in the text or attachments that could identify the authors
> may result in its rejection.)*

Note "anexos" — **attachments**, not just the manuscript. That means the figure
files and file metadata count.

> "A nota biográfica apenas deve ser inserida nos formulários iniciais. Não deve
> constar no ficheiro submetido para revisão."
> *(The biographical note goes only in the platform forms. It must not appear in
> the file submitted for review.)*

### Anonymity audit of what we're submitting

| Check | Result |
|---|---|
| Author names / emails / CalArts in prose | none found |
| Acknowledgements (named 5 people) | removed from manuscript |
| Word metadata (`dc:creator`, `dc:title`) | empty |
| Figure files — EXIF, GPS, camera, owner | clean |
| Self-citations that would unmask you | none (no prior work cited) |

One harmless residue: `Figure_06.tif` carries `software: Google` (Google Photos
processed the original). That names no person or institution — not a risk.

---

## 2. Format check against last year's published articles

Four real ROTURA articles are in `published_examples/`. Two are English, which
is what matters for you:

| File | Language | Why it's here |
|---|---|---|
| `rotura_548.pdf` | **English** | "Now You See Me: Public Service and Portuguese Radio on Instagram" — closest match to your situation |
| `rotura_356.pdf` | **English** | "A reading of the unreadable book: Xu Bing's Book from the Sky" — essay, 7 figures, good figure-caption reference |
| `rotura_526.pdf` | Portuguese | shows the bilingual pattern for contrast |
| `rotura_532.pdf` | Portuguese | ditto, two authors |

### What they do vs. what yours does

| Convention | Published English articles | Yours | |
|---|---|---|---|
| Title | English only | English only | OK |
| Abstract | `Abstract`, English only | same | OK |
| Keywords | `Keywords`, English only | same | OK |
| In-text citations | APA author-date — `(Xu, 2005)`, `(Lopes et al., 2023)` | same | OK |
| Reference list order | alphabetical, APA entries | same | OK |
| Reference heading | **"Bibliographic references"** | matched | fixed |
| Reference numbering | **`[1] [2] [3]`** prefixes | matched | fixed |
| Figure captions | `Figure 1. Caption.` bold number, trailing period | matched | fixed |
| Section headings | **unnumbered** — "Introduction" | numbered — "1. Introduction" | see below |

**The bilingual question is settled.** Both English articles open with
`Abstract` and `Keywords` once, in English. Neither has a `Resumo`. Only the
Portuguese-bodied articles carry both languages. So an English paper needs no
Portuguese — which is why the resumo was removed.

### The one open choice: section numbering

Published articles use unnumbered headings. Yours are numbered. I left them
numbered **on purpose**: your conclusion says "the emerging ecosystem and
workflow described in **Section 2**", and unnumbering would break that
cross-reference — which would mean editing your wording.

Numbered sections are common and no editor will reject over it. If you'd rather
match house style exactly, that sentence needs rewording first. Your call.

---

## 3. BLOCKERS — the paper is currently too short

This is the real obstacle, not formatting.

| | Yours | Required | Gap |
|---|---|---|---|
| Body text | **2,543** | 3,000–6,000 | **457 words short** |
| Abstract | **117** | 200–300 | **83 words short** |

Process Report is the right category — you're only under its floor, not over
its ceiling. Roughly one good paragraph per case study closes the body gap;
three or four sentences closes the abstract.

I have not written these for you, since you asked me to stop changing your
wording. Places that would take expansion naturally without inventing claims:

- **3.1 / 4.1 Purpose** — why you built each one, what was unsatisfying about
  what already existed
- **3.5 / 4.6 Evaluation** — what actually happened when you or others used them
- **5. Conclusions** — you raise the "demo problem" in one sentence; it can
  carry a paragraph
- **Abstract** — it currently describes the paper; it could also state what you
  found

---

## 4. Pre-submission checklist

- [ ] **Expand body to 3,000+ words**
- [ ] **Expand abstract to 200+ words**
- [ ] Re-export Figures 02, 05, 06, 09 at ~1900 px (see below)
- [ ] Register an account on the OJS platform
- [ ] Write biographical notes (max 150 words each) — into the **form**, not the file
- [ ] Add ORCIDs — every published author has one; get them at orcid.org
- [ ] Choose and word the **AI-use declaration** (mandatory; see `author_info.md`)
- [ ] Decide the Kapur duplicate (co-author *and* acknowledged — pick one)
- [ ] Upload `MY_SUBMISSION.docx` as the review file
- [ ] Upload the 9 TIFFs in `my_figures/` as supplementary files
- [ ] Confirm no author data in the uploaded file before submitting

### Photo resolution

Four photographs are below a usable print size at 300 dpi:

| Figure | Prints at | Source |
|---|---|---|
| 02 | 7.2 cm | NEPTR1.jpeg (856 px) |
| 05 | 8.7 cm | NEPTRshel2.jpeg (1030 px) |
| 06 | 6.2 cm | BOUBA1.jpg (729 px) |
| 09 | 9.9 cm | BOUBA2.png (1172 px) |

Column width wants ~1900 px. Re-export from camera originals or the Blender
renders, drop them into `figures/`, and re-run `build_figures.sh`. If the
originals are gone these still submit — they just print small.

---

## 5. Files

```
ROTURA_resources/
├── SUBMISSION_GUIDE.md      this file
├── MY_SUBMISSION.docx       the manuscript, anonymized, ready but short
├── my_figures/              9 TIFFs @ 300 dpi, metadata stripped
└── published_examples/      4 real ROTURA articles, 2 in English

~/Documents/_EMGOR_SYNTH/luthier_paper_v2/     working directory
├── rotura_submission.md     editable source
├── build_docx.sh            regenerate the .docx
├── build_figures.sh         regenerate the figure package
├── author_info.md           author data, bio notes, AI declaration
└── portuguese_translation_UNUSED.md   in case a form asks for it
```
