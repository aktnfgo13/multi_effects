class EffectChain:
    """
    여러 EffectBlock을 순서대로 처리하는
    Multi Effects Chain
    """

    def __init__(
        self,
        effects=None,
    ):
        self.effects = []

        # Effect Type별 ID 번호 관리
        self._type_counters = {}

        if effects:

            for effect in effects:
                self.add_effect(
                    effect
                )

    def _generate_effect_id(
        self,
        effect_type,
    ):
        """
        effect_type을 기준으로
        고유 ID 생성

        delay -> delay_1
        delay -> delay_2
        """

        current = (
            self._type_counters.get(
                effect_type,
                0,
            )
        )

        existing_ids = {
            effect.effect_id
            for effect in self.effects
            if effect.effect_id
        }

        while True:

            current += 1

            candidate = (
                f"{effect_type}_{current}"
            )

            if candidate not in existing_ids:
                break

        self._type_counters[
            effect_type
        ] = current

        return candidate

    def _prepare_effect(
        self,
        effect,
    ):
        """
        Effect ID 생성 및 중복 검사
        """

        if effect.effect_id is None:

            effect.effect_id = (
                self._generate_effect_id(
                    effect.effect_type
                )
            )

        else:

            for current in self.effects:

                if (
                    current.effect_id
                    == effect.effect_id
                ):
                    raise ValueError(
                        "Duplicate effect_id: "
                        f"{effect.effect_id}"
                    )

        return effect

    def add_effect(
        self,
        effect,
    ):
        effect = self._prepare_effect(
            effect
        )

        self.effects.append(
            effect
        )

    def insert_effect(
        self,
        index,
        effect,
    ):
        effect = self._prepare_effect(
            effect
        )

        self.effects.insert(
            index,
            effect,
        )

    def remove_effect(
        self,
        index,
    ):
        return self.effects.pop(
            index
        )

    def get_effect_by_id(
        self,
        effect_id,
    ):
        """
        고유 ID로 Effect 검색
        """

        for effect in self.effects:

            if (
                effect.effect_id
                == effect_id
            ):
                return effect

        return None

    def get_effect(
        self,
        name,
    ):
        """
        표시 이름으로 Effect 검색.

        동일한 이름이 여러 개라면
        첫 번째 Effect만 반환한다.

        내부 로직에서는
        get_effect_by_id() 사용 권장.
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
        output = audio_data

        for effect in self.effects:

            print(
                f"Processing: "
                f"{effect.name} "
                f"({effect.effect_id})"
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
                f"[{state}] "
                f"({effect.effect_id})"
            )