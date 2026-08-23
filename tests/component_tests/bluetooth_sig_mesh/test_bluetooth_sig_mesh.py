from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize(
    "config_file",
    [
        "test_esp32.yaml",
        "test_rp2040.yaml",
        "test_bk72xx.yaml",
        "test_ln882h.yaml",
    ],
)
def test_bluetooth_sig_mesh_config_generates(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    assert "bluetooth_sig_mesh::BluetoothSIGMesh" in main_cpp
