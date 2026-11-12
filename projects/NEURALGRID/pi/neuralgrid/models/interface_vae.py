"""Interface-VAE -- Phase 4, spec section 7.1.

A tiny pure-numpy VAE:

    features (FEATURE_DIM,) --encoder--> mu, logvar (LATENT_DIM,)
                                             |
                                    low-temperature sample
                                             |
    latent z (LATENT_DIM,) --decoder--> config vector (config.VECTOR_LEN,) in [0,1]

No PyTorch/TensorFlow -- forward pass is a handful of numpy matmuls, sized
so the whole thing is a few thousand parameters (see `param_count()`),
comfortably "much smaller than sub-1M". Runs free on the Pi.

Weights are plain numpy arrays, held in a flat dict so save/load is just
`np.savez` / `np.load` -- no framework-specific serialization.

"Low-temperature latent sampling" (spec: "supportive, anticipatory mirror
... stays close to the player's habits") means: at inference time we
mostly just use `mu` (temperature -> 0 is deterministic mirror of the
encoded habit) with a small amount of injected noise scaled by
`temperature * std`, not full-variance VAE sampling. Default temperature
is low (0.15) for exactly that reason.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Dict, Optional, Tuple

import numpy as np

from .features import FEATURE_DIM
from .. import config as config_mod

log = logging.getLogger("neuralgrid.models.interface_vae")

VECTOR_LEN = config_mod.VECTOR_LEN  # 89, imported so this module never hardcodes it
HIDDEN_DIM = 32
LATENT_DIM = 12
DEFAULT_TEMPERATURE = 0.15

_RNG_SEED = 1234  # fixed default init seed -- deterministic bootstrap weights until trained


def _xavier(rng: np.random.Generator, n_in: int, n_out: int) -> np.ndarray:
    scale = np.sqrt(2.0 / (n_in + n_out))
    return (rng.standard_normal((n_in, n_out)) * scale).astype(np.float32)


def _sigmoid(x: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-np.clip(x, -60.0, 60.0)))


def _tanh(x: np.ndarray) -> np.ndarray:
    return np.tanh(x)


class InterfaceVAE:
    """Tiny encoder/decoder pair. All weights live in `self.w`, a flat
    dict of numpy arrays, so save/load is trivial and the whole model is
    inspectable/printable without any framework."""

    PARAM_NAMES = (
        "enc_w1", "enc_b1",
        "enc_w_mu", "enc_b_mu",
        "enc_w_logvar", "enc_b_logvar",
        "dec_w1", "dec_b1",
        "dec_w2", "dec_b2",
    )

    def __init__(
        self,
        feature_dim: int = FEATURE_DIM,
        hidden_dim: int = HIDDEN_DIM,
        latent_dim: int = LATENT_DIM,
        vector_len: int = VECTOR_LEN,
        seed: int = _RNG_SEED,
        weights: Optional[Dict[str, np.ndarray]] = None,
    ):
        self.feature_dim = feature_dim
        self.hidden_dim = hidden_dim
        self.latent_dim = latent_dim
        self.vector_len = vector_len

        if weights is not None:
            self.w = {k: np.asarray(v, dtype=np.float32) for k, v in weights.items()}
            self._validate_shapes()
            return

        rng = np.random.default_rng(seed)
        self.w = {
            "enc_w1": _xavier(rng, feature_dim, hidden_dim),
            "enc_b1": np.zeros(hidden_dim, dtype=np.float32),
            "enc_w_mu": _xavier(rng, hidden_dim, latent_dim),
            "enc_b_mu": np.zeros(latent_dim, dtype=np.float32),
            "enc_w_logvar": _xavier(rng, hidden_dim, latent_dim),
            # start near-zero variance in log-space -> small initial sigma,
            # consistent with "stay close to the player" from day one.
            "enc_b_logvar": np.full(latent_dim, -2.0, dtype=np.float32),
            "dec_w1": _xavier(rng, latent_dim, hidden_dim),
            "dec_b1": np.zeros(hidden_dim, dtype=np.float32),
            "dec_w2": _xavier(rng, hidden_dim, vector_len),
            "dec_b2": np.zeros(vector_len, dtype=np.float32),
        }

    def _validate_shapes(self) -> None:
        expected = {
            "enc_w1": (self.feature_dim, self.hidden_dim),
            "enc_b1": (self.hidden_dim,),
            "enc_w_mu": (self.hidden_dim, self.latent_dim),
            "enc_b_mu": (self.latent_dim,),
            "enc_w_logvar": (self.hidden_dim, self.latent_dim),
            "enc_b_logvar": (self.latent_dim,),
            "dec_w1": (self.latent_dim, self.hidden_dim),
            "dec_b1": (self.hidden_dim,),
            "dec_w2": (self.hidden_dim, self.vector_len),
            "dec_b2": (self.vector_len,),
        }
        for name, shape in expected.items():
            if name not in self.w:
                raise ValueError(f"InterfaceVAE weights missing {name!r}")
            got = tuple(self.w[name].shape)
            if got != shape:
                raise ValueError(f"InterfaceVAE weight {name!r} shape {got} != expected {shape}")

    # -- forward pass ---------------------------------------------------

    def encode(self, features: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """features (feature_dim,) -> (mu, logvar), each (latent_dim,)."""
        x = np.asarray(features, dtype=np.float32).reshape(-1)
        if x.shape != (self.feature_dim,):
            raise ValueError(f"encode: expected features shape ({self.feature_dim},), got {x.shape}")
        h = _tanh(x @ self.w["enc_w1"] + self.w["enc_b1"])
        mu = h @ self.w["enc_w_mu"] + self.w["enc_b_mu"]
        logvar = h @ self.w["enc_w_logvar"] + self.w["enc_b_logvar"]
        return mu.astype(np.float32), logvar.astype(np.float32)

    def sample_latent(
        self,
        mu: np.ndarray,
        logvar: np.ndarray,
        temperature: float = DEFAULT_TEMPERATURE,
        rng: Optional[np.random.Generator] = None,
    ) -> np.ndarray:
        """Low-temperature reparameterized sample: z = mu + temperature *
        sigma * eps. temperature=0 -> deterministic mirror of mu (no
        exploration at all); the spec's low-temperature default (0.15)
        keeps most of that determinism while allowing gentle variation."""
        if temperature <= 0.0:
            return mu.astype(np.float32)
        rng = rng if rng is not None else np.random.default_rng()
        sigma = np.exp(0.5 * logvar)
        eps = rng.standard_normal(mu.shape).astype(np.float32)
        return (mu + temperature * sigma * eps).astype(np.float32)

    def decode(self, z: np.ndarray) -> np.ndarray:
        """latent (latent_dim,) -> config vector (vector_len,) in [0,1]
        (sigmoid output -- enum blocks decode via argmax downstream in
        config.from_vector, so soft/continuous logits here are fine per
        the config boundary contract)."""
        z = np.asarray(z, dtype=np.float32).reshape(-1)
        if z.shape != (self.latent_dim,):
            raise ValueError(f"decode: expected latent shape ({self.latent_dim},), got {z.shape}")
        h = _tanh(z @ self.w["dec_w1"] + self.w["dec_b1"])
        out = _sigmoid(h @ self.w["dec_w2"] + self.w["dec_b2"])
        return out.astype(np.float32)

    def infer(
        self,
        features: np.ndarray,
        temperature: float = DEFAULT_TEMPERATURE,
        rng: Optional[np.random.Generator] = None,
    ) -> np.ndarray:
        """features -> config vector, end to end. This is what a live
        stager calls once per beat."""
        mu, logvar = self.encode(features)
        z = self.sample_latent(mu, logvar, temperature=temperature, rng=rng)
        return self.decode(z)

    # -- persistence ---------------------------------------------------

    def save(self, path: str) -> None:
        np.savez(path, **self.w)

    @classmethod
    def load(cls, path: str) -> "InterfaceVAE":
        data = np.load(path)
        weights = {k: data[k] for k in cls.PARAM_NAMES}
        return cls(weights=weights)

    def param_count(self) -> int:
        return int(sum(v.size for v in self.w.values()))


def load_default(models_dir: Optional[str] = None) -> Optional[InterfaceVAE]:
    """Best-effort load of the bootstrap-trained weights shipped at
    neuralgrid/models/interface_vae.npz. Returns None (and logs once) if
    the file isn't there yet -- callers (ModelStager) must no-op in that
    case rather than crash the live loop."""
    if models_dir is None:
        models_dir = str(Path(__file__).resolve().parent)
    path = Path(models_dir) / "interface_vae.npz"
    if not path.exists():
        log.warning("interface_vae.npz not found at %s -- model brain unavailable", path)
        return None
    try:
        return InterfaceVAE.load(str(path))
    except Exception:  # noqa: BLE001
        log.warning("failed to load interface_vae.npz at %s", path, exc_info=True)
        return None
