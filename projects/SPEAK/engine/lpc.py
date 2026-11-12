import numpy as np

# TMS5100 lookup tables — sourced directly from ti_lpc_src/cmd_line_vers/ti_lpc_cmd.cpp
ENERGY = [0, 0, 1, 1, 2, 3, 5, 7, 10, 15, 21, 30, 43, 61, 86, 0]
PERIOD = [0, 41, 43, 45, 47, 49, 51, 53, 55, 58, 60, 63, 66, 70, 73, 76,
          79, 83, 87, 90, 94, 99, 103, 107, 112, 118, 123, 129, 134, 140, 147, 153]
CHIRP  = [0, 42, 212, 50, 178, 18, 37, 20, 2, 225, 197, 2, 95, 90, 5, 15,
          38, 252, 165, 165, 214, 221, 220, 252, 37, 43, 34, 33, 15, 255,
          248, 238, 237, 239, 247, 246, 250, 0, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0]

K = [
    [-501,-497,-493,-488,-480,-471,-460,-446,-427,-405,-378,-344,-305,-259,
     -206,-148,-86,-21,45,110,171,227,277,320,357,388,413,434,451,464,474,498],
    [-349,-328,-305,-280,-252,-223,-192,-158,-124,-88,-51,-14,23,60,97,133,
     167,199,230,259,286,310,333,354,372,389,404,417,429,439,449,506],
    [-397,-365,-327,-282,-229,-170,-104,-36,35,104,169,228,281,326,364,396],
    [-369,-334,-293,-245,-191,-131,-67,-1,64,128,188,243,291,332,367,397],
    [-319,-286,-250,-211,-168,-122,-74,-25,24,73,121,167,210,249,285,318],
    [-290,-252,-209,-163,-114,-62,-9,44,97,147,194,238,278,313,344,371],
    [-291,-256,-216,-174,-128,-80,-31,19,69,117,163,206,246,283,316,345],
    [-218,-133,-38,59,152,235,305,361],
    [-226,-157,-82,-3,76,151,220,280],
    [-179,-122,-61,1,62,123,179,231],
]
K_BITS = [5, 5, 4, 4, 4, 4, 4, 3, 3, 3]

SAMPLES_PER_FRAME = 200   # 8000 Hz x 25 ms
MKFRACT = 512.0
GAIN = 1.5


class _BitStream:
    """Reads bits LSB-first from a byte sequence."""

    def __init__(self, data: bytes):
        self._data = data
        self._byte = 0
        self._bit = 0

    @property
    def exhausted(self) -> bool:
        return self._byte >= len(self._data)

    def read(self, n: int) -> int:
        val = 0
        for i in range(n):
            if self._byte >= len(self._data):
                return val
            val |= ((self._data[self._byte] >> self._bit) & 1) << i
            self._bit += 1
            if self._bit == 8:
                self._bit = 0
                self._byte += 1
        return val


def decode_frames(data: bytes) -> list[dict]:
    """Parse TMS5100 LPC bytes into a list of frame parameter dicts.

    Each dict has: energy (0-14), period (0-31), k (list of 10 K-table indices).
    Repeat frames are expanded — k is copied from the previous non-repeat frame.
    """
    bs = _BitStream(data)
    frames = []
    prev_k = [0] * 10
    while True:
        energy_idx = bs.read(4)
        if bs.exhausted and energy_idx == 0:
            break
        if energy_idx == 0xF:
            break
        if energy_idx == 0:
            frames.append({'energy': 0, 'period': 0, 'k': list(prev_k)})
            if bs.exhausted:
                break
            continue
        repeat = bs.read(1)
        period_idx = bs.read(5)
        if not repeat:
            k = [0] * 10
            for i in range(4):
                k[i] = bs.read(K_BITS[i])
            if period_idx:
                for i in range(4, 10):
                    k[i] = bs.read(K_BITS[i])
            prev_k = list(k)
        else:
            k = list(prev_k)
        frames.append({'energy': energy_idx, 'period': period_idx, 'k': k})
        if bs.exhausted:
            break
    return frames


