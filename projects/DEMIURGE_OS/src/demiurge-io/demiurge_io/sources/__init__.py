"""sources — one driver per input device. Each exposes:

    .id            short source-family name (informational)
    .descriptors   dict[str, SourceDescriptor] — one per emitted sensor id
    .run(on_reading, on_control)   blocking; call from its own thread

Sensors go through on_reading(Reading) -> conditioning -> router. Anything
that is a discrete UI control event (not this package's concern — see
teensy.py's module docstring) goes through on_control(...) and bypasses
conditioning/routing entirely.
"""
