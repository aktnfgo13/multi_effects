class EffectChain:
    """
    여러 EffectBlock을 순서대로 처리하는
    Multi Effects Chain
    """

    def __init__(
        self,
        effects=None,
    ):
        self.effects = (
            list(effects)
            if effects
            else []
        )

    def add_effect(
        self,
        effect,
    ):
        """
        Chain 마지막에 Effect 추가
        """

        self.effects.append(
            effect
        )

    def insert_effect(
        self,
        index,
        effect,
    ):
        """
        원하는 위치에 Effect 삽입
        """

        self.effects.insert(
            index,
            effect,
        )

    def remove_effect(
        self,
        index,
    ):
        """
        Effect 삭제
        """

        return self.effects.pop(
            index
        )

    def get_effect(
        self,
        name,
    ):
        """
        이름으로 Effect 찾기
        """

        for effect in self.effects:

            if effect.name == name:
                return effect

        return None

    def move_effect(
        self,
        old_index,
        new_index,
    ):
        """
        Effect 순서 변경
        """

        effect = self.effects.pop(
            old_index
        )

        self.effects.insert(
            new_index,
            effect,
        )

    def process(
        self,
        audio_data,
        sample_rate,
    ):
        """
        Chain 순서대로 모든 DSP 실행
        """

        output = audio_data

        for effect in self.effects:

            print(
                f"Processing: "
                f"{effect.name}"
                f"{' [BYPASS]' if effect.bypass else ''}"
            )

            output = effect.process(
                output,
                sample_rate,
            )

        return output

    def print_chain(
        self,
    ):
        """
        현재 Chain 상태 출력
        """

        print(
            "=== EFFECT CHAIN ==="
        )

        for index, effect in enumerate(
            self.effects
        ):

            state = (
                "BYPASS"
                if effect.bypass
                else "ON"
            )

            print(
                f"{index + 1}. "
                f"{effect.name} "
                f"[{state}]"
            )