from engine.voice import synthesize
from engine.audio import play
samples, sr = synthesize("yo what is up", mode="mix", ti_amount=0.85)
play(samples, sr)
