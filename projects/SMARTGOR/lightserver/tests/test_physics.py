"""Physics sanity: gravity accelerates, bounce reverses, damping bleeds energy,
collisions exchange momentum, emitters spawn at the configured rate."""
import math

import numpy as np

from lightserver.physics import ParticleSystem

N = 800


def make_ps(physics=None, initial=None, emitters=None, springs=None):
    return ParticleSystem(N, physics=physics, initial=initial,
                          emitters=emitters, springs=springs, seed=42)


def test_gravity_accelerates_particle():
    g = 100.0
    ps = make_ps(physics={"gravity": {"mode": "direction", "accel": g},
                          "bounce": {"enabled": False}},
                 initial=[{"pos": 10, "vel": 0, "color": "#ffffff"}])
    dt, steps = 0.01, 100  # 1 second
    t = 0.0
    for _ in range(steps):
        ps.step(dt, t)
        t += dt
    # semi-implicit Euler: v = g*t exactly
    assert math.isclose(ps.vel[0], g * 1.0, rel_tol=1e-9)
    # x ~ x0 + g t^2 / 2 (discrete sum slightly above continuous value)
    assert abs(ps.pos[0] - (10 + 0.5 * g)) < 1.5


def test_bounce_reverses_velocity_with_restitution():
    ps = make_ps(physics={"bounce": {"enabled": True, "restitution": 0.5}},
                 initial=[{"pos": 5, "vel": -200, "color": "#ffffff"}])
    ps.step(1 / 30, 0.0)  # crosses the 0 wall this step
    assert ps.vel[0] > 0, "velocity must reverse off the wall"
    assert math.isclose(ps.vel[0], 100.0, rel_tol=1e-9)  # 200 * 0.5
    assert ps.pos[0] >= 0


def test_bounce_high_wall():
    ps = make_ps(physics={"bounce": {"enabled": True, "restitution": 1.0}},
                 initial=[{"pos": 795, "vel": 300, "color": "#ffffff"}])
    ps.step(1 / 30, 0.0)
    assert ps.vel[0] < 0
    assert math.isclose(abs(ps.vel[0]), 300.0, rel_tol=1e-9)
    assert ps.pos[0] <= N - 1


def test_damping_decays_energy():
    d = 2.0
    ps = make_ps(physics={"damping": d, "bounce": {"enabled": True, "restitution": 1.0}},
                 initial=[{"pos": 400, "vel": 100, "color": "#ffffff"}])
    dt = 0.01
    e0 = 0.5 * ps.mass[0] * ps.vel[0] ** 2
    t = 0.0
    for _ in range(100):  # 1 second, stays away from the walls
        ps.step(dt, t)
        t += dt
    # v = v0 * e^(-d * t) exactly under per-step exponential damping
    assert math.isclose(ps.vel[0], 100 * math.exp(-d), rel_tol=1e-6)
    e1 = 0.5 * ps.mass[0] * ps.vel[0] ** 2
    assert e1 < e0 * 0.05


def test_particle_collision_exchanges_momentum():
    ps = make_ps(physics={"collisions": True, "bounce": {"enabled": True, "restitution": 1.0}},
                 initial=[{"pos": 390, "vel": 120, "color": "#ff0000", "size": 2, "mass": 1},
                          {"pos": 410, "vel": -120, "color": "#0000ff", "size": 2, "mass": 1}])
    t, dt = 0.0, 1 / 60
    for _ in range(30):
        ps.step(dt, t)
        t += dt
    # equal masses head-on: velocities swap (elastic)
    assert ps.vel[0] < 0 and ps.vel[1] > 0


def test_emitter_spawn_rate_and_lifetime_cull():
    ps = make_ps(emitters=[{"pos": [100, 200], "rate": 10, "vel": 0,
                            "lifetime": 0.5, "colors": ["#ffffff"]}])
    t, dt = 0.0, 1 / 30
    for _ in range(30):  # 1 second
        ps.step(dt, t)
        t += dt
    assert 5 <= ps.count <= 15  # ~10/s spawned, some already culled
    for _ in range(60):  # 2 more seconds with 0.5s lifetime -> steady state
        ps.step(dt, t)
        t += dt
    assert ps.count <= 10


def test_spring_pulls_toward_anchor():
    ps = make_ps(physics={"damping": 0.0, "bounce": {"enabled": True, "restitution": 1.0}},
                 initial=[{"pos": 300, "vel": 0, "color": "#ffffff"}],
                 springs=[{"a": 0, "anchor": 400, "k": 25, "damping": 4}])
    t, dt = 0.0, 1 / 60
    for _ in range(600):  # 10 s, overdamped-ish settle
        ps.step(dt, t)
        t += dt
    assert abs(ps.pos[0] - 400) < 5


def test_render_splats_at_particle_position():
    ps = make_ps(initial=[{"pos": 250, "vel": 0, "color": "#ffffff", "size": 1.5}])
    out = np.zeros((N, 3), dtype=np.float32)
    ps.render(out)
    assert out[250].max() > 0.9
    assert out[250].max() == out.max(axis=0).max()
    assert out[0].max() == 0.0
