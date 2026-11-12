"""Fixed fixtures for the golden OSC vectors in ``septagraph/golden/*.osc``.

Shared by ``test_codec.py`` (asserts byte equality) and ``conftest.py``
(``--write-golden`` regenerates the files). The JS codec test page
re-encodes these same values and compares against the same bytes, so
CHANGING ANY VALUE HERE IS A WIRE-FORMAT CHANGE.
"""
from septagraph import codec
from septagraph.axes import K
from septagraph.types import (
    HALF, ZERO, CatalogPoint, Hit, Knot, Manifest, Move, Septagraph, vec,
)

HIT_BASIC = Hit("grid:7", 0, True, 0.8, vec(0.42, 0.7, 0.9, 0.02, 0.35, 0.5, 0.1, 0.2, 0.5), ZERO)

MOVE_GLOBAL = Move("", HALF)

SHAPE_TWO_KNOTS = Septagraph(
    "bow", 0, 2.5,
    (Knot(0.0, HALF, ZERO), Knot(1.0, vec(*[0.6] * K), vec(*[0.1] * K))),
)

MANIFEST_CONTINUOUS = Manifest(
    "chuck", "continuous", "sept-render-chuck", ZERO, tuple([1.0] * K), (),
)

MANIFEST_CATALOG_2PTS = Manifest(
    "lab", "catalog", "Machine Lab", None, None,
    (
        CatalogPoint(0, HALF, ZERO, "breakBot", 12.0, 0.1, 1.0),
        CatalogPoint(1, vec(*[0.3] * K), ZERO, "tammy", 8.0, 0.2, 1.0),
    ),
)

# name -> zero-arg callable producing the packet bytes
GOLDEN_FIXTURES = {
    "hit_basic": lambda: codec.encode_hit(HIT_BASIC),
    "move_global": lambda: codec.encode_move(MOVE_GLOBAL),
    "shape_two_knots": lambda: codec.encode_shape(SHAPE_TWO_KNOTS),
    "manifest_continuous": lambda: codec.encode_manifest(MANIFEST_CONTINUOUS),
    "manifest_catalog_2pts": lambda: codec.encode_manifest(MANIFEST_CATALOG_2PTS),
}
