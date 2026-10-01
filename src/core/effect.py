import re


def normalize_effect_type(
    value,
):
    """
    Effect Type을 내부 식별용 문자열로 변환

    "Amp Sim" -> "amp_sim"
    "Noise Gate" -> "noise_gate"
    """

    value = value.strip().lower()

    value = re.sub(
        r"[^a-z0-9]+",
        "_",
        value,
    )

    value = value.strip("_")

    return value or "effect"


class EffectBlock:
    """
    멀티이펙터의 공통 Effect Block

    effect_id:
        각 Effect 인스턴스의 고유 ID
        ex) delay_1, delay_2

    effect_type:
        DSP 종류
        ex) delay, reverb, amp_sim

    name:
        사용자에게 보여줄 이름
        ex) Ambient Delay
    """

    def __init__(
        self,
        name,
        processor,
        parameters=None,
        parameter_specs=None,
        bypass=False,
        use_sample_rate=True,
        effect_type=None,
        effect_id=None,
    ):
        self.name = name

        self.effect_type = (
            normalize_effect_type(
                effect_type
            )
            if effect_type
            else normalize_effect_type(
                name
            )
        )

        # EffectChain에서 자동 생성 가능
        self.effect_id = effect_id

        self.processor = processor

        self.parameters = (
            parameters.copy()
            if parameters
            else {}
        )

        self.parameter_specs = (
            parameter_specs.copy()
            if parameter_specs
            else {}
        )

        self.bypass = bypass

        self.use_sample_rate = (
            use_sample_rate
        )

        # 초기값도 범위 안으로 제한
        for parameter_name in (
            self.parameters
        ):
            self.parameters[
                parameter_name
            ] = self._validate_parameter(
                parameter_name,
                self.parameters[
                    parameter_name
                ],
            )

    def _validate_parameter(
        self,
        name,
        value,
    ):
        spec = self.parameter_specs.get(
            name
        )

        if spec is None:
            return value

        return spec.clamp(
            value
        )

    def process(
        self,
        audio_data,
        sample_rate,
    ):
        if self.bypass:
            return audio_data

        if self.use_sample_rate:

            return self.processor(
                audio_data,
                sample_rate,
                **self.parameters,
            )

        return self.processor(
            audio_data,
            **self.parameters,
        )

    def set_parameter(
        self,
        name,
        value,
    ):
        value = self._validate_parameter(
            name,
            value,
        )

        self.parameters[
            name
        ] = value

    def get_parameter(
        self,
        name,
    ):
        return self.parameters.get(
            name
        )

    def get_parameter_spec(
        self,
        name,
    ):
        return self.parameter_specs.get(
            name
        )

    def reset_parameter(
        self,
        name,
    ):
        spec = self.parameter_specs.get(
            name
        )

        if spec is None:
            return

        self.parameters[
            name
        ] = spec.default

    def reset_all_parameters(
        self,
    ):
        for name, spec in (
            self.parameter_specs.items()
        ):
            self.parameters[
                name
            ] = spec.default

    def set_bypass(
        self,
        bypass,
    ):
        self.bypass = bool(
            bypass
        )

    def toggle_bypass(
        self,
    ):
        self.bypass = (
            not self.bypass
        )

    def __repr__(
        self,
    ):
        state = (
            "BYPASS"
            if self.bypass
            else "ON"
        )

        return (
            "EffectBlock("
            f"id='{self.effect_id}', "
            f"type='{self.effect_type}', "
            f"name='{self.name}', "
            f"state={state}, "
            f"parameters={self.parameters}"
            ")"
        )