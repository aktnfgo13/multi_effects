class EffectBlock:
    """
    멀티이펙터의 공통 Effect Block
    """

    def __init__(
        self,
        name,
        processor,
        parameters=None,
        parameter_specs=None,
        bypass=False,
        use_sample_rate=True,
    ):
        self.name = name
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
        """
        ParameterSpec이 있으면
        min/max 범위를 적용
        """

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
        """
        Parameter 값을 변경한다.

        ParameterSpec이 존재하면
        min/max 범위를 자동 적용한다.
        """

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
        """
        Parameter의 범위/단위 정보 반환
        """

        return self.parameter_specs.get(
            name
        )

    def reset_parameter(
        self,
        name,
    ):
        """
        Parameter를 기본값으로 복원
        """

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
        """
        정의된 Parameter들을
        모두 기본값으로 복원
        """

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
            f"EffectBlock("
            f"name='{self.name}', "
            f"state={state}, "
            f"parameters="
            f"{self.parameters}"
            f")"
        )