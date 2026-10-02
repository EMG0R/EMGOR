---
id: emgor.papers.omniplex
title: 8. OMNIPLEX (2027+)
order: 8
blurb: One cable for audio, video, and control — a Rust transport, now folding into DemiurgeOS.
parent: emgor.papers
updated: 2026-09-21
read: papers/edit.html?id=omniplex
draft: false
---

Adapters and converters are used everywhere a single USB-C cable would do. Nearly every device on a modern stage or desk carries a USB-C port, and yet signal still routes through audio interfaces and video capture cards — rounds of unnecessary conversion between machines that are each already digital.

OMNIPLEX is the alternative: one cable and one system carrying audio, video, and control data between machines, with the stream arriving on the host as if it were native hardware. Devices negotiate roles on the link rather than having them fixed by whichever box owns the capture hardware, and the UI makes the bandwidth budget of a cable a first-class, inspectable object.
