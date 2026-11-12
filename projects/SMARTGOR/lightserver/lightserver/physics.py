"""1-D particle physics: gravity, wind, turbulence, attractors, springs,
elastic bounces (walls + particle-particle), emitters, lifetimes.

Units are physical-ish: positions in px (0..num_pixels), velocities px/s,
accelerations px/s^2. All state is float64 struct-of-arrays, vectorized.
"""
from __future__ import annotations

import math

import numpy as np

from .color import Palette, hex_to_linear
from .noise import signed_noise

# color_mode codes (int8, stored per-particle so SoA stays vectorized):
#   0 static   -> use the spawned color as-is (legacy behavior)
#   1 velocity -> color driven by current speed (palette lookup)
#   2 age      -> color driven by age/lifetime fraction (palette lookup)
#   3 index    -> color driven by a fixed per-particle palette offset,
#                 assigned at spawn (each particle keeps its own hue)
CMODE = {"static": 0, "velocity": 1, "age": 2, "index": 3}


class Emitter:
    def __init__(self, cfg: dict, rng: np.random.Generator):
        self.rng = rng
        pos = cfg.get("pos", 0)
        self.pos_range = tuple(pos) if isinstance(pos, (list, tuple)) else (float(pos), float(pos))
        self.rate = float(cfg.get("rate", 5.0))            # particles / s
        self.vel = float(cfg.get("vel", 0.0))              # px/s mean
        self.vel_jitter = float(cfg.get("vel_jitter", 0.0))
        self.lifetime = cfg.get("lifetime", None)          # s (None = immortal)
        self.lifetime_jitter = float(cfg.get("lifetime_jitter", 0.0))
        colors = cfg.get("colors") or [cfg.get("color", "#ffffff")]
        self.colors = np.stack([hex_to_linear(c) for c in colors])
        self.size = float(cfg.get("size", 1.5))            # gaussian sigma, px
        self.size_jitter = float(cfg.get("size_jitter", 0.0))
        self.mass = float(cfg.get("mass", 1.0))
        self.burst = int(cfg.get("burst", 0))              # spawned once at t=0
        # generative multi-color: see CMODE above. "index" mode gets each
        # particle a random-but-fixed palette offset so a burst of particles
        # comes out as a spread of hues rather than one flat color.
        self.cmode = CMODE.get(cfg.get("color_mode", "static"), 0)
        self.hue_drift = float(cfg.get("hue_drift", 0.0))  # extra cycles/s per particle
        self._acc = 0.0
        self._burst_done = False

    def due(self, dt: float) -> int:
        n = 0
        if not self._burst_done:
            n += self.burst
            self._burst_done = True
        self._acc += self.rate * dt
        whole = int(self._acc)
        self._acc -= whole
        return n + whole

    def spawn_arrays(self, n: int):
        r = self.rng
        pos = r.uniform(self.pos_range[0], self.pos_range[1], n)
        vel = self.vel + r.normal(0.0, self.vel_jitter or 1e-9, n) * (1 if self.vel_jitter else 0)
        if self.lifetime is None:
            life = np.full(n, np.inf)
        else:
            life = np.maximum(0.05, self.lifetime + r.normal(0, self.lifetime_jitter or 1e-9, n)
                              * (1 if self.lifetime_jitter else 0))
        color = self.colors[r.integers(0, len(self.colors), n)]
        size = np.maximum(0.3, self.size + r.normal(0, self.size_jitter or 1e-9, n)
                          * (1 if self.size_jitter else 0))
        mass = np.full(n, self.mass)
        cmode = np.full(n, self.cmode, dtype=np.int8)
        color_u = r.uniform(0.0, 1.0, n)  # fixed per-particle palette offset ("index" mode)
        hue_drift = np.full(n, self.hue_drift)
        return pos, vel, life, color, size, mass, cmode, color_u, hue_drift


