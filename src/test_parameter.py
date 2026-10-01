from core.effect import (
    EffectBlock,
)

from core.parameter import (
    ParameterSpec,
)

from effects.delay import (
    apply_delay,
)


delay = EffectBlock(
    name="Delay",
    processor=apply_delay,

    parameters={
        "delay_ms": 400.0,
        "feedback": 0.3,
        "mix": 0.2,
    },

    parameter_specs={
        "delay_ms": ParameterSpec(
            name="Delay Time",
            default=400.0,
            minimum=1.0,
            maximum=2000.0,
            unit="ms",
            step=1.0,
        ),

        "feedback": ParameterSpec(
            name="Feedback",
            default=0.3,
            minimum=0.0,
            maximum=0.95,
            unit="",
            step=0.01,
        ),

        "mix": ParameterSpec(
            name="Mix",
            default=0.2,
            minimum=0.0,
            maximum=1.0,
            unit="",
            step=0.01,
        ),
    },
)


print(
    "=== ORIGINAL ==="
)

print(
    "Delay:",
    delay.get_parameter(
        "delay_ms"
    ),
)

print(
    "Feedback:",
    delay.get_parameter(
        "feedback"
    ),
)

print(
    "Mix:",
    delay.get_parameter(
        "mix"
    ),
)


# =========================================================
# 정상값
# =========================================================

delay.set_parameter(
    "delay_ms",
    750.0,
)

print()
print(
    "=== NORMAL VALUE ==="
)

print(
    "Delay:",
    delay.get_parameter(
        "delay_ms"
    ),
)


# =========================================================
# Maximum 초과
# =========================================================

delay.set_parameter(
    "delay_ms",
    5000.0,
)

print()
print(
    "=== OVER MAXIMUM ==="
)

print(
    "Requested: 5000 ms"
)

print(
    "Actual:",
    delay.get_parameter(
        "delay_ms"
    ),
)


# =========================================================
# Minimum 미만
# =========================================================

delay.set_parameter(
    "feedback",
    -5.0,
)

print()
print(
    "=== UNDER MINIMUM ==="
)

print(
    "Requested Feedback: -5"
)

print(
    "Actual Feedback:",
    delay.get_parameter(
        "feedback"
    ),
)


# =========================================================
# Parameter 정보
# =========================================================

spec = delay.get_parameter_spec(
    "delay_ms"
)

print()
print(
    "=== PARAMETER SPEC ==="
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
    "Minimum:",
    spec.minimum,
)

print(
    "Maximum:",
    spec.maximum,
)

print(
    "Unit:",
    spec.unit,
)

print(
    "Step:",
    spec.step,
)


# =========================================================
# Default 복원
# =========================================================

delay.reset_parameter(
    "delay_ms"
)

print()
print(
    "=== RESET ==="
)

print(
    "Delay:",
    delay.get_parameter(
        "delay_ms"
    ),
)