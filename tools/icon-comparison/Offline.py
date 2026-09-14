import os
import sys

from Models import ARTIFACTS


def enforce_offline() -> None:
    os.environ.update({
        "HF_HUB_OFFLINE": "1",
        "HF_HUB_DISABLE_TELEMETRY": "1",
        "DO_NOT_TRACK": "1",
        "HF_HOME": str(ARTIFACTS / "Cache" / "HuggingFace"),
        "TOKENIZERS_PARALLELISM": "false",
    })

    def audit(event: str, args: tuple) -> None:
        if event in {"socket.connect", "socket.getaddrinfo", "socket.bind"}:
            raise RuntimeError(f"Network operation forbidden during offline comparison: {event}")

    sys.addaudithook(audit)
