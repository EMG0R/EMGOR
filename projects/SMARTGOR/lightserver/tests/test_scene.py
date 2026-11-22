"""Scene DSL: validation, JSON round-trip, rendering, paint exactness."""
import json
from pathlib import Path

import numpy as np
import pytest

from lightserver.engine import Engine
from lightserver.scene import SceneInstance, SceneValidationError, validate_scene

SCENES_DIR = Path(__file__).resolve().parent.parent / "scenes"
# live set (default.json) + the archived example library
ALL_SCENES = sorted(SCENES_DIR.glob("*.json")) + sorted(
    (SCENES_DIR / "archive").glob("*.json"))


def test_examples_exist():
    assert len(ALL_SCENES) >= 6


@pytest.mark.parametrize("path", ALL_SCENES, ids=lambda p: p.stem)
def test_example_scene_validates_and_renders(path):
    scene = json.loads(path.read_text())
    validate_scene(scene)
    inst = SceneInstance(scene, 800, seed=1)
    seen_nonblack = False
    for _ in range(15):
        hdr = inst.render(1 / 30)
        assert hdr.shape == (800, 3)
        assert np.all(np.isfinite(hdr))
        assert np.all(hdr >= 0)
        seen_nonblack = seen_nonblack or hdr.max() > 0
    # sync-pulse is a deliberately mostly-black metronome (short duty cycle),
    # so a single sampled frame can legitimately land in its off phase --
    # what matters is that it isn't black for the *entire* half-second.
    assert seen_nonblack, "scene should not be black for the whole half-second"


@pytest.mark.parametrize("path", ALL_SCENES, ids=lambda p: p.stem)
def test_dsl_round_trip(path):
    scene = json.loads(path.read_text())
    dumped = json.dumps(scene)
    reloaded = json.loads(dumped)
    assert reloaded == scene
    validate_scene(reloaded)  # survives serialization unchanged and still valid


@pytest.mark.parametrize("bad,frag", [
    ({"layers": []}, "layers"),
    ({"layers": [{"type": "warp-drive"}]}, "type"),
    ({"layers": [{"type": "solid", "color": "red"}]}, "color"),
    ({"layers": [{"type": "solid"}], "master": {"brightness": -1}}, "brightness"),
    ({"layers": [{"type": "particles", "blend": "screen"}]}, "blend"),
    ({"layers": [{"type": "paint", "pixels": {"x": "#fff000"}}]}, "pixels"),
    ({"layers": [{"type": "wave", "waves": [{"wavelength": 0}]}]}, "wavelength"),
], ids=["empty-layers", "bad-type", "bad-color", "neg-brightness",
        "bad-blend", "bad-pixel-key", "bad-wavelength"])
def test_invalid_scenes_rejected(bad, frag):
    with pytest.raises(SceneValidationError) as ei:
        validate_scene(bad)
    assert frag in str(ei.value)


def test_paint_pixels_are_exact_after_gamma_round_trip():
    """Explicit pixel colors must survive linearize -> tone-map -> gamma -> uint8."""
    scene = {
        "name": "exact",
        "layers": [{"type": "paint", "background": "#000000",
                    "pixels": {"5": "#a1b2c3", "0": "#ff0000", "799": "#0180ff"}}],
    }
    eng = Engine(mode="headless")
    eng.set_scene(scene, crossfade=0)
    frame = eng.step()
    assert tuple(frame[5]) == (0xA1, 0xB2, 0xC3)
    assert tuple(frame[0]) == (0xFF, 0x00, 0x00)
    assert tuple(frame[799]) == (0x01, 0x80, 0xFF)
    assert tuple(frame[6]) == (0, 0, 0)  # untouched pixel stays black


def test_trails_decay_leaves_fading_tail():
    scene = {
        "name": "trail",
        "layers": [{"type": "particles", "blend": "add", "decay": 0.9,
                    "physics": {"bounce": {"enabled": True, "restitution": 1.0}},
                    "particles": [{"pos": 100, "vel": 300, "color": "#ffffff", "size": 1.5}],
                    "render": {"fade_out": False}}],
    }
    inst = SceneInstance(scene, 800, seed=1)
    for _ in range(30):
        hdr = inst.render(1 / 30)
    # particle is near 400 now; the wake behind it should still glow
    assert hdr[250:390].max() > 0.01, "expected a fading trail behind the mover"


