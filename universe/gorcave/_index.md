---
id: emgor.gorcave
title: GORCAVE SERVER
blurb: A chat line into the machine watching the room
parent: emgor
source: gorcave (home Raspberry Pi)
tags: [raspberry-pi, home-automation, agent, chat, server, gorcave]
updated: 2026-08-03
draft: false
launch: gorcave-chat.html
---

# GORCAVE SERVER

There's a Raspberry Pi in the house that isn't part of any of the instruments. It just sits there, watching.

Its hostname is `gorcave`. No relation to NEPTR, no relation to SPEAK's voice companion, nothing borrowed from the instrument side of the house. Just a machine that happens to be called what it is.

It watches the lighting, the plants, and the network. You can talk to it — open a chat, ask it what it knows, and it answers back.

## What's here

Clicking this planet drops you straight into a chat widget instead of a doc page — a live line to the agent running on `gorcave`:

- A live agent running locally on `gorcave`, with eyes on lighting, plants, and network state.
- A chat interface to talk to it directly, instead of digging through logs or scripts.
- A passphrase gate in front of all of it. Nobody's proposing an open line into someone's house.

## Status

Live. The chat routes over a private tunnel straight to the Pi in the house.
