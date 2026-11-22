import math


def nearest_pow2_ratio(x, min_exp=-2, max_exp=3):
    """Snap a positive ratio to the nearest 2**n with min_exp <= n <= max_exp.
    Uses round-half-up in log2 space so 1.5 -> 2.0 (not banker's rounding)."""
    if x <= 0:
        raise ValueError("ratio must be positive")
    n = math.floor(math.log2(x) + 0.5)
    n = max(min_exp, min(max_exp, n))
    return 2.0 ** n


def quantize_length(recorded_len, master_len, min_exp=-2, max_exp=3):
    """Channel loop length snaps to master_len * nearest power-of-two ratio."""
    if master_len <= 0:
        raise ValueError("master_len must be positive")
    ratio = nearest_pow2_ratio(recorded_len / master_len, min_exp, max_exp)
    return master_len * ratio
