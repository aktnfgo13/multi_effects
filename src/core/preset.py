import json
from pathlib import Path

import numpy as np


PRESET_VERSION = 2


def _make_json_safe(
    value,
):
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

            safe_value = (
                _make_json_safe(
                    item
                )
            )

            if safe_value is not None:
                result[key] = safe_value

        return result

    # ndarray 등은 현재 저장하지 않음
    return None


def _serialize_parameters(
    parameters,
):
    serialized = {}

    for name, value in parameters.items():

        safe_value = _make_json_safe(
            value
        )

        if safe_value is not None:
            serialized[
                name
            ] = safe_value

    return serialized


def save_preset(
    chain,
    preset_path,
):
    """
    Preset Version 2

    Effect를 name이 아닌
    effect_id 기준으로 저장한다.
    """

    preset_path = Path(
        preset_path
    )

    preset_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    preset_data = {
        "version": PRESET_VERSION,
        "effects": [],
    }

    used_ids = set()

    for effect in chain.effects:

        if effect.effect_id is None:
            raise ValueError(
                f"Effect '{effect.name}' "
                "has no effect_id."
            )

        if effect.effect_id in used_ids:
            raise ValueError(
                "Duplicate effect_id: "
                f"{effect.effect_id}"
            )

        used_ids.add(
            effect.effect_id
        )

        effect_data = {
            "id": effect.effect_id,
            "type": effect.effect_type,
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


def _apply_saved_effect(
    effect,
    saved_effect,
):
    """
    저장된 하나의 Effect 상태를
    현재 EffectBlock에 적용
    """

    saved_type = saved_effect.get(
        "type"
    )

    if (
        saved_type is not None
        and saved_type
        != effect.effect_type
    ):
        raise ValueError(
            "Effect type mismatch: "
            f"{effect.effect_id} "
            f"({effect.effect_type} != "
            f"{saved_type})"
        )

    # 사용자가 지정한 표시 이름도 복원
    saved_name = saved_effect.get(
        "name"
    )

    if saved_name:
        effect.name = saved_name

    effect.set_bypass(
        saved_effect.get(
            "bypass",
            False,
        )
    )

    parameters = saved_effect.get(
        "parameters",
        {},
    )

    for parameter_name, value in (
        parameters.items()
    ):

        effect.set_parameter(
            parameter_name,
            value,
        )


def _load_version_2(
    chain,
    saved_effects,
):
    """
    Version 2:
    effect_id 기준 복원
    """

    current_effects = {
        effect.effect_id: effect
        for effect in chain.effects
    }

    reordered_effects = []

    for saved_effect in saved_effects:

        effect_id = saved_effect.get(
            "id"
        )

        if effect_id is None:
            continue

        effect = current_effects.get(
            effect_id
        )

        # 현재 Chain에 없는 Effect
        if effect is None:
            continue

        _apply_saved_effect(
            effect,
            saved_effect,
        )

        reordered_effects.append(
            effect
        )

    # Preset에 없던 Effect는 뒤에 유지
    for effect in chain.effects:

        if (
            effect
            not in reordered_effects
        ):
            reordered_effects.append(
                effect
            )

    chain.effects = reordered_effects


def _load_version_1(
    chain,
    saved_effects,
):
    """
    기존 Version 1 호환용.

    Version 1은 name으로 식별했기 때문에
    동일 이름 Effect 여러 개는 지원하지 않는다.
    """

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

        if effect is None:
            continue

        effect.set_bypass(
            saved_effect.get(
                "bypass",
                False,
            )
        )

        parameters = saved_effect.get(
            "parameters",
            {},
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

    for effect in chain.effects:

        if (
            effect
            not in reordered_effects
        ):
            reordered_effects.append(
                effect
            )

    chain.effects = reordered_effects


def load_preset(
    chain,
    preset_path,
):
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

    version = preset_data.get(
        "version",
        1,
    )

    saved_effects = preset_data.get(
        "effects",
        [],
    )

    if version >= 2:

        _load_version_2(
            chain,
            saved_effects,
        )

    else:

        _load_version_1(
            chain,
            saved_effects,
        )

    return preset_data