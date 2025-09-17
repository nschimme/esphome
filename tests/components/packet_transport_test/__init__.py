import pytest
from tests.integration.conftest import run_compiled

pytestmark = pytest.mark.asyncio

async def test_host_compile(run_compiled):
    with open("tests/components/packet_transport_test/test_host.yaml") as f:
        content = f.read()
    await run_compiled(content)

async def test_remote_compile(run_compiled):
    with open("tests/components/packet_transport_test/test_remote.yaml") as f:
        content = f.read()
    await run_compiled(content)
