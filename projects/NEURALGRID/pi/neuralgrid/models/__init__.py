"""neuralGrid MODEL LAYER (Phase 4).

Pure-numpy models only -- no PyTorch/TensorFlow, so the whole stack keeps
running free on the Pi. See docs/superpowers/specs/2026-07-06-neuralgrid-
design.md section 7 for the three-model plan; this package currently
implements 7.1, the Interface-VAE (`interface_vae.py`), plus the
interaction-feature extraction it consumes (`features.py`).
"""
