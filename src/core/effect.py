class EffectBlock:
    """
    멀티이펙터의 공통 Effect Block

    name:
        이펙터 이름

    processor:
        실제 DSP 함수

    parameters:
        DSP 함수에 전달할 parameter

    bypass:
        True면 DSP를 통과하지 않음

    use_sample_rate:
        DSP 함수가 sample_rate를 사용하는지 여부
    """

    def __init__(
        self,
        name,
        processor,
        parameters=None,
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

        self.bypass = bypass

        self.use_sample_rate = (
            use_sample_rate
        )

    def process(
        self,
        audio_data,
        sample_rate,
    ):
        """
        Effect DSP 실행
        """

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
        Effect parameter 변경
        """

        self.parameters[name] = value

    def get_parameter(
        self,
        name,
    ):
        """
        Effect parameter 확인
        """

        return self.parameters.get(
            name
        )

    def set_bypass(
        self,
        bypass,
    ):
        """
        Bypass 설정
        """

        self.bypass = bool(
            bypass
        )

    def toggle_bypass(
        self,
    ):
        """
        Bypass ON/OFF 전환
        """

        self.bypass = (
            not self.bypass
        )

    def __repr__(self):

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