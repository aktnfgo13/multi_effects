from core.effect import (
    EffectBlock,
)

from core.parameter import (
    ParameterSpec,
)

from effects.amp_sim import (
    apply_amp_sim,
)

from effects.chorus import apply_chorus
from effects.flanger import apply_flanger
from effects.phaser import apply_phaser
from effects.tremolo import apply_tremolo
from effects.vibrato import apply_vibrato

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

def create_chorus_block(
    name="Chorus",
    effect_id=None,
):
    return EffectBlock(
        name=name,
        effect_type="chorus",
        effect_id=effect_id,
        processor=apply_chorus,

        parameters={
            "rate_hz": 0.8,
            "depth_ms": 5.0,
            "base_delay_ms": 15.0,
            "mix": 0.35,
            "stereo_phase_deg": 90.0,
        },

        parameter_specs={
            "rate_hz": ParameterSpec(
                name="Rate",
                default=0.8,
                minimum=0.05,
                maximum=5.0,
                unit="Hz",
                step=0.05,
            ),

            "depth_ms": ParameterSpec(
                name="Depth",
                default=5.0,
                minimum=0.1,
                maximum=10.0,
                unit="ms",
                step=0.1,
            ),

            "base_delay_ms": ParameterSpec(
                name="Delay",
                default=15.0,
                minimum=5.0,
                maximum=30.0,
                unit="ms",
                step=0.5,
            ),

            "mix": ParameterSpec(
                name="Mix",
                default=0.35,
                minimum=0.0,
                maximum=1.0,
                unit="",
                step=0.01,
            ),

            "stereo_phase_deg": ParameterSpec(
                name="Stereo Phase",
                default=90.0,
                minimum=0.0,
                maximum=180.0,
                unit="deg",
                step=1.0,
            ),
        },
    )


def create_flanger_block(
    name="Flanger",
    effect_id=None,
):
    return EffectBlock(
        name=name,
        effect_type="flanger",
        effect_id=effect_id,
        processor=apply_flanger,

        parameters={
            "rate_hz": 0.4,
            "depth_ms": 1.5,
            "base_delay_ms": 2.0,
            "mix": 0.5,
            "stereo_phase_deg": 90.0,
        },

        parameter_specs={
            "rate_hz": ParameterSpec(
                name="Rate",
                default=0.4,
                minimum=0.05,
                maximum=5.0,
                unit="Hz",
                step=0.05,
            ),

            "depth_ms": ParameterSpec(
                name="Depth",
                default=1.5,
                minimum=0.1,
                maximum=4.0,
                unit="ms",
                step=0.1,
            ),

            "base_delay_ms": ParameterSpec(
                name="Delay",
                default=2.0,
                minimum=0.2,
                maximum=6.0,
                unit="ms",
                step=0.1,
            ),

            "mix": ParameterSpec(
                name="Mix",
                default=0.5,
                minimum=0.0,
                maximum=1.0,
                unit="",
                step=0.01,
            ),

            "stereo_phase_deg": ParameterSpec(
                name="Stereo Phase",
                default=90.0,
                minimum=0.0,
                maximum=180.0,
                unit="deg",
                step=1.0,
            ),
        },
    )


def create_phaser_block(
    name="Phaser",
    effect_id=None,
):
    return EffectBlock(
        name=name,
        effect_type="phaser",
        effect_id=effect_id,
        processor=apply_phaser,

        parameters={
            "rate_hz": 0.5,
            "min_frequency_hz": 300.0,
            "max_frequency_hz": 1600.0,
            "stages": 4,
            "mix": 0.5,
            "stereo_phase_deg": 90.0,
            "block_size": 256,
        },

        parameter_specs={
            "rate_hz": ParameterSpec(
                name="Rate",
                default=0.5,
                minimum=0.05,
                maximum=5.0,
                unit="Hz",
                step=0.05,
            ),

            "min_frequency_hz": ParameterSpec(
                name="Sweep Low",
                default=300.0,
                minimum=50.0,
                maximum=1500.0,
                unit="Hz",
                step=10.0,
            ),

            "max_frequency_hz": ParameterSpec(
                name="Sweep High",
                default=1600.0,
                minimum=500.0,
                maximum=5000.0,
                unit="Hz",
                step=10.0,
            ),

            "stages": ParameterSpec(
                name="Stages",
                default=4,
                minimum=1,
                maximum=12,
                unit="",
                step=1,
            ),

            "mix": ParameterSpec(
                name="Mix",
                default=0.5,
                minimum=0.0,
                maximum=1.0,
                unit="",
                step=0.01,
            ),

            "stereo_phase_deg": ParameterSpec(
                name="Stereo Phase",
                default=90.0,
                minimum=0.0,
                maximum=180.0,
                unit="deg",
                step=1.0,
            ),

            "block_size": ParameterSpec(
                name="Block Size",
                default=256,
                minimum=32,
                maximum=2048,
                unit="samples",
                step=32,
            ),
        },
    )


def create_tremolo_block(
    name="Tremolo",
    effect_id=None,
):
    return EffectBlock(
        name=name,
        effect_type="tremolo",
        effect_id=effect_id,
        processor=apply_tremolo,

        parameters={
            "rate_hz": 4.0,
            "depth": 0.5,
            "stereo_phase_deg": 0.0,
        },

        parameter_specs={
            "rate_hz": ParameterSpec(
                name="Rate",
                default=4.0,
                minimum=0.1,
                maximum=20.0,
                unit="Hz",
                step=0.1,
            ),

            "depth": ParameterSpec(
                name="Depth",
                default=0.5,
                minimum=0.0,
                maximum=1.0,
                unit="",
                step=0.01,
            ),

            "stereo_phase_deg": ParameterSpec(
                name="Stereo Phase",
                default=0.0,
                minimum=0.0,
                maximum=180.0,
                unit="deg",
                step=1.0,
            ),
        },
    )


def create_vibrato_block(
    name="Vibrato",
    effect_id=None,
):
    return EffectBlock(
        name=name,
        effect_type="vibrato",
        effect_id=effect_id,
        processor=apply_vibrato,

        parameters={
            "rate_hz": 5.0,
            "depth_ms": 2.0,
            "base_delay_ms": 5.0,
            "stereo_phase_deg": 0.0,
        },

        parameter_specs={
            "rate_hz": ParameterSpec(
                name="Rate",
                default=5.0,
                minimum=0.1,
                maximum=12.0,
                unit="Hz",
                step=0.1,
            ),

            "depth_ms": ParameterSpec(
                name="Depth",
                default=2.0,
                minimum=0.1,
                maximum=5.0,
                unit="ms",
                step=0.1,
            ),

            "base_delay_ms": ParameterSpec(
                name="Delay",
                default=5.0,
                minimum=1.0,
                maximum=10.0,
                unit="ms",
                step=0.1,
            ),

            "stereo_phase_deg": ParameterSpec(
                name="Stereo Phase",
                default=0.0,
                minimum=0.0,
                maximum=180.0,
                unit="deg",
                step=1.0,
            ),
        },
    )