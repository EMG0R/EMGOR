---
id: emgor.server
title: SERVER
blurb: A chat line into the machine watching the room
parent: emgor
source: _____pi (home Raspberry Pi, in progress)
tags: [raspberry-pi, home-automation, agent, chat, server, in-progress]
updated: 2026-08-03
draft: true
---

<!-- SUPERSEDED — renamed to universe/gorcave/_index.md -->

<!--
  launch: not set yet. When the real chat interface exists as an in-repo page
  (e.g. server.html), set `launch: server.html` here so clicking this
  planet jumps straight into the chat instead of opening the doc overlay.
  Leave unset until that page is real.
-->

# SERVER

There's a Raspberry Pi in the house that isn't part of any of the instruments. It just sits there, watching.

Its name is **Server** — hostname `gorcave`, username `server`, and yes, that's the whole joke. No relation to NEPTR, no relation to SPEAK's voice companion, nothing borrowed from the instrument side of the house. Just a machine that happens to be called what it is.

Right now it watches the lighting, the plants, and the network. Eventually you'll be able to just talk to it — open a chat, ask it to dim a room or check on a soil sensor, and it answers back. Nobody outside this house has used it yet. Most people won't even know it exists.

## What's here

Nothing you can touch yet. This is the front door to a project that's still mostly wiring:

- A live agent running locally on `gorcave`, with eyes on lighting, plants, and network state.
- A chat interface to talk to it directly, instead of digging through logs or scripts.
- A password/passkey gate in front of any of it — nobody's proposing an open line into someone's house. That gate isn't built. Until it exists, there's no chat to reach.

## Status

Early. The agent side is being assembled separately from this site. This page exists so the planet has a place to grow into — right now it's a marker, not a door.
