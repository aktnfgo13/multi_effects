from core.effect_factory import (
    create_amp_sim_block,
)


amp = create_amp_sim_block()


print(
    "=== AMP BLOCK ==="
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
    "Master:",
    amp.get_parameter(
        "master_db"
    ),
)

print(
    "Presence:",
    amp.get_parameter(
        "presence_db"
    ),
)


# =========================================================
# Parameter 변경
# =========================================================

amp.set_parameter(
    "gain_db",
    18.0,
)

amp.set_parameter(
    "bass_db",
    3.0,
)

amp.set_parameter(
    "presence_db",
    2.5,
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
# Clamp Test
# =========================================================

amp.set_parameter(
    "gain_db",
    100.0,
)

print()
print(
    "=== CLAMP TEST ==="
)

print(
    "Requested Gain: 100 dB"
)

print(
    "Actual Gain:",
    amp.get_parameter(
        "gain_db"
    ),
)


# =========================================================
# Bypass
# =========================================================

amp.set_bypass(
    True
)

print()
print(
    "Amp Bypass:",
    amp.bypass,
)


amp.set_bypass(
    False
)


# =========================================================
# Parameter Spec
# =========================================================

spec = amp.get_parameter_spec(
    "gain_db"
)

print()
print(
    "=== GAIN SPEC ==="
)

print(
    "Name:",
    spec.name,
)

print(
    "Default:",
    spec.default,
)

print(
    "Min:",
    spec.minimum,
)

print(
    "Max:",
    spec.maximum,
)

print(
    "Unit:",
    spec.unit,
)