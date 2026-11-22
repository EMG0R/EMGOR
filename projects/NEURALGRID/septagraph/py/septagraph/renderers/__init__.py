"""Septagraph renderers: sound-engine-side implementations of the wire grammar.

Each module here consumes ``/sept/v1/*`` messages (Hit/Move/ManifestRequest,
see :mod:`septagraph.codec`) and turns them into whatever the target engine
speaks. See :mod:`septagraph.renderers.lab` for the CalArts Machine Lab
renderer (Task 3.2).
"""
