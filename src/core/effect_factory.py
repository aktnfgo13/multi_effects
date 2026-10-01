from core.effect import (
    EffectBlock,
)

from core.parameter import (
    ParameterSpec,
)

from effects.amp_sim import (
    apply_amp_sim,
)


def create_amp_sim_block(
    name="Amp Sim",
    effect_id=None,
):
    """
    Amp Sim EffectBlock 생성

    DSP 함수 + 기본값 + ParameterSpec을
    한 곳에서 관리한다.
    """

    return EffectBlock(
        name=name,

        effect_type="amp_sim",
        effect_id=effect_id,

        processor=apply_amp_sim,

        parameters={
            # Preamp
            "gain_db": 12.0,
            "drive": 1.5,
            "bias": 0.05,

            # Tone Stack
            "bass_db": 0.0,
            "mid_db": 0.0,
            "treble_db": 0.0,

            # Power Amp
            "master_db": -3.0,
            "power_drive": 1.2,
            "presence_db": 0.0,

            # Filtering
            "low_cut_hz": 70.0,
            "high_cut_hz": 9000.0,

            # Output
            "output_db": -6.0,
        },

        parameter_specs={
            # =================================================
            # Preamp
            # =================================================

            "gain_db": ParameterSpec(
                name="Gain",
                default=12.0,
                minimum=0.0,
                maximum=30.0,
                unit="dB",
                step=0.5,
            ),

            "drive": ParameterSpec(
                name="Preamp Drive",
                default=1.5,
                minimum=0.1,
                maximum=4.0,
                unit="",
                step=0.1,
            ),

            "bias": ParameterSpec(
                name="Bias",
                default=0.05,
                minimum=-0.2,
                maximum=0.2,
                unit="",
                step=0.01,
            ),

            # =================================================
            # Tone Stack
            # =================================================

            "bass_db": ParameterSpec(
                name="Bass",
                default=0.0,
                minimum=-12.0,
                maximum=12.0,
                unit="dB",
                step=0.5,
            ),

            "mid_db": ParameterSpec(
                name="Mid",
                default=0.0,
                minimum=-12.0,
                maximum=12.0,
                unit="dB",
                step=0.5,
            ),

            "treble_db": ParameterSpec(
                name="Treble",
                default=0.0,
                minimum=-12.0,
                maximum=12.0,
                unit="dB",
                step=0.5,
            ),

            # =================================================
            # Power Amp
            # =================================================

            "master_db": ParameterSpec(
                name="Master",
                default=-3.0,
                minimum=-24.0,
                maximum=6.0,
                unit="dB",
                step=0.5,
            ),

            "power_drive": ParameterSpec(
                name="Power Drive",
                default=1.2,
                minimum=0.5,
                maximum=3.0,
                unit="",
                step=0.1,
            ),

            "presence_db": ParameterSpec(
                name="Presence",
                default=0.0,
                minimum=-12.0,
                maximum=12.0,
                unit="dB",
                step=0.5,
            ),

            # =================================================
            # Filtering
            # =================================================

            "low_cut_hz": ParameterSpec(
                name="Low Cut",
                default=70.0,
                minimum=20.0,
                maximum=300.0,
                unit="Hz",
                step=1.0,
            ),

            "high_cut_hz": ParameterSpec(
                name="High Cut",
                default=9000.0,
                minimum=3000.0,
                maximum=18000.0,
                unit="Hz",
                step=100.0,
            ),

            # =================================================
            # Output
            # =================================================

            "output_db": ParameterSpec(
                name="Output",
                default=-6.0,
                minimum=-24.0,
                maximum=6.0,
                unit="dB",
                step=0.5,
            ),
        },

        bypass=False,

        use_sample_rate=True,
    )