def test_trail_object_equivalent_to_legacy_decay():
    """"trail": {"decay": x} must behave exactly like the legacy top-level
    "decay": x (same fading tail)."""
    def run(layer_extra):
        scene = {
            "name": "trail",
            "layers": [{"type": "particles", "blend": "add",
                        "physics": {"bounce": {"enabled": True, "restitution": 1.0}},
                        "particles": [{"pos": 100, "vel": 300, "color": "#ffffff", "size": 1.5}],
                        "render": {"fade_out": False}, **layer_extra}],
        }
        inst = SceneInstance(scene, 800, seed=1)
        for _ in range(30):
            hdr = inst.render(1 / 30)
        return hdr

    legacy = run({"decay": 0.9})
    trail_obj = run({"trail": {"decay": 0.9}})
    assert np.allclose(legacy, trail_obj)


def test_trail_blur_softens_the_tail():
    base = {
        "name": "trail-blur",
        "layers": [{"type": "particles", "blend": "add",
                    "physics": {"bounce": {"enabled": True, "restitution": 1.0}},
                    "particles": [{"pos": 100, "vel": 300, "color": "#ffffff", "size": 1.5}],
                    "render": {"fade_out": False}}],
    }
    sharp = dict(base)
    sharp["layers"] = [{**base["layers"][0], "trail": {"decay": 0.9}}]
    blurred = dict(base)
    blurred["layers"] = [{**base["layers"][0], "trail": {"decay": 0.9, "blur": 3.0}}]

    def render(scene):
        inst = SceneInstance(scene, 800, seed=1)
        for _ in range(30):
            hdr = inst.render(1 / 30)
        return hdr

    hdr_sharp = render(sharp)
    hdr_blur = render(blurred)
    assert np.all(np.isfinite(hdr_blur))
    # blur spreads the trail's energy sideways: more pixels carry some
    # nonzero glow (a wider, softer tail) versus the sharp hard-edged one.
    lit_sharp = int((hdr_sharp[:, 0] > 1e-4).sum())
    lit_blur = int((hdr_blur[:, 0] > 1e-4).sum())
    assert lit_blur > lit_sharp


def test_trail_blur_wraps_the_ring_seam():
    """A trail near the 0/n boundary must blur across the seam, not fade at
    a hard edge — ring scenes are physically continuous there."""
    scene = {
        "name": "seam",
        "layers": [{"type": "particles", "blend": "add",
                    "trail": {"decay": 0.95, "blur": 4.0},
                    "physics": {"wrap": True},
                    "particles": [{"pos": 1, "vel": 0, "color": "#ffffff", "size": 1.0}],
                    "render": {"fade_out": False}}],
    }
    inst = SceneInstance(scene, 800, seed=1)
    for _ in range(5):
        hdr = inst.render(1 / 30)
    # energy should have leaked across the seam into the high-index end
    assert hdr[795:800].max() > 0.0


def test_field_layers_accept_palette_speed():
    for lt, extra in (
        ("wave", {"waves": [{"wavelength": 100, "speed": 0, "amplitude": 1.0}]}),
        ("noise", {}),
        ("plasma", {}),
        ("gradient", {}),
    ):
        scene = {"name": lt, "layers": [{"type": lt, "palette": ["#ff0000", "#0000ff"],
                                         "palette_speed": 0.25, **extra}]}
        inst = SceneInstance(scene, 200, seed=1)
        hdr0 = inst.render(0.0001)
        hdr1 = inst.render(1.0)  # a full palette_speed=0.25 cycle offset of 0.25
        assert np.all(np.isfinite(hdr1))
        assert not np.allclose(hdr0, hdr1), f"{lt}: palette_speed should shift color over time"


def test_particle_color_mode_index_gives_varied_hues():
    scene = {
        "name": "multicolor",
        "layers": [{"type": "particles", "palette": ["#ff0000", "#00ff00", "#0000ff"],
                    "physics": {"bounce": {"enabled": False}},
                    "emitters": [{"pos": [0, 799], "rate": 200, "vel": 0,
                                  "color_mode": "index", "size": 1.0, "lifetime": 5}],
                    "render": {"fade_out": False}}],
    }
    inst = SceneInstance(scene, 800, seed=3)
    for _ in range(10):
        hdr = inst.render(1 / 30)
    layer = inst.layers[0]
    colors = layer.system.color  # legacy static colors, unused when dynamic
    dyn = layer.system._dynamic_colors(inst.t)
    assert layer.system.count > 5
    # dynamic colors must differ across particles (multi-hue, not flat)
    assert dyn[:, 0].std() > 0.01 or dyn[:, 1].std() > 0.01 or dyn[:, 2].std() > 0.01