def encode_frames(frames: list[dict]) -> bytes:
    """Encode a list of frame dicts back to TMS5100 LPC bytes (with stop frame)."""
    bits: list[int] = []

    def push(val: int, n: int) -> None:
        for i in range(n):
            bits.append((val >> i) & 1)

    for f in frames:
        e = f['energy']
        push(e, 4)
        if e == 0:
            continue
        push(0, 1)  # repeat=False (always explicit after expand)
        p = f['period']
        push(p, 5)
        k = f['k']
        for i in range(4):
            push(k[i], K_BITS[i])
        if p:
            for i in range(4, 10):
                push(k[i], K_BITS[i])

    push(0xF, 4)  # stop frame

    result = []
    for i in range(0, len(bits), 8):
        byte = 0
        for j, b in enumerate(bits[i:i + 8]):
            byte |= b << j
        result.append(byte)
    return bytes(result)


def concat_lpc(*clips: bytes) -> bytes:
    """Concatenate multiple LPC byte clips into one properly-terminated stream."""
    all_frames: list[dict] = []
    for clip in clips:
        if clip:
            all_frames.extend(decode_frames(clip))
    return encode_frames(all_frames) if all_frames else b''


def decode(data: bytes) -> np.ndarray:
    """Decode TMS5100 LPC byte data to float32 audio samples at 8000 Hz."""
    bs = _BitStream(data)
    samples = []

    k = [0.0] * 10
    x = [0.0] * 10
    period_cnt = 0
    synth_rand = 1

    while True:
        energy_idx = bs.read(4)

        if energy_idx == 0:
            samples.extend([0.0] * SAMPLES_PER_FRAME)
            if bs.exhausted:
                break
            continue

        if energy_idx == 0xF:
            break

        if bs.exhausted:
            break

        energy = ENERGY[energy_idx]
        repeat = bs.read(1)
        period_idx = bs.read(5)
        period = PERIOD[period_idx]

        if not repeat:
            new_k = [0.0] * 10
            for i in range(4):
                idx = bs.read(K_BITS[i])
                new_k[i] = float(K[i][idx])
            if period:
                for i in range(4, 10):
                    idx = bs.read(K_BITS[i])
                    new_k[i] = float(K[i][idx])
            k = new_k

        for _ in range(SAMPLES_PER_FRAME):
            if period:
                chirp_val = CHIRP[period_cnt] if period_cnt < 41 else 0
                chirp_signed = chirp_val if chirp_val < 128 else chirp_val - 256
                u10 = (chirp_signed / 256.0) * (energy / 256.0)
                period_cnt = 0 if period_cnt >= period - 1 else period_cnt + 1
            else:
                synth_rand = (synth_rand >> 1) ^ (0xB800 if (synth_rand & 1) else 0)
                wnoise = energy if (synth_rand & 1) else -energy
                u10 = wnoise / 2048.0

            # Lattice filter — forward pass
            u10_ = u10
            u9  = u10_ - (k[9] / MKFRACT) * x[9]
            u8  = u9   - (k[8] / MKFRACT) * x[8]
            u7  = u8   - (k[7] / MKFRACT) * x[7]
            u6  = u7   - (k[6] / MKFRACT) * x[6]
            u5  = u6   - (k[5] / MKFRACT) * x[5]
            u4  = u5   - (k[4] / MKFRACT) * x[4]
            u3  = u4   - (k[3] / MKFRACT) * x[3]
            u2  = u3   - (k[2] / MKFRACT) * x[2]
            u1  = u2   - (k[1] / MKFRACT) * x[1]
            u0  = u1   - (k[0] / MKFRACT) * x[0]
            u0  = max(-1.0, min(1.0, u0))

            # Lattice filter — reverse pass (update state)
            x[9] = x[8] + (k[8] / MKFRACT) * u8
            x[8] = x[7] + (k[7] / MKFRACT) * u7
            x[7] = x[6] + (k[6] / MKFRACT) * u6
            x[6] = x[5] + (k[5] / MKFRACT) * u5
            x[5] = x[4] + (k[4] / MKFRACT) * u4
            x[4] = x[3] + (k[3] / MKFRACT) * u3
            x[3] = x[2] + (k[2] / MKFRACT) * u2
            x[2] = x[1] + (k[1] / MKFRACT) * u1
            x[1] = x[0] + (k[0] / MKFRACT) * u0
            x[0] = u0

            samples.append(u0 * GAIN)

    return np.array(samples, dtype=np.float32)
