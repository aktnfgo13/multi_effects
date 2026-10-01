from pathlib import Path

from core.effect_chain import (
    EffectChain,
)

from core.effect_factory import (
    create_amp_sim_block,
)

from core.preset import (
    save_preset,
    load_preset,
)


PROJECT_ROOT = (
    Path(__file__)
    .resolve()
    .parent
    .parent
)

PRESET_PATH = (
    PROJECT_ROOT
    / "presets"
    / "amp_test.json"
)


amp = create_amp_sim_block()

chain = EffectChain(
    [
        amp,
    ]
)


# =========================================================
# Original Setting
# =========================================================

amp.set_parameter(
    "gain_db",
    16.0,
)

amp.set_parameter(
    "bass_db",
    2.0,
)

amp.set_parameter(
    "mid_db",
    -2.0,
)

amp.set_parameter(
    "treble_db",
    3.0,
)

amp.set_parameter(
    "master_db",
    -4.0,
)

amp.set_parameter(
    "presence_db",
    2.0,
)


save_preset(
    chain,
    PRESET_PATH,
)


print(
    "=== SAVED ==="
)

print(
    "Gain:",
    amp.get_parameter(
        "gain_db"
    ),
)

print(
    "Bass:",
    amp.get_parameter(
        "bass_db"
    ),
)

print(
    "Presence:",
    amp.get_parameter(
        "presence_db"
    ),
)


# =========================================================
# 일부러 변경
# =========================================================

amp.set_parameter(
    "gain_db",
    2.0,
)

amp.set_parameter(
    "bass_db",
    -10.0,
)

amp.set_parameter(
    "presence_db",
    -10.0,
)


print()
print(
    "=== MODIFIED ==="
)

print(
    "Gain:",
    amp.get_parameter(
        "gain_db"
    ),
)

print(
    "Bass:",
    amp.get_parameter(
        "bass_db"
    ),
)

print(
    "Presence:",
    amp.get_parameter(
        "presence_db"
    ),
)


# =========================================================
# Load
# =========================================================

load_preset(
    chain,
    PRESET_PATH,
)


print()
print(
    "=== RESTORED ==="
)

print(
    "Gain:",
    amp.get_parameter(
        "gain_db"
    ),
)

print(
    "Bass:",
    amp.get_parameter(
        "bass_db"
    ),
)

print(
    "Presence:",
    amp.get_parameter(
        "presence_db"
    ),
)