#!/usr/bin/env python3
"""Minimal local MCP server for AOI Vision Lab Qt.

The server speaks JSON-RPC over stdin/stdout and has no Python dependencies.
It delegates image processing to the C++ executable, so GUI and AI automation
always use exactly the same OpenCV inspection engine.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
EXE_CANDIDATES = [
    Path(os.environ["AOI_VISION_EXE"]).expanduser() if os.environ.get("AOI_VISION_EXE") else None,
    ROOT / "AOIVisionLabQt.exe",  # Portable package layout.
    ROOT / "build" / "Desktop_Qt_6_10_2_MinGW_64_bit-Debug" / "AOIVisionLabQt.exe",
]
EXE = next((path.resolve() for path in EXE_CANDIDATES if path and path.is_file()),
           ROOT / "AOIVisionLabQt.exe")
QT_BIN = Path(r"C:\Qt\6.10.2\mingw_64\bin")
OPENCV_BIN = ROOT / "vcpkg_installed" / "x64-mingw-dynamic" / "debug" / "bin"


def send(message: dict[str, Any]) -> None:
    # MCP stdio uses one compact JSON-RPC message per line.  Nothing else may
    # be printed to stdout because clients treat it as protocol traffic.
    data = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    sys.stdout.buffer.write(data + b"\n")
    sys.stdout.buffer.flush()


def receive() -> dict[str, Any] | None:
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line.strip():
            return json.loads(line.decode("utf-8"))


def analyze(arguments: dict[str, Any]) -> dict[str, Any]:
    reference = Path(str(arguments.get("reference", ""))).expanduser().resolve()
    candidate = Path(str(arguments.get("candidate", ""))).expanduser().resolve()
    if not reference.is_file():
        raise ValueError(f"Reference image does not exist: {reference}")
    if not candidate.is_file():
        raise ValueError(f"Candidate image does not exist: {candidate}")
    if not EXE.is_file():
        raise RuntimeError(f"AOI executable was not found: {EXE}")

    requested_report = arguments.get("report")
    report = (Path(str(requested_report)).expanduser().resolve()
              if requested_report else Path(tempfile.gettempdir()) / "aoi-vision-report.json")
    command = [str(EXE), "--reference", str(reference), "--inspect", str(candidate),
               "--report", str(report)]
    visualization = arguments.get("visualization")
    if visualization:
        command.extend(["--visualization", str(Path(str(visualization)).expanduser().resolve())])

    environment = os.environ.copy()
    # A portable package keeps all DLLs beside the executable. Development
    # builds additionally need the Qt and vcpkg directories on PATH.
    search_paths = [str(EXE.parent)]
    search_paths.extend(str(path) for path in (QT_BIN, OPENCV_BIN) if path.is_dir())
    search_paths.append(environment.get("PATH", ""))
    environment["PATH"] = os.pathsep.join(search_paths)
    completed = subprocess.run(command, env=environment, capture_output=True,
                               text=True, timeout=120, check=False)
    if not report.is_file():
        raise RuntimeError(completed.stderr.strip() or "AOI did not create its JSON report.")
    result = json.loads(report.read_text(encoding="utf-8"))
    result["report"] = str(report)
    return result


TOOLS = [{
    "name": "analyze_board",
    "description": "Load a PCB reference image, compare a candidate board and return an AOI JSON report.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "reference": {"type": "string", "description": "Absolute path to the reference PCB image."},
            "candidate": {"type": "string", "description": "Absolute path to the board image to inspect."},
            "report": {"type": "string", "description": "Optional output path for the JSON report."},
            "visualization": {"type": "string", "description": "Optional output path for the marked PNG/JPG."}
        },
        "required": ["reference", "candidate"],
        "additionalProperties": False
    }
}]


def handle(request: dict[str, Any]) -> None:
    method = request.get("method")
    request_id = request.get("id")
    if method == "initialize":
        result = {
            "protocolVersion": "2025-06-18",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "aoi-vision-lab", "version": "0.1.0"}
        }
    elif method == "tools/list":
        result = {"tools": TOOLS}
    elif method == "tools/call":
        params = request.get("params", {})
        if params.get("name") != "analyze_board":
            raise ValueError("Unknown tool")
        data = analyze(params.get("arguments", {}))
        result = {"content": [{"type": "text", "text": json.dumps(data, ensure_ascii=False, indent=2)}],
                  "structuredContent": data}
    elif method in ("notifications/initialized", "ping"):
        if request_id is None:
            return
        result = {}
    else:
        if request_id is None:
            return
        raise ValueError(f"Unsupported method: {method}")
    if request_id is not None:
        send({"jsonrpc": "2.0", "id": request_id, "result": result})


def main() -> int:
    while True:
        request = receive()
        if request is None:
            return 0
        try:
            handle(request)
        except Exception as error:  # MCP must return errors instead of crashing.
            if request.get("id") is not None:
                send({"jsonrpc": "2.0", "id": request["id"],
                      "error": {"code": -32603, "message": str(error)}})


if __name__ == "__main__":
    raise SystemExit(main())
