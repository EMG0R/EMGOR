---
id: emgor.papers.season-2026-27.digital-luthier
title: Digital Luthier
blurb: The methodology paper — embedded audio, fabrication, and interaction design as one workflow. In submission to ROTURA.
parent: emgor.papers.season-2026-27
source: luthier_paper_v2/rotura_submission.md
links:
  - { label: "Read as paper", url: "papers/nime/digital-luthier.html" }
tags: [rotura, methodology, digital-lutherie, fabrication, interaction-design, neptr, bouba]
updated: 2026-09-21
draft: false
---

# Workflow of a Modern Digital Luthier

**Status: in submission.** A Process Report for *ROTURA — Revista de Comunicação, Cultura e Artes*, special dossier on Advances in Digital and Interactive Arts. Double-blind review. Co-authored with Ajay Kapur.

## Abstract

The parallel maturation of single-board computers, microcontrollers, audio-visual programming languages, fabrication techniques, and access to information has created an environment where form emerges as a coherent system rather than in sequential layers. This convergence enables individuals to rapidly develop self-contained interfaces where gesture, sound, graphics, and physical structure are implemented as direct extensions of artistic intention. The paper presents two case studies demonstrating this methodology — NEPTR, a handheld performance instrument with dense sensor input and synchronized audiovisual feedback, and BOUBA, a quadraphonic tactile sound object for therapeutic ambient synthesis — and argues that story-driven design across integrated domains produces coherent interactive identity.

## The argument

A threshold has been crossed, and it is not Moore's Law. Ten years ago a self-contained digital instrument demanded either significant resources or a compromise: sensor-rich controllers leaned on external computers, and self-contained devices gave up computational headroom or interface density. That gap has collapsed.

What replaces it is an ecosystem where platform selection serves interaction requirements instead of constraining them — a Teensy handling time-critical analog sensing while a Pi runs the expensive work, or a custom PCB carrying exactly the components an instrument needs. Enclosure stops being housing and becomes interface from the first stage of design.

The design literature gets *more* urgent in this environment, not less. Fels on intimacy emerging from mapping quality rather than parameter count; Hunt and Wanderley on expressivity living in gesture-sound relationships; Jordà on the tension between flexibility and identity; Cook's insistence that some instruments are meant to be stupid; Zappi and McPherson on constrained instruments producing richer engagement. When adding complexity costs nothing, restraint becomes the discipline that separates an instrument from a pile of features.

## The two case studies

**NEPTR** — a handheld performance instrument for synthesis, effects, and looping, supporting both handheld and floor-based interaction. A Pi 5 with a Teensy 4.0 on audio I/O and a Teensy 4.1 on controls. Stereo line-in normalled to two contact microphones mounted inside the shell, making the acoustic relationship to the instrument inescapable. The 3D-printed enclosure serves two ergonomics at once — game-controller grips for handheld play, flat pedalboard posture for foot control — at a material cost under \$30.

**BOUBA** — a quadraphonic sound object for therapeutic generative synthesis, about \$80 to reproduce. Four speakers from two PCM5102 DACs through four LM386 amplifiers, no external connection but power. Its presets operationalize published findings across four pillars: sleep, mood, productivity, and autonomic regulation.

Both are evaluated against O'Modhrain's framework — playability, learnability, sonic range, robustness — and neither is presented as finished or high-quality. That is the point. Both prototypes were completed in under four months combined, for under \$300 combined, without institutional resources or commercial software.

## A note on NEPTR's two lives

The NEPTR documented in this paper runs ChucK for synthesis with a Python GUI over OSC. That is an earlier instrument than the one described in [Performance System of a Modern Digital Luthier](/papers/season-2026-27/neptr-performance-system), which runs Csound on DEMIURGE with a Rust supervisor underneath.

This is not a contradiction to be resolved. The case study documents the prototype at the moment it demonstrated the workflow; the performance-system paper documents what that prototype became. Both are true of their own moment.

## Where it sits

This is the paper that states the season's argument out loud. [DemiurgeOS](/papers/season-2026-27/demiurgeos) and [the NEPTR paper](/papers/season-2026-27/neptr-performance-system) are evidence for it from underneath and above; [the thesis](/papers/season-2026-27/thesis) is what all three mean together.
