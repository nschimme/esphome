from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize(
    ("config_file", "expected_class"),
    [
        ("test_esp32.yaml", "ESP32BluetoothSIGMesh"),
        ("test_rp2040.yaml", "RP2040BluetoothSIGMesh"),
        ("test_bk72xx.yaml", "BK72XXBluetoothSIGMesh"),
        ("test_ln882h.yaml", "LN882HBluetoothSIGMesh"),
    ],
)
def test_bluetooth_sig_mesh_config_generates(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected_class: str,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    assert f"bluetooth_sig_mesh::{expected_class}" in main_cpp
