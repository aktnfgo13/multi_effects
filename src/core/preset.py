import json
from pathlib import Path

import numpy as np


def _make_json_safe(value):
    """
    JSON에 저장 가능한 값만 변환한다.

    지원:
    int / float / str / bool / None
    list / tuple
    dict

    NumPy scalar는 Python scalar로 변환한다.
    NumPy array는 현재 preset에서 제외한다.
    """

    if isinstance(
        value,
        (
            str,
            int,
            float,
            bool,
        ),
    ):
        return value

    if value is None:
        return None

    if isinstance(
        value,
        np.generic,
    ):
        return value.item()

    if isinstance(
        value,
        (list, tuple),
    ):
        return [
            _make_json_safe(item)
            for item in value
        ]

    if isinstance(
        value,
        dict,
    ):
        result = {}

        for key, item in value.items():

            safe_value = _make_json_safe(
                item
            )

            if safe_value is not None:
                result[key] = safe_value

        return result

    # ndarray 등은 현재 저장하지 않음
    return None


def _serialize_parameters(
    parameters,
):
    """
    Effect parameter 중
    JSON 저장 가능한 값만 추출한다.
    """

    serialized = {}

    for name, value in parameters.items():

        safe_value = _make_json_safe(
            value
        )

        if safe_value is not None:
            serialized[name] = safe_value

    return serialized


def save_preset(
    chain,
    preset_path,
):
    """
    현재 Effect Chain을 JSON Preset으로 저장한다.
    """

    preset_path = Path(
        preset_path
    )

    preset_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    preset_data = {
        "version": 1,
        "effects": [],
    }

    for effect in chain.effects:

        effect_data = {
            "name": effect.name,
            "bypass": effect.bypass,
            "parameters": (
                _serialize_parameters(
                    effect.parameters
                )
            ),
        }

        preset_data[
            "effects"
        ].append(
            effect_data
        )

    with open(
        preset_path,
        "w",
        encoding="utf-8",
    ) as file:

        json.dump(
            preset_data,
            file,
            indent=4,
            ensure_ascii=False,
        )


def load_preset(
    chain,
    preset_path,
):
    """
    JSON Preset을 읽어서
    현재 Effect Chain에 적용한다.

    - Effect 순서 복원
    - Bypass 복원
    - Parameter 복원
    """

    preset_path = Path(
        preset_path
    )

    with open(
        preset_path,
        "r",
        encoding="utf-8",
    ) as file:

        preset_data = json.load(
            file
        )

    saved_effects = preset_data.get(
        "effects",
        [],
    )

    # 현재 chain의 Effect를 이름으로 찾을 수 있게 만듦
    current_effects = {
        effect.name: effect
        for effect in chain.effects
    }

    reordered_effects = []

    for saved_effect in saved_effects:

        name = saved_effect.get(
            "name"
        )

        effect = current_effects.get(
            name
        )

        # 현재 프로그램에 없는 Effect면 건너뜀
        if effect is None:
            continue

        effect.set_bypass(
            saved_effect.get(
                "bypass",
                False,
            )
        )

        parameters = (
            saved_effect.get(
                "parameters",
                {},
            )
        )

        for parameter_name, value in (
            parameters.items()
        ):

            effect.set_parameter(
                parameter_name,
                value,
            )

        reordered_effects.append(
            effect
        )

    # Preset에 없던 Effect는 뒤에 유지
    for effect in chain.effects:

        if effect not in reordered_effects:

            reordered_effects.append(
                effect
            )

    chain.effects = reordered_effects

    return preset_data