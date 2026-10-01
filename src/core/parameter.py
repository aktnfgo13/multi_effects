class ParameterSpec:
    """
    Effect Parameter의 정의 정보

    name:
        parameter 이름

    default:
        기본값

    minimum:
        최소값

    maximum:
        최대값

    unit:
        표시 단위

    step:
        UI에서 한 단계씩 움직일 값
    """

    def __init__(
        self,
        name,
        default,
        minimum=None,
        maximum=None,
        unit="",
        step=None,
    ):
        self.name = name
        self.default = default
        self.minimum = minimum
        self.maximum = maximum
        self.unit = unit
        self.step = step

    def clamp(
        self,
        value,
    ):
        """
        허용 범위를 벗어난 값을 자동 제한
        """

        if (
            self.minimum is not None
            and value < self.minimum
        ):
            value = self.minimum

        if (
            self.maximum is not None
            and value > self.maximum
        ):
            value = self.maximum

        return value

    def __repr__(
        self,
    ):
        return (
            f"ParameterSpec("
            f"name='{self.name}', "
            f"default={self.default}, "
            f"min={self.minimum}, "
            f"max={self.maximum}, "
            f"unit='{self.unit}'"
            f")"
        )