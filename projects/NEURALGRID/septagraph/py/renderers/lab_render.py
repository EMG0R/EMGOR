"""Thin CLI shim so the plan's documented path (``renderers/lab_render.py``)
also works. The real implementation lives in ``septagraph.renderers.lab``
(importable as a package module); this file just calls its ``main()``.

    python renderers/lab_render.py --map renderers/lab_map.json --lab 192.168.0.103:8001 --dry-run

is equivalent to:

    python -m septagraph.renderers.lab --map renderers/lab_map.json --lab 192.168.0.103:8001 --dry-run
"""
from septagraph.renderers.lab import main

if __name__ == "__main__":
    main()
