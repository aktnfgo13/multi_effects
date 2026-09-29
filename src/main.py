from pathlib import Path
import soundfile as sf


# 프로젝트 루트 경로
PROJECT_ROOT = Path(__file__).resolve().parent.parent

INPUT_PATH = PROJECT_ROOT / "audio" / "input" / "test.wav"
OUTPUT_PATH = PROJECT_ROOT / "audio" / "output" / "test_copy.wav"


audio_data, sample_rate = sf.read(INPUT_PATH)

print("=== Audio Information ===")
print(f"Sample Rate : {sample_rate} Hz")

if audio_data.ndim == 1:
    channels = 1
else:
    channels = audio_data.shape[1]

print(f"Channels    : {channels}")

duration = len(audio_data) / sample_rate

print(f"Duration    : {duration:.2f} sec")
print(f"Samples     : {len(audio_data)}")
print(f"Data Shape  : {audio_data.shape}")

sf.write(OUTPUT_PATH, audio_data, sample_rate)

print()
print(f"Saved: {OUTPUT_PATH}")