#!/usr/bin/env python3
"""Bootstrap-train the Interface-VAE (Phase 4, spec section 7.1) on a
SYNTHETIC dataset before any real playing session has ever been logged --
"models are valid day one" (spec, "Training data").

Maps sampled interaction-feature windows, clustered around a few
interaction *modes*, to hand-authored target configs that make musical
sense for that mode:

  - hammering/direct-trigger  -> classic instrument layout, high density,
    punchy accent velocity curve, softpots ignored (menu_select).
  - sliding/expressive        -> scale-degree layout, softer fixed
    velocity, more echo, softpots doing pitch_bend.
  - idle/silence              -> exactly config.default_config() (the
    safety-floor behavior when nothing is happening yet).
  - balanced/hybrid           -> a middle-ground preset (moderate density,
    up-arp, isomorphic layout) for players who mix both sensor families.

Trains a tiny numpy VAE (see neuralgrid/models/interface_vae.py) with a
hand-rolled forward/backward pass -- no autodiff framework, this is a
~4.5k-parameter network on a few hundred samples, plain full-batch
gradient descent converges in well under a second.

    python3 pi/train_interface_vae.py            # trains, writes the .npz, prints loss
    python3 pi/train_interface_vae.py --selftest  # fast smoke test, no file I/O required
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

from neuralgrid import config as config_mod
from neuralgrid.models.features import FEATURE_DIM
from neuralgrid.models.interface_vae import InterfaceVAE, HIDDEN_DIM, LATENT_DIM

OUT_PATH = Path(__file__).resolve().parent / "neuralgrid" / "models" / "interface_vae.npz"

BETA = 0.02  # KL weight -- small: bootstrap priority is valid, sensible reconstructions
LEARNING_RATE = 0.3
EPOCHS = 4000
SEED = 42


# ---------------------------------------------------------------------------
# hand-authored target presets
# ---------------------------------------------------------------------------


def _preset_direct_punchy() -> Dict:
    cfg = config_mod.default_config()
    cfg.update(
        active_menu=0,  # instrument
        note_layout=1,  # rows_in_fourths -- classic direct-trigger instrument grid
        scale_id=1,  # major
        density=0.9,
        velocity_curve=3,  # accent -- punchy
        velocity_fixed=0.95,
        swing=0.05,
        arp_mode=0,  # off -- direct trigger, no arpeggiation
        echo_send=0.05,
        quantize_strength=0.0,
        led_palette=2,  # fire -- energetic
        led_brightness=1.0,
        softpot_fn=[0, 0, 0, 0],  # menu_select -- direct players ignore softpots for pitch
    )
    return cfg


def _preset_expressive_slide() -> Dict:
    cfg = config_mod.default_config()
    cfg.update(
        active_menu=0,
        note_layout=2,  # scale_degrees
        scale_id=6,  # minor_pentatonic -- forgiving, singable under a slide
        density=0.35,
        velocity_curve=0,  # fixed, soft touch
        velocity_fixed=0.5,
        swing=0.15,
        arp_mode=0,
        echo_send=0.4,
        quantize_strength=0.1,
        led_palette=3,  # ice -- calmer
        led_brightness=0.75,
        softpot_fn=[2, 2, 2, 2],  # pitch_bend -- expressive players ride the softpots
    )
    return cfg


def _preset_idle() -> Dict:
    return config_mod.default_config()


def _preset_balanced_hybrid() -> Dict:
    cfg = config_mod.default_config()
    cfg.update(
        active_menu=0,
        note_layout=3,  # isomorphic
        scale_id=4,  # mixolydian
        density=0.6,
        velocity_curve=1,  # row
        velocity_fixed=0.75,
        swing=0.1,
        arp_mode=1,  # up
        echo_send=0.2,
        quantize_strength=0.05,
        led_palette=4,  # purple
        led_brightness=0.9,
        softpot_fn=[1, 1, 1, 1],  # cc -- hybrid players use softpots as modulation
    )
    return cfg


PRESETS = {
    "direct": _preset_direct_punchy(),
    "expressive": _preset_expressive_slide(),
    "idle": _preset_idle(),
    "hybrid": _preset_balanced_hybrid(),
}


# ---------------------------------------------------------------------------
# synthetic feature-space samplers (directly in feature space -- these
# match what neuralgrid.models.features.FeatureWindow.compute_features
# would produce for that playing style, per its documented semantics)
# ---------------------------------------------------------------------------


def _clip01(a: np.ndarray) -> np.ndarray:
    return np.clip(a, 0.0, 1.0)


def _sample_direct(rng: np.random.Generator, n: int) -> np.ndarray:
    key_hit_rate = rng.uniform(0.6, 1.0, n)
    centroid_x = rng.uniform(0.0, 1.0, n)
    centroid_y = rng.uniform(0.0, 1.0, n)
    spread_x = rng.uniform(0.2, 0.6, n)
    spread_y = rng.uniform(0.2, 0.6, n)
    velocity_proxy = rng.uniform(0.5, 1.0, n)
    softpot_activity = rng.uniform(0.0, 0.2, n)
    softpot_entropy = rng.uniform(0.0, 0.3, n)
    softpot_slide_speed = rng.uniform(0.0, 0.15, n)
    interaction_mode = rng.uniform(0.0, 0.25, n)
    overall_density = rng.uniform(0.5, 1.0, n)
    silence = np.zeros(n)
    return np.stack(
        [key_hit_rate, centroid_x, centroid_y, spread_x, spread_y, velocity_proxy,
         softpot_activity, softpot_entropy, softpot_slide_speed, interaction_mode,
         overall_density, silence], axis=1,
    )


def _sample_expressive(rng: np.random.Generator, n: int) -> np.ndarray:
    key_hit_rate = rng.uniform(0.0, 0.2, n)
    centroid_x = rng.uniform(0.3, 0.7, n)
    centroid_y = rng.uniform(0.3, 0.7, n)
    spread_x = rng.uniform(0.0, 0.2, n)
    spread_y = rng.uniform(0.0, 0.2, n)
    velocity_proxy = rng.uniform(0.0, 0.4, n)
    softpot_activity = rng.uniform(0.6, 1.0, n)
    softpot_entropy = rng.uniform(0.4, 1.0, n)
    softpot_slide_speed = rng.uniform(0.5, 1.0, n)
    interaction_mode = rng.uniform(0.75, 1.0, n)
    overall_density = rng.uniform(0.3, 0.7, n)
    silence = np.zeros(n)
    return np.stack(
        [key_hit_rate, centroid_x, centroid_y, spread_x, spread_y, velocity_proxy,
         softpot_activity, softpot_entropy, softpot_slide_speed, interaction_mode,
         overall_density, silence], axis=1,
    )


def _sample_idle(rng: np.random.Generator, n: int) -> np.ndarray:
    base = rng.uniform(0.0, 0.05, (n, FEATURE_DIM))
    base[:, 1] = 0.5  # centroid_x neutral
    base[:, 2] = 0.5  # centroid_y neutral
    base[:, 9] = rng.uniform(0.4, 0.6, n)  # interaction_mode: no evidence -> neutral
    base[:, 11] = 1.0  # silence flag
    return base


def _sample_hybrid(rng: np.random.Generator, n: int) -> np.ndarray:
    key_hit_rate = rng.uniform(0.3, 0.6, n)
    centroid_x = rng.uniform(0.2, 0.8, n)
    centroid_y = rng.uniform(0.2, 0.8, n)
    spread_x = rng.uniform(0.2, 0.5, n)
    spread_y = rng.uniform(0.2, 0.5, n)
    velocity_proxy = rng.uniform(0.3, 0.6, n)
    softpot_activity = rng.uniform(0.3, 0.6, n)
    softpot_entropy = rng.uniform(0.3, 0.7, n)
    softpot_slide_speed = rng.uniform(0.2, 0.5, n)
    interaction_mode = rng.uniform(0.4, 0.6, n)
    overall_density = rng.uniform(0.3, 0.6, n)
    silence = np.zeros(n)
    return np.stack(
        [key_hit_rate, centroid_x, centroid_y, spread_x, spread_y, velocity_proxy,
         softpot_activity, softpot_entropy, softpot_slide_speed, interaction_mode,
         overall_density, silence], axis=1,
    )


SAMPLERS = {
    "direct": _sample_direct,
    "expressive": _sample_expressive,
    "idle": _sample_idle,
    "hybrid": _sample_hybrid,
}


def build_dataset(rng: np.random.Generator, n_per_cluster: int = 80) -> Tuple[np.ndarray, np.ndarray]:
    feats: List[np.ndarray] = []
    targets: List[np.ndarray] = []
    for name, sampler in SAMPLERS.items():
        f = _clip01(sampler(rng, n_per_cluster)).astype(np.float32)
        tgt_vec = config_mod.to_vector(PRESETS[name])
        # jitter the *scalar* fields of the target a little so the decoder
        # doesn't just memorize one point per cluster -- clamp back to
        # [0,1] afterwards (one-hot enum blocks are left exact; small
        # noise on those would only blur the argmax boundary, not help).
        t = np.tile(tgt_vec, (n_per_cluster, 1)).copy()
        scalar_noise = rng.normal(0.0, 0.02, t.shape).astype(np.float32)
        # only perturb where the "one-hot-ness" doesn't matter: cheap
        # approach is to perturb everything a tiny bit and rely on argmax
        # robustness downstream (from_vector uses argmax for enums).
        t = np.clip(t + scalar_noise, 0.0, 1.0).astype(np.float32)
        feats.append(f)
        targets.append(t)
    X = np.concatenate(feats, axis=0)
    T = np.concatenate(targets, axis=0)
    return X, T


# ---------------------------------------------------------------------------
# manual forward/backward training loop (no autodiff framework)
# ---------------------------------------------------------------------------


def train(
    model: InterfaceVAE,
    X: np.ndarray,
    T: np.ndarray,
    epochs: int = EPOCHS,
    lr: float = LEARNING_RATE,
    beta: float = BETA,
    seed: int = SEED,
    verbose: bool = True,
) -> float:
    rng = np.random.default_rng(seed)
    n, latent_dim = X.shape[0], model.latent_dim
    w = model.w

    final_loss = float("nan")
    for epoch in range(epochs):
        # -- forward --
        H1 = np.tanh(X @ w["enc_w1"] + w["enc_b1"])
        MU = H1 @ w["enc_w_mu"] + w["enc_b_mu"]
        LOGVAR = H1 @ w["enc_w_logvar"] + w["enc_b_logvar"]
        SIGMA = np.exp(0.5 * LOGVAR)
        EPS = rng.standard_normal(MU.shape).astype(np.float32)
        Z = MU + SIGMA * EPS

        H2 = np.tanh(Z @ w["dec_w1"] + w["dec_b1"])
        OUT = 1.0 / (1.0 + np.exp(-np.clip(H2 @ w["dec_w2"] + w["dec_b2"], -60, 60)))

        recon = float(np.mean((OUT - T) ** 2))
        kl = float(np.mean(-0.5 * (1.0 + LOGVAR - MU ** 2 - np.exp(LOGVAR))))
        loss = recon + beta * kl
        final_loss = loss

        if verbose and (epoch % 500 == 0 or epoch == epochs - 1):
            print(f"  epoch {epoch:5d}  recon={recon:.5f}  kl={kl:.5f}  loss={loss:.5f}")

        # -- backward --
        vector_len = T.shape[1]
        G_OUT = 2.0 * (OUT - T) / (n * vector_len)
        G_preout = G_OUT * OUT * (1.0 - OUT)
        grad_dec_w2 = H2.T @ G_preout
        grad_dec_b2 = G_preout.sum(axis=0)
        G_H2 = G_preout @ w["dec_w2"].T
        G_preh2 = G_H2 * (1.0 - H2 ** 2)
        grad_dec_w1 = Z.T @ G_preh2
        grad_dec_b1 = G_preh2.sum(axis=0)
        G_Z = G_preh2 @ w["dec_w1"].T

        denom = n * latent_dim
        dKL_dMU = MU / denom
        dKL_dLOGVAR = 0.5 * (np.exp(LOGVAR) - 1.0) / denom

        G_MU = G_Z + beta * dKL_dMU
        G_LOGVAR = G_Z * EPS * 0.5 * SIGMA + beta * dKL_dLOGVAR

        grad_enc_w_mu = H1.T @ G_MU
        grad_enc_b_mu = G_MU.sum(axis=0)
        grad_enc_w_logvar = H1.T @ G_LOGVAR
        grad_enc_b_logvar = G_LOGVAR.sum(axis=0)

        G_H1 = G_MU @ w["enc_w_mu"].T + G_LOGVAR @ w["enc_w_logvar"].T
        G_preh1 = G_H1 * (1.0 - H1 ** 2)
        grad_enc_w1 = X.T @ G_preh1
        grad_enc_b1 = G_preh1.sum(axis=0)

        grads = {
            "enc_w1": grad_enc_w1, "enc_b1": grad_enc_b1,
            "enc_w_mu": grad_enc_w_mu, "enc_b_mu": grad_enc_b_mu,
            "enc_w_logvar": grad_enc_w_logvar, "enc_b_logvar": grad_enc_b_logvar,
            "dec_w1": grad_dec_w1, "dec_b1": grad_dec_b1,
            "dec_w2": grad_dec_w2, "dec_b2": grad_dec_b2,
        }
        for k in w:
            w[k] = (w[k] - lr * grads[k]).astype(np.float32)

    return final_loss


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--epochs", type=int, default=EPOCHS)
    parser.add_argument("--lr", type=float, default=LEARNING_RATE)
    parser.add_argument("--out", default=str(OUT_PATH))
    parser.add_argument("--selftest", action="store_true", help="fast smoke test, no file I/O required")
    args = parser.parse_args()

    if args.selftest:
        ok = run_selftest()
        sys.exit(0 if ok else 1)

    rng = np.random.default_rng(SEED)
    model = InterfaceVAE(seed=SEED)
    print(f"Interface-VAE: {model.param_count()} parameters "
          f"(feature_dim={model.feature_dim}, hidden={model.hidden_dim}, "
          f"latent={model.latent_dim}, vector_len={model.vector_len})")

    X, T = build_dataset(rng)
    print(f"synthetic bootstrap dataset: {X.shape[0]} samples "
          f"({len(SAMPLERS)} clusters x {X.shape[0] // len(SAMPLERS)})")

    print(f"training for {args.epochs} epochs, lr={args.lr}, beta={BETA} ...")
    final_loss = train(model, X, T, epochs=args.epochs, lr=args.lr)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    model.save(str(out_path))
    print(f"final loss: {final_loss:.6f}")
    print(f"saved weights -> {out_path}")


# ---------------------------------------------------------------------------
# --selftest : fast smoke test, no npz written
# ---------------------------------------------------------------------------


def run_selftest() -> bool:
    print("=" * 70)
    print("train_interface_vae self-test (shapes + one gradient step reduces loss)")
    print("=" * 70)
    ok = True

    def check(label: str, cond: bool) -> None:
        nonlocal ok
        status = "PASS" if cond else "FAIL"
        print(f"  [{status}] {label}")
        ok = ok and cond

    rng = np.random.default_rng(0)
    model = InterfaceVAE(seed=0)
    X, T = build_dataset(rng, n_per_cluster=5)
    check("dataset feature shape", X.shape == (5 * len(SAMPLERS), FEATURE_DIM))
    check("dataset target shape", T.shape == (5 * len(SAMPLERS), config_mod.VECTOR_LEN))
    check("dataset features within [0,1]", bool(np.all(X >= 0.0) and np.all(X <= 1.0)))
    check("dataset targets within [0,1]", bool(np.all(T >= 0.0) and np.all(T <= 1.0)))

    loss_before = train(model, X, T, epochs=1, lr=0.0, verbose=False)
    loss_after = train(model, X, T, epochs=30, lr=0.3, verbose=False)
    check("a few gradient steps reduce the loss", loss_after < loss_before,)

    print()
    print("OVERALL:", "PASS" if ok else "FAIL")
    return ok


if __name__ == "__main__":
    main()
