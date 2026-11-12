"""ModelStager -- Phase 4, the neural "brain" stager.

Mirrors live/stager_manual.py's ManualStager shape (same public interface:
construct with `engine`, call `tick()` once per engine loop iteration,
talk to the engine only through `engine.stage_config()`) but instead of
cycling a hand-authored demo sequence, it periodically:

  1. reads the engine's rolling interaction-feature window
     (`engine.interaction_features`, fed live by LiveEngine's own
     `_on_key`/`_on_analog` callbacks -- see engine.py),
  2. runs it through the tiny numpy Interface-VAE (`interface_vae.infer`),
  3. stages the resulting config vector.

The actual swap into the *active* config still only happens on the next
downbeat via LiveEngine's existing Phase 3 mechanism (`stage_config` +
`_on_clock_downbeat`) -- ModelStager never touches `engine.config`
directly, exactly like ManualStager.

OFF THE HOT PATH: `tick()` is called from `run_forever(extra_tick=...)`,
i.e. once per ~5ms loop iteration just like ManualStager, but the actual
inference work (encode+decode, a handful of tiny numpy matmuls on a
~4.5k-parameter network) only runs at most once every `interval_seconds`
(default: matches roughly one beat at a typical tempo) -- not every tick.
The forward pass itself is O(feature_dim*hidden + hidden*latent +
latent*hidden + hidden*vector_len) ~= a few thousand FLOPs, microseconds
on a Pi, so even the "do work" tick never meaningfully delays the next
`engine.tick()`. If the model failed to load (missing .npz), `tick()`
degrades to a one-time logged no-op forever -- it will never raise into
the live loop.
"""

from __future__ import annotations

import logging
import time
from typing import Optional

from ..models.interface_vae import InterfaceVAE, load_default

log = logging.getLogger("neuralgrid.live.stager_model")

SOURCE = "brain"

# Roughly "about once per beat" per the build spec, without coupling to
# the actual tempo (the downbeat gate in engine.py is what actually
# quantizes the swap to the beat -- this just controls how often we
# bother re-running inference between swaps).
DEFAULT_INTERVAL_SECONDS = 0.5

_UNSET = object()  # sentinel: distinguishes "no model arg given -> auto-load" from
                    # "model=None explicitly given -> force the no-op state" (used by tests)


class ModelStager:
    def __init__(
        self,
        engine,
        model=_UNSET,
        interval_seconds: float = DEFAULT_INTERVAL_SECONDS,
        temperature: Optional[float] = None,
    ):
        self.engine = engine
        self.interval_seconds = interval_seconds
        self.temperature = temperature  # None -> InterfaceVAE's own low-temperature default
        self.model: Optional[InterfaceVAE] = load_default() if model is _UNSET else model
        self._warned_missing = self.model is None
        self._next_at = time.monotonic() + interval_seconds
        self._n_staged = 0

        if self.model is None:
            log.warning(
                "ModelStager: interface_vae.npz not found -- brain stager will no-op "
                "(run pi/train_interface_vae.py to produce it). Falling back to no-op."
            )

    def stage_now(self) -> bool:
        """Run inference once right now and stage the result, regardless
        of the interval timer. Returns whether the engine accepted it."""
        if self.model is None:
            return False
        features_window = getattr(self.engine, "interaction_features", None)
        if features_window is None:
            log.warning("ModelStager: engine has no interaction_features window -- no-op")
            return False
        try:
            features = features_window.compute_features()
            kwargs = {} if self.temperature is None else {"temperature": self.temperature}
            vec = self.model.infer(features, **kwargs)
        except Exception:  # noqa: BLE001 -- never let inference crash the live loop
            log.warning("ModelStager: inference failed, skipping this cycle", exc_info=True)
            return False
        ok = self.engine.stage_config(vec, source=SOURCE)
        self._n_staged += 1
        log.info("brain stager: staged config #%d (accepted=%s)", self._n_staged, ok)
        return ok

    def tick(self, now: Optional[float] = None) -> None:
        """Call once per engine loop iteration (via run_forever(extra_tick=
        this)). Cheap no-op most calls; runs inference + stages at most
        once every `interval_seconds`. Never blocks or raises."""
        if self.model is None:
            return
        now = now if now is not None else time.monotonic()
        if now >= self._next_at:
            self.stage_now()
            self._next_at = now + self.interval_seconds
