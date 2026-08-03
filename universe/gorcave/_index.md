---
id: emgor.gorcave
title: GORCAVE SERVER
blurb: A chat line into the machine watching the room
parent: emgor
source: gorcave (home Raspberry Pi, in progress)
tags: [raspberry-pi, home-automation, agent, chat, server, gorcave, in-progress]
updated: 2026-08-03
draft: true
launch: gorcave-chat.html
---

# GORCAVE SERVER

There's a Raspberry Pi in the house that isn't part of any of the instruments. It just sits there, watching.

Its hostname is `gorcave`. No relation to NEPTR, no relation to SPEAK's voice companion, nothing borrowed from the instrument side of the house. Just a machine that happens to be called what it is.

Right now it watches the lighting, the plants, and the network. Eventually you'll be able to just talk to it — open a chat, ask it to dim a room or check on a soil sensor, and it answers back. Nobody outside this house has used it yet. Most people won't even know it exists.

## What's here

Clicking this planet drops you straight into the chat widget instead of a doc page — a live-ish line to the agent running on `gorcave`, once it's actually reachable:

- A live agent running locally on `gorcave`, with eyes on lighting, plants, and network state.
- A chat interface to talk to it directly, instead of digging through logs or scripts.
- A passphrase gate in front of any of it — nobody's proposing an open line into someone's house. The widget already asks for one; the backend side of that gate isn't live yet.

## Status

Early. The chat widget exists now, but it's pointed at a placeholder backend URL — the Pi isn't reachable from the public internet yet. Until the Tailscale Funnel side is wired up on `gorcave`, this stays a marker with a door on it, not an open one.
