from engine import stt
from engine.voice import synthesize
from engine.audio import play

print("warming up whisper...")
stt._get_model()
print("ready — speak after the beep\n")

samples, sr = synthesize("ready", mode="mix", ti_amount=0.85)
play(samples, sr)

while True:
    print("listening (VAD)...")
    text = stt.listen_and_transcribe()
    print(f"  → {repr(text)}\n")
    if text:
        samples, sr = synthesize(text, mode="mix", ti_amount=0.85)
        play(samples, sr)