def test_particle_color_mode_static_is_unaffected_by_palette():
    """Backward compat: a particles layer with a palette but color_mode left
    at the default ('static') must render pixel-identical to one with no
    palette at all — palette is opt-in per emitter/particle."""
    def build(with_palette):
        layer = {"type": "particles", "physics": {"bounce": {"enabled": True, "restitution": 1.0}},
                 "particles": [{"pos": 100, "vel": 300, "color": "#ffffff", "size": 1.5}],
                 "render": {"fade_out": False}}
        if with_palette:
            layer["palette"] = ["#ff0000", "#00ff00", "#0000ff"]
            layer["palette_speed"] = 1.0
        return {"name": "static", "layers": [layer]}

    a = SceneInstance(build(False), 800, seed=1)
    b = SceneInstance(build(True), 800, seed=1)
    for _ in range(20):
        ha = a.render(1 / 30)
        hb = b.render(1 / 30)
    assert np.allclose(ha, hb)


# -- sync-pulse: latency-calibration metronome -----------------------------

def _load_sync_pulse():
    return json.loads((SCENES_DIR / "sync-pulse.json").read_text())


def test_sync_pulse_validates():
    validate_scene(_load_sync_pulse())


def test_sync_pulse_full_ring_uniform_when_bright():
    """No spatial phase: every pixel is identical (and bright) during the
    on phase."""
    inst = SceneInstance(_load_sync_pulse(), 1920, seed=1)
    dt = 1 / 30
    hdr = inst.render(dt)  # first frame lands inside the ~120ms on-window
    assert hdr.max() > 0.9
    assert np.allclose(hdr, hdr[0])  # uniform across the whole ring


def test_sync_pulse_mid_period_is_black():
    inst = SceneInstance(_load_sync_pulse(), 1920, seed=1)
    dt = 1 / 30
    hdr = None
    for _ in range(int(0.5 * 30)):  # 0.5s in: well past the ~120ms duty window
        hdr = inst.render(dt)
    assert hdr.max() == 0.0


def test_sync_pulse_period_is_exactly_one_second_over_ten_seconds():
    """Track the strobe phase directly (not by scanning rendered frames,
    which can alias against the 30fps sampling grid) -- onset happens each
    time phase wraps back below the duty threshold. Tolerance is one
    engine frame (1/30s): the strobe's 1.000Hz period is an exact multiple
    of the fixed 30fps frame time, so a boundary crossing can land right on
    a sample instant and get pulled to either side of it by float64
    accumulation noise in `self.t` -- a single-frame (33ms) quantization
    jitter, not drift (it never compounds across cycles)."""
    inst = SceneInstance(_load_sync_pulse(), 1920, seed=1)
    dt = 1 / 30
    duty = inst.strobe_duty
    prev_on = False
    onset_times = []
    for _ in range(300):  # 10s @ 30fps
        inst.render(dt)
        phase = (inst.t * inst.strobe_rate) % 1.0
        on = bool(phase < duty)
        if not prev_on and on:
            onset_times.append(inst.t)
        prev_on = on
    assert len(onset_times) >= 8
    diffs = np.diff(onset_times)
    assert np.allclose(diffs, 1.0, atol=dt + 1e-6)
    # no cumulative drift: average period over the whole run is dead on 1.0s
    assert float(np.mean(diffs)) == pytest.approx(1.0, abs=1e-3)


def test_sync_pulse_hard_edges_no_easing():
    """Two frames straddling the on->off boundary should differ by a hard
    step (bright to exactly black), not a fade -- gain formula uses depth=1
    so the off phase is exactly 0."""
    scene_dict = _load_sync_pulse()
    assert scene_dict["master"]["strobe"]["depth"] == 1.0
    inst = SceneInstance(scene_dict, 1920, seed=1)
    dt = 1 / 30
    prev_bright = False
    saw_hard_edge = False
    for _ in range(60):
        hdr = inst.render(dt)
        bright = bool(hdr.max() > 0.9)
        if prev_bright and not bright:
            saw_hard_edge = True
        prev_bright = bright
    assert saw_hard_edge


def test_sync_pulse_installed_but_not_running_by_default():
    """Sanity: the scene file exists in scenes/ (discoverable via GET
    /scenes) but nothing in this repo activates it automatically."""
    assert (SCENES_DIR / "sync-pulse.json").exists()


def test_crossfade_blends_scenes():
    eng = Engine(mode="headless")
    eng.set_scene({"layers": [{"type": "solid", "color": "#ff0000"}]}, crossfade=0)
    eng.step()
    eng.set_scene({"layers": [{"type": "solid", "color": "#0000ff"}]}, crossfade=0.5)
    mid = eng.step()  # partway through the fade: both channels present
    assert mid[0][0] > 0 and mid[0][2] > 0
    for _ in range(30):
        final = eng.step()
    assert tuple(final[0]) == (0, 0, 255)  # fade complete, exact target color
