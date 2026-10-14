#!/usr/bin/env -S uv run --script
# /// script
# dependencies = ["websockets==15.0.1"]
# ///
"""Launch a packaged frontend with a saved screen/camera/microphone profile.

    uv run --no-project test/packaged-startup.py --app APP --config-dir DIR --platform windows --expect-arch arm64

Checks the running app over obs-websocket, including capture plugin registration,
saved inputs/scene/profile, and clean shutdown. Hosted runners have no camera or
microphone devices; this check validates package loading and saved-profile state.
The config directory must be absent and is removed after the owned process exits.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import secrets
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time
import uuid
from pathlib import Path

PROFILE = "PackagedSmoke"
SCENE = "Packaged scene"
KINDS = {
    "windows": ("monitor_capture", "dshow_input", "wasapi_input_capture"),
    "macos": ("screen_capture", "macos-avcapture", "coreaudio_input_capture"),
}
NAMES = ("Package screen", "Package camera", "Package microphone")


def require_config_directory(config: Path, platform: str) -> None:
    if platform == "windows":
        appdata = os.environ.get("APPDATA")
        if not appdata:
            raise RuntimeError("APPDATA is required to locate the frontend configuration")
        expected = Path(appdata) / "obs-studio"
    else:
        expected = Path.home() / "Library/Application Support/obs-studio"
    if config.resolve() != expected.resolve():
        raise RuntimeError(f"fixture must use the frontend configuration directory: {expected}")


def write_profile(config: Path, platform: str, port: int, password: str) -> None:
    if config.exists():
        raise RuntimeError(f"refusing to replace existing configuration: {config}")
    config.mkdir(parents=True)
    try:
        _write_profile(config, platform, port, password)
    except BaseException:
        shutil.rmtree(config)
        raise


def _write_profile(config: Path, platform: str, port: int, password: str) -> None:
    (config / "global.ini").write_text(
        "[General]\nLastVersion=4294967295\nMacOSPermissionsDialogLastShown=2147483647\n", encoding="utf-8"
    )
    (config / "user.ini").write_text(
        "[General]\nFirstRun=true\nConfirmOnExit=false\n"
        f"[Basic]\nProfile={PROFILE}\nProfileDir={PROFILE}\n"
        f"SceneCollection={PROFILE}\nSceneCollectionFile={PROFILE}\n", encoding="utf-8"
    )
    profile = config / "basic/profiles" / PROFILE
    profile.mkdir(parents=True)
    (profile / "basic.ini").write_text(
        f"[General]\nName={PROFILE}\n[Video]\nBaseCX=1280\nBaseCY=720\n"
        "OutputCX=1280\nOutputCY=720\nFPSType=0\nFPSCommon=30\n"
        "[Audio]\nSampleRate=48000\nChannelSetup=Stereo\n", encoding="utf-8"
    )
    sources = [
        {"name": name, "id": kind, "uuid": str(uuid.uuid4()), "settings": {}, "volume": 1.0}
        for name, kind in zip(NAMES, KINDS[platform], strict=True)
    ]
    sources.append({
        "name": SCENE, "id": "scene", "uuid": str(uuid.uuid4()),
        "settings": {"items": [
            {"name": source["name"], "source_uuid": source["uuid"], "visible": True}
            for source in sources
        ]},
    })
    scenes = config / "basic/scenes"
    scenes.mkdir(parents=True)
    (scenes / f"{PROFILE}.json").write_text(json.dumps({
        "name": PROFILE, "current_scene": SCENE, "current_program_scene": SCENE,
        "scene_order": [{"name": SCENE}], "sources": sources,
    }), encoding="utf-8")
    websocket = config / "plugin_config/obs-websocket"
    websocket.mkdir(parents=True)
    (websocket / "config.json").write_text(json.dumps({
        "first_load": False, "server_enabled": True, "server_port": port,
        "auth_required": True, "server_password": password, "alerts_enabled": False,
    }), encoding="utf-8")


def require_windows_arch(app: Path, arch: str) -> None:
    with app.open("rb") as executable:
        header = executable.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise RuntimeError("packaged executable has no PE header")
        executable.seek(struct.unpack_from("<I", header, 0x3C)[0])
        pe = executable.read(6)
    if len(pe) != 6 or pe[:4] != b"PE\0\0":
        raise RuntimeError("packaged executable has an invalid PE header")
    machine = struct.unpack_from("<H", pe, 4)[0]
    expected = {"x64": 0x8664, "arm64": 0xAA64}[arch]
    if machine != expected:
        raise RuntimeError(f"packaged executable machine {machine:#x} is not {arch} ({expected:#x})")


def validate_profile(platform: str, kinds: list[str], inputs: list[dict], scene: dict, profile: dict) -> None:
    missing = set(KINDS[platform]) - set(kinds)
    if missing:
        raise RuntimeError(f"packaged capture plugins are missing: {sorted(missing)}")
    actual = {item["inputName"]: item.get("unversionedInputKind", item["inputKind"]) for item in inputs}
    for name, kind in zip(NAMES, KINDS[platform], strict=True):
        if actual.get(name) != kind:
            raise RuntimeError(f"saved input {name!r} has kind {actual.get(name)!r}, expected {kind!r}")
    if scene.get("currentProgramSceneName") != SCENE:
        raise RuntimeError(f"saved scene was not selected: {scene}")
    if profile.get("currentProfileName") != PROFILE:
        raise RuntimeError(f"saved profile was not selected: {profile}")


async def inspect_running_app(proc: subprocess.Popen, port: int, password: str, platform: str) -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parent / "websocket-tier4"))
    from obsws import ObsWsClient, SUB_NONE

    deadline = time.monotonic() + 120
    client = None
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"packaged app exited during startup: {proc.returncode}")
        candidate = ObsWsClient(f"ws://127.0.0.1:{port}")
        try:
            hello = await asyncio.wait_for(candidate.connect(), timeout=5)
            if hello is None:
                raise RuntimeError("websocket handshake did not complete")
            await candidate.identify(
                password=password, auth_challenge=hello["d"]["authentication"], event_subscriptions=SUB_NONE
            )
            if await candidate.wait_op(2, timeout=5) is None:
                raise RuntimeError("websocket authentication failed")
            client = candidate
            break
        except (OSError, TimeoutError):
            await candidate.close_conn()
            await asyncio.sleep(0.25)
        except BaseException:
            await candidate.close_conn()
            raise
    if client is None:
        raise RuntimeError("packaged frontend did not complete startup within 120 seconds")
    try:
        async def request(name, data=None):
            response = await client.request(name, data)
            result = response["d"]
            if not result["requestStatus"]["result"]:
                raise RuntimeError(f"{name} failed: {result['requestStatus']}")
            return result.get("responseData", {})

        kinds = await request("GetInputKindList", {"unversioned": True})
        inputs = await request("GetInputList")
        scene = await request("GetSceneList")
        profile = await request("GetProfileList")
        validate_profile(platform, kinds["inputKinds"], inputs["inputs"], scene, profile)
        items = await request("GetSceneItemList", {"sceneName": SCENE})
        if set(NAMES) - {item["sourceName"] for item in items["sceneItems"]}:
            raise RuntimeError("saved scene lost its capture source items")
    finally:
        await client.close_conn()


def close_frontend(proc: subprocess.Popen, platform: str) -> None:
    if platform == "macos":
        proc.send_signal(signal.SIGINT)
    else:
        import ctypes
        from ctypes import wintypes

        user32 = ctypes.WinDLL("user32", use_last_error=True)
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
        user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        user32.IsWindowVisible.argtypes = [wintypes.HWND]
        user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        windows = []

        @callback_type
        def owned_window(hwnd, _):
            pid = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == proc.pid and user32.IsWindowVisible(hwnd):
                windows.append(hwnd)
            return True

        user32.EnumWindows(owned_window, 0)
        if not windows:
            raise RuntimeError("cannot find the owned frontend window for clean shutdown")
        for hwnd in windows:
            user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE, like the window's close button.
    if proc.wait(timeout=30) != 0:
        raise RuntimeError(f"packaged frontend failed during shutdown: {proc.returncode}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--config-dir", type=Path, required=True)
    parser.add_argument("--platform", choices=KINDS, required=True)
    parser.add_argument("--expect-arch", choices=("x64", "arm64"))
    args = parser.parse_args()
    app = args.app.resolve()
    if args.platform == "macos":
        app = app / "Contents/MacOS/OBS"
    if not app.is_file():
        raise RuntimeError(f"packaged executable is missing: {app}")
    if args.platform == "windows":
        if args.expect_arch is None:
            raise RuntimeError("Windows packages require --expect-arch")
        require_windows_arch(app, args.expect_arch)
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    password = secrets.token_hex(24)
    config = args.config_dir.resolve()
    require_config_directory(config, args.platform)
    # Ownership is established before launching: never delete an existing config.
    write_profile(config, args.platform, port, password)
    proc = None
    try:
        proc = subprocess.Popen(
            [str(app), "--multi", "--disable-shutdown-check", "--disable-updater",
             "--profile", PROFILE, "--collection", PROFILE, "--scene", SCENE], cwd=app.parent
        )
        asyncio.run(inspect_running_app(proc, port, password, args.platform))
        close_frontend(proc, args.platform)
        print("packaged startup: capture plugins registered, saved profile loaded, clean shutdown")
    finally:
        if proc is not None and proc.poll() is None:
            try:
                close_frontend(proc, args.platform)
            except (RuntimeError, OSError, subprocess.TimeoutExpired):
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait(timeout=10)
        shutil.rmtree(config)


if __name__ == "__main__":
    main()
