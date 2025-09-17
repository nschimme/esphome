import pytest
from tests.integration.conftest import run_compiled

pytestmark = pytest.mark.asyncio

async def test_loopback(run_compiled):
    with open("tests/components/packet_transport_test/test_loopback.yaml") as f:
        content = f.read()
    await run_compiled(content)
