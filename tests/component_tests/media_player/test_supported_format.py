"""Tests for the supported-format struct the media player advertises to Home Assistant."""

from esphome.components import media_player

FORMAT = {"sample_rate": 48000, "num_channels": 2}


def _build(format_: str) -> str:
    return str(
        media_player.build_supported_format_struct(
            {**FORMAT, "format": format_},
            media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"],
        )
    )


def test_adts_is_advertised_as_adts() -> None:
    assert '.format = "adts"' in _build("ADTS")


def test_adts_omits_sample_bytes() -> None:
    # Home Assistant's ffmpeg AAC encoder rejects -sample_fmt s16
    assert "sample_bytes" not in _build("ADTS")


def test_flac_keeps_sample_bytes() -> None:
    assert ".sample_bytes = 2" in _build("FLAC")


def _requested_codecs(**pipelines: str) -> dict[str, bool]:
    """Run the codec request for pipelines of the given formats and report what was enabled."""
    from esphome.components import audio
    from esphome.core import CORE

    CORE.data.pop(audio.DOMAIN, None)
    config = {name: {"format": fmt} for name, fmt in pipelines.items()}
    media_player.request_codecs_for_format_configs(config, list(config))
    data = CORE.data[audio.DOMAIN]
    return {"aac": data.aac_support, "flac": data.flac_support}


def test_none_does_not_enable_aac() -> None:
    assert _requested_codecs(media="NONE") == {"aac": False, "flac": True}


def test_adts_enables_only_aac() -> None:
    assert _requested_codecs(media="ADTS") == {"aac": True, "flac": False}


def test_none_with_adts_enables_both() -> None:
    assert _requested_codecs(media="NONE", announcement="ADTS") == {
        "aac": True,
        "flac": True,
    }