class ParticleSystem:
    """Struct-of-arrays particle sim over the 1-D strip world [0, n)."""

    def __init__(self, num_pixels: int, physics: dict | None = None,
                 emitters: list[dict] | None = None,
                 initial: list[dict] | None = None,
                 springs: list[dict] | None = None,
                 seed: int | None = None,
                 palette: list[str] | None = None,
                 palette_speed: float = 0.0):
        self.n = num_pixels
        p = physics or {}
        self.rng = np.random.default_rng(seed)

        # Generative multi-color: an optional layer-wide palette that
        # emitters/particles can opt into via "color_mode" (see CMODE).
        # When absent, particles keep their spawned static color exactly as
        # before (fully backward compatible).
        self.palette = Palette(palette) if palette else None
        self.palette_speed = float(palette_speed)  # palette cycles/sec, generative motion

        # Ring topology: with wrap=true the world is a closed loop — positions
        # are taken modulo n, there are no walls (bounce is ignored), and the
        # splat kernel wraps across the 0/n boundary. Used by ring-space
        # scenes so effects cross the strip seam seamlessly.
        self.wrap = bool(p.get("wrap", False))

        g = p.get("gravity") or {}
        self.gravity_mode = g.get("mode", "direction") if g else None
        self.gravity_accel = float(g.get("accel", 0.0)) if g else 0.0
        self.gravity_point = float(g.get("point", num_pixels / 2)) if g else 0.0

        self.damping = float(p.get("damping", 0.0))        # 1/s

        b = p.get("bounce") or {}
        self.bounce = bool(b.get("enabled", True)) if isinstance(b, dict) else bool(b)
        self.restitution = float(b.get("restitution", 0.85)) if isinstance(b, dict) else 0.85

        self.collisions = bool(p.get("collisions", False))

        w = p.get("wind") or {}
        self.wind_accel = float(w.get("accel", 0.0))
        self.wind_vary_hz = float(w.get("vary_hz", 0.0))
        self.wind_vary = float(w.get("vary_amount", 0.0))

        t = p.get("turbulence") or {}
        self.turb_accel = float(t.get("accel", 0.0))
        self.turb_scale = max(1.0, float(t.get("scale", 60.0)))
        self.turb_speed = float(t.get("speed", 0.5))

        self.attractors = [
            {"pos": float(a.get("pos", num_pixels / 2)),
             "strength": float(a.get("strength", 0.0)),
             "falloff": a.get("falloff", "constant"),
             "radius": max(1.0, float(a.get("radius", num_pixels)))}
            for a in (p.get("attractors") or [])
        ]

        # Orbit drive: organic scripted velocity for ring travellers. The
        # whole layer's particles chase a shared target velocity whose
        # magnitude wanders between min_speed and top_speed (two
        # incommensurate sine periods -> non-repeating feel) and whose
        # direction reverses as a randomized event ~reverses_per_min times
        # a minute, smoothly (exponential chase, time constant smooth_s:
        # decelerate -> flip -> accelerate, never teleports).
        dv = p.get("drive") or {}
        self.drive = None
        if dv:
            self.drive = {
                "top": float(dv.get("top_speed", 100.0)),
                "min": float(dv.get("min_speed", 30.0)),
                "rev_per_min": max(0.0, float(dv.get("reverses_per_min", 1.5))),
                "smooth": max(0.1, float(dv.get("smooth_s", 2.5))),
                "t1": max(1.0, float(dv.get("mod_period_s", 37.0))),
                "t2": max(1.0, float(dv.get("mod_period2_s", 23.0))),
            }
            self._drive_dir = 1.0
            self._next_flip = self._draw_flip(0.0)

        self.emitters = [Emitter(e, self.rng) for e in (emitters or [])]
        self.spring_cfgs = springs or []

        # struct-of-arrays state
        z = np.zeros(0)
        self.pos, self.vel, self.mass, self.size = z.copy(), z.copy(), z.copy(), z.copy()
        self.age, self.life = z.copy(), z.copy()
        self.color = np.zeros((0, 3), dtype=np.float32)
        self.cmode = np.zeros(0, dtype=np.int8)
        self.color_u = z.copy()
        self.hue_drift = z.copy()
        # Per-particle monotonic id, assigned at spawn and preserved through
        # _compact() (never reused/reassigned) -- lets an external consumer
        # (e.g. the semantic tap side-channel, see taps.py/semantic.py)
        # track "the same particle" frame to frame by id, independent of its
        # current index in the SoA arrays.
        self.pid = np.zeros(0, dtype=np.int64)
        self._next_pid = 0

        for pc in (initial or []):
            self._append(
                pos=float(pc.get("pos", num_pixels / 2)),
                vel=float(pc.get("vel", 0.0)),
                life=float(pc["lifetime"]) if pc.get("lifetime") is not None else np.inf,
                color=hex_to_linear(pc.get("color", "#ffffff")),
                size=float(pc.get("size", 1.5)),
                mass=float(pc.get("mass", 1.0)),
                cmode=CMODE.get(pc.get("color_mode", "static"), 0),
                color_u=float(self.rng.uniform(0.0, 1.0)),
                hue_drift=float(pc.get("hue_drift", 0.0)),
            )

    # -- state management -------------------------------------------------
    def _append(self, pos, vel, life, color, size, mass, cmode=0, color_u=0.0, hue_drift=0.0):
        self.pos = np.append(self.pos, pos)
        self.vel = np.append(self.vel, vel)
        self.life = np.append(self.life, life)
        self.age = np.append(self.age, 0.0)
        self.size = np.append(self.size, size)
        self.mass = np.append(self.mass, mass)
        self.color = np.vstack([self.color, np.asarray(color, dtype=np.float32).reshape(1, 3)])
        self.cmode = np.append(self.cmode, np.int8(cmode))
        self.color_u = np.append(self.color_u, color_u)
        self.hue_drift = np.append(self.hue_drift, hue_drift)
        self.pid = np.append(self.pid, self._next_pid)
        self._next_pid += 1

    def _append_arrays(self, pos, vel, life, color, size, mass, cmode=None, color_u=None, hue_drift=None):
        n = len(pos)
        self.pos = np.concatenate([self.pos, pos])
        self.vel = np.concatenate([self.vel, vel])
        self.life = np.concatenate([self.life, life])
        self.age = np.concatenate([self.age, np.zeros(n)])
        self.size = np.concatenate([self.size, size])
        self.mass = np.concatenate([self.mass, mass])
        self.color = np.vstack([self.color, color.astype(np.float32)])
        self.cmode = np.concatenate([self.cmode, cmode if cmode is not None
                                     else np.zeros(n, dtype=np.int8)])
        self.color_u = np.concatenate([self.color_u, color_u if color_u is not None
                                       else np.zeros(n)])
        self.hue_drift = np.concatenate([self.hue_drift, hue_drift if hue_drift is not None
                                         else np.zeros(n)])
        new_ids = np.arange(self._next_pid, self._next_pid + n, dtype=np.int64)
        self.pid = np.concatenate([self.pid, new_ids])
        self._next_pid += n

    def _compact(self, keep: np.ndarray):
        self.pos, self.vel = self.pos[keep], self.vel[keep]
        self.life, self.age = self.life[keep], self.age[keep]
        self.size, self.mass = self.size[keep], self.mass[keep]
        self.color = self.color[keep]
        self.cmode = self.cmode[keep]
        self.color_u = self.color_u[keep]
        self.hue_drift = self.hue_drift[keep]
        self.pid = self.pid[keep]

    @property
    def count(self) -> int:
        return len(self.pos)

    def _draw_flip(self, t: float) -> float:
        """Next reversal time: mean 60/rev_per_min seconds, jittered."""
        if not self.drive or self.drive["rev_per_min"] <= 0:
            return math.inf
        mean = 60.0 / self.drive["rev_per_min"]
        return t + mean * float(self.rng.uniform(0.55, 1.45))

    # -- simulation --------------------------------------------------------
    def step(self, dt: float, t: float):
        # spawn
        for em in self.emitters:
            n = em.due(dt)
            if n > 0 and self.count + n <= 4000:  # hard cap for safety
                self._append_arrays(*em.spawn_arrays(n))

        if self.count:
            acc = np.zeros_like(self.pos)

            # gravity
            if self.gravity_accel:
                if self.gravity_mode == "point":
                    acc += self.gravity_accel * np.sign(self.gravity_point - self.pos)
                else:
                    acc += self.gravity_accel  # signed: negative pulls toward px 0

            # wind (time-varying)
            if self.wind_accel:
                mod = 1.0
                if self.wind_vary_hz:
                    mod = (1.0 - self.wind_vary) + self.wind_vary * math.sin(
                        2 * math.pi * self.wind_vary_hz * t)
                acc += self.wind_accel * mod

            # turbulence — per-particle noise force
            if self.turb_accel:
                acc += self.turb_accel * signed_noise(
                    self.pos / self.turb_scale, t * self.turb_speed, octaves=2)

            # attractors / repulsors
            for a in self.attractors:
                d = a["pos"] - self.pos
                dist = np.abs(d) + 1e-6
                direction = np.sign(d)
                if a["falloff"] == "inverse_square":
                    mag = a["strength"] / np.maximum(1.0, (dist / 10.0) ** 2)
                elif a["falloff"] == "linear":
                    mag = a["strength"] * np.clip(1.0 - dist / a["radius"], 0.0, 1.0)
                else:
                    mag = np.where(dist <= a["radius"], a["strength"], 0.0)
                acc += direction * mag

            # springs (forces -> acceleration via mass)
            for s in self.spring_cfgs:
                k = float(s.get("k", 20.0))
                c = float(s.get("damping", 1.0))
                rest = float(s.get("rest", 0.0))
                ia = int(s.get("a", 0))
                if ia >= self.count:
                    continue
                if "b" in s and s["b"] is not None:
                    ib = int(s["b"])
                    if ib >= self.count:
                        continue
                    ext = (self.pos[ib] - self.pos[ia]) - rest
                    relv = self.vel[ib] - self.vel[ia]
                    f = k * ext + c * relv
                    acc[ia] += f / self.mass[ia]
                    acc[ib] -= f / self.mass[ib]
                else:
                    anchor = float(s.get("anchor", self.n / 2))
                    ext = (self.pos[ia] - anchor) - rest
                    f = -k * ext - c * self.vel[ia]
                    acc[ia] += f / self.mass[ia]

            # integrate (semi-implicit Euler)
            self.vel += acc * dt
            if self.damping:
                self.vel *= math.exp(-self.damping * dt)

            # orbit drive: chase the scripted target velocity
            if self.drive is not None:
                dr = self.drive
                if t >= self._next_flip:
                    self._drive_dir *= -1.0
                    self._next_flip = self._draw_flip(t)
                m = 0.5 + 0.5 * math.sin(2 * math.pi * t / dr["t1"]
                                         + 1.7 * math.sin(2 * math.pi * t / dr["t2"]))
                target = self._drive_dir * (dr["min"] + (dr["top"] - dr["min"]) * m)
                self.vel += (target - self.vel) * min(1.0, dt / dr["smooth"])

            self.pos += self.vel * dt

            # walls (or ring wrap-around)
            hi = self.n - 1.0
            if self.wrap:
                self.pos %= self.n
            elif self.bounce:
                low = self.pos < 0
                self.pos[low] = -self.pos[low]
                self.vel[low] = -self.vel[low] * self.restitution
                high = self.pos > hi
                self.pos[high] = 2 * hi - self.pos[high]
                self.vel[high] = -self.vel[high] * self.restitution
                np.clip(self.pos, 0.0, hi, out=self.pos)

            # particle-particle elastic collisions
            if self.collisions and self.count > 1:
                self._collide()

            # age & cull
            self.age += dt
            keep = self.age < self.life
            if not self.bounce and not self.wrap:
                keep &= (self.pos > -20) & (self.pos < self.n + 20)
            if not keep.all():
                self._compact(keep)

    def _collide(self):
        order = np.argsort(self.pos)
        p, v, m, s = self.pos[order], self.vel[order], self.mass[order], self.size[order]
        for i in range(len(p) - 1):
            j = i + 1
            gap = p[j] - p[i]
            touch = (s[i] + s[j]) * 0.75
            if gap < touch and v[i] > v[j]:  # overlapping and approaching
                m1, m2, v1, v2 = m[i], m[j], v[i], v[j]
                inv = 1.0 / (m1 + m2)
                nv1 = ((m1 - m2) * v1 + 2 * m2 * v2) * inv
                nv2 = ((m2 - m1) * v2 + 2 * m1 * v1) * inv
                v[i], v[j] = nv1 * self.restitution, nv2 * self.restitution
                push = (touch - gap) * 0.5
                p[i] -= push
                p[j] += push
        self.pos[order], self.vel[order] = p, v

    # -- rendering ---------------------------------------------------------
    def _dynamic_colors(self, t: float) -> np.ndarray:
        """Resolve final per-particle color: static particles keep their
        spawned color; particles with color_mode != static are looked up
        live from the layer palette (see CMODE), with palette_speed and
        per-particle hue_drift adding continuous generative motion."""
        if self.palette is None or not self.count:
            return self.color
        dynamic = self.cmode != 0
        if not dynamic.any():
            return self.color
        vel_u = 0.5 + 0.5 * np.tanh(self.vel / 60.0)
        with np.errstate(invalid="ignore", divide="ignore"):
            age_u = np.where(np.isfinite(self.life) & (self.life > 0),
                             np.clip(self.age / np.maximum(self.life, 1e-6), 0.0, 1.0),
                             (self.age * 0.2) % 1.0)
        u = np.select(
            [self.cmode == 1, self.cmode == 2, self.cmode == 3],
            [vel_u, age_u, self.color_u],
            default=self.color_u,
        )
        u = u + t * self.palette_speed + t * self.hue_drift
        dyn = self.palette.lookup(u)
        return np.where(dynamic[:, None], dyn, self.color).astype(np.float32)

    def resolved_colors(self, t: float) -> np.ndarray:
        """Public wrapper around `_dynamic_colors` for external consumers
        (e.g. the taps.py/semantic.py side-channel) that need the final
        per-particle color without duplicating the CMODE resolution
        logic. Same return shape/semantics as `_dynamic_colors`."""
        return self._dynamic_colors(t)

    def render(self, out: np.ndarray, falloff: str = "gaussian", fade_out: bool = True,
              t: float = 0.0):
        """Additively splat particles into out (n,3) linear HDR."""
        if not self.count:
            return
        xs = np.arange(self.n, dtype=np.float64)
        colors = self._dynamic_colors(t)
        for i in range(self.count):
            p, sigma = self.pos[i], max(0.35, self.size[i])
            gain = 1.0
            if fade_out and np.isfinite(self.life[i]) and self.life[i] > 0:
                frac = 1.0 - self.age[i] / self.life[i]
                gain = max(0.0, frac) ** 1.5
            if gain <= 0:
                continue
            r = int(math.ceil(sigma * 3.5)) + 1
            if self.wrap:
                # ring: splat with wrapped indices so the kernel crosses 0/n
                idx = np.arange(int(p) - r, int(p) + r + 1)
                d = idx - p
                if falloff == "hard":
                    w = (np.abs(d) <= sigma).astype(np.float64)
                else:
                    w = np.exp(-(d * d) / (2.0 * sigma * sigma))
                np.add.at(out, idx % self.n,
                          (w[:, None] * gain).astype(np.float32) * colors[i])
                continue
            lo = max(0, int(p) - r)
            hi = min(self.n, int(p) + r + 1)
            if hi <= lo:
                continue
            d = xs[lo:hi] - p
            if falloff == "hard":
                w = (np.abs(d) <= sigma).astype(np.float64)
            else:
                w = np.exp(-(d * d) / (2.0 * sigma * sigma))
            out[lo:hi] += (w[:, None] * gain).astype(np.float32) * colors[i]
