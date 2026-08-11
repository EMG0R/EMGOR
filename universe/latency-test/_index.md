---
id: emgor.latency-test
title: LATENCY PERCEPTION TEST
blurb: Find the smallest audio delay you can actually feel
parent: emgor
source: browser (Web Audio)
tags: [latency, psychoacoustics, web-audio, perception, measurement, guitar]
updated: 2026-08-11
draft: false
size: 1.4
launch: latency-test.html
---

# LATENCY PERCEPTION TEST

A guitar rig here went from 29 ms round trip to 8.02 ms over one night — measured properly, cable loopback, not arithmetic. The obvious next question is whether to keep going. Six milliseconds? Four?

That question has no answer in milliseconds. Latency only matters if you can feel it, and nobody knows where their own floor sits. So this measures the listener instead of the rig.

## What it does

Clicking this planet opens a test rather than a doc page. It runs a **two-interval forced-choice adaptive staircase** — the standard psychophysical method for finding a detection threshold, and the reason it's forced-choice is that "does this feel laggy?" measures your mood as much as your ears.

- You tap `space` four times in each of two takes. One of them has an added delay.
- You say which one felt delayed. You always have to pick.
- The delay shrinks when you're right twice running and grows when you're wrong, converging on the size you catch about 71% of the time.
- Attention checks at a blatant 140 ms and a binomial test against chance sit underneath it, so a run that was really guessing gets reported as guessing instead of as a number.

Fifteen scored trials, about sixty seconds of tapping.

## What it measures, and what it doesn't

It measures a **difference threshold** — the smallest *added* delay you can pick out. It does not measure your reaction time, which is around 200 ms and has nothing to do with any of this, and it does not measure rhythmic ability.

The browser's own output latency sits under both takes equally, so it cancels from the comparison rather than inflating the answer. The page reports that pedestal on screen and explains why it is not subtracted from the result. Keyboard and OS input latency remain genuinely unknown from inside a browser; that is stated rather than papered over.

The result is framed against the 8.02 ms figure directly: whether more latency work would be perceptible to you at all, or whether the rig is already under your floor.

## Status

Live, self-contained, runs entirely in the browser. Nothing is recorded and nothing leaves the page.
