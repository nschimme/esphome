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
