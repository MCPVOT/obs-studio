#!/usr/bin/env -S uv run --script
"""Run a staged frontend with an owned scene/profile on an isolated X display."""
from __future__ import annotations

import argparse
import configparser
import json
import os
from pathlib import Path
import select
import secrets
import socket
import subprocess
import time
import uuid


def profile(runtime: Path) -> Path:
    if os.environ.get("OBS_BUILD_BROWSER"):
        (runtime / "browser-panel.html").write_text(
            '<html><body style="background:#11cb4d"><script>'
            'document.title="Native CEF Panel Ready";</script></body></html>'
        )
    config = runtime / "user-config/obs-studio-rust"
    config.mkdir(parents=True)
    browser_accel = "BrowserHWAccel=false\n" if os.environ.get("LIBGL_ALWAYS_SOFTWARE") == "1" else ""
    (config / "global.ini").write_text(f"[General]\nLastVersion=4294967295\n{browser_accel}")
    (config / "user.ini").write_text(
        "[General]\nLanguage=en-US\nFirstRun=true\nConfirmOnExit=false\n"
        "[Basic]\nProfile=NativeSmoke\nProfileDir=NativeSmoke\n"
        "SceneCollection=NativeSmoke\nSceneCollectionFile=NativeSmoke\n"
    )
    selected = config / "basic/profiles/NativeSmoke"
    selected.mkdir(parents=True)
    (selected / "basic.ini").write_text(
        "[General]\nName=NativeSmoke\n[Video]\nBaseCX=640\nBaseCY=360\n"
        "OutputCX=640\nOutputCY=360\nFPSType=0\nFPSCommon=30\n"
        "[Audio]\nSampleRate=48000\nChannelSetup=Stereo\n"
    )
    if os.environ.get("OBS_BUILD_APP_RECORD"):
        recordings = runtime / "recordings"
        recordings.mkdir()
        encoder = "nvenc" if os.environ.get("OBS_BUILD_NVENC") else "x264"
        with (selected / "basic.ini").open("a") as output:
            output.write(
                "[Output]\nMode=Simple\n[SimpleOutput]\n"
                f"FilePath={recordings}\nRecFormat2=mkv\nRecQuality=Small\n"
                f"RecEncoder={encoder}\nRecAudioEncoder=aac\nRecTracks=1\n"
                "StreamAudioEncoder=aac\nABitrate=128\nVBitrate=1000\n"
            )
    color = {"name": "Native white", "id": "color_source_v3", "uuid": str(uuid.uuid4()),
             "settings": {"width": 640, "height": 360, "color": 0xFFFFFFFF}, "volume": 1.0}
    scene = {"name": "Native scene", "id": "scene", "uuid": str(uuid.uuid4()),
             "settings": {"items": [{"name": color["name"], "source_uuid": color["uuid"], "visible": True}]}}
    scenes = config / "basic/scenes"
    scenes.mkdir(parents=True)
    modules = {}
    manifest = runtime / "expected-scripting.json"
    if manifest.exists():
        scripts = runtime / "saved-scripts"
        scripts.mkdir()
        modules["scripts-tool"] = []
        for language in json.loads(manifest.read_text())["languages"]:
            path = scripts / ("native_gui.lua" if language == "lua" else "native_gui.py")
            path.write_text(LUA_SCRIPT if language == "lua" else PYTHON_SCRIPT)
            modules["scripts-tool"].append({"path": str(path), "settings": {"seed": 41}})
    (scenes / "NativeSmoke.json").write_text(json.dumps({
        "name": "NativeSmoke", "current_scene": "Native scene", "current_program_scene": "Native scene",
        "scene_order": [{"name": "Native scene"}], "sources": [color, scene], "modules": modules,
    }))
    if os.environ.get("OBS_BUILD_WEBSOCKET"):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        password = secrets.token_hex(16)
        directory = config / "plugin_config/obs-websocket"
        directory.mkdir(parents=True)
        (directory / "config.json").write_text(json.dumps({
            "first_load": False, "server_enabled": True, "server_port": port,
            "alerts_enabled": False, "auth_required": True, "server_password": password,
        }))
        (runtime / "expected-websocket.json").write_text(json.dumps({"port": port, "password": password}))
    return config


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--inspector", type=Path, required=True)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    config = profile(runtime)
    env = dict(os.environ)
    env.update(XDG_CONFIG_HOME=str(runtime / "user-config"), XDG_DATA_HOME=str(runtime / "user-data"),
               XDG_CACHE_HOME=str(runtime / "user-cache"), QT_QPA_PLATFORM="xcb",
               QT_QPA_PLATFORMTHEME="", QT_STYLE_OVERRIDE="Fusion")
    # Desktop-session paths may select plugins from a different Qt installation.
    # Use the linked Qt library's own default plugin paths for this isolated test.
    env.pop("QT_PLUGIN_PATH", None)
    env.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
    for variable in ["PYTHONHOME", "PYTHONPATH", "LUA_PATH", "LUA_CPATH"]:
        env.pop(variable, None)
    manifest = runtime / "expected-scripting.json"
    scripting = json.loads(manifest.read_text()) if manifest.exists() else None
    env["OBS_BUILD_GUI_EVIDENCE"] = str(runtime)
    env["OBS_BUILD_GUI_SCRIPT_COUNT"] = str(len(scripting["languages"])) if scripting else "-1"
    env.pop("LD_LIBRARY_PATH", None)
    env.pop("OBS_DATA_PATH", None)
    env.pop("WAYLAND_DISPLAY", None)
    recording = bool(os.environ.get("OBS_BUILD_APP_RECORD"))
    if os.environ.get("OBS_BUILD_NVENC"):
        assert (runtime / "bin/obs-nvenc-test").is_file(), "NVENC capability helper was not staged"
    command = [str(runtime / "bin/obs-studio-rust"), "--multi", "--only-bundled-plugins",
               "--disable-shutdown-check", "--disable-updater", "--disable-missing-files-check", "--unfiltered_log"]
    if os.environ.get("OBS_BUILD_BROWSER"):
        # This fresh disposable profile contains no user credentials. Avoid an
        # interactive desktop keyring unlock blocking CEF's initial cookie load
        # in the isolated Xvfb/D-Bus session. Production launch is unaffected.
        command.append("--password-store=basic")
    # The wire probe starts recording after its other protocol checks, keeping
    # dependency setup and network checks outside the timed recording interval.
    wire_recording = recording and bool(os.environ.get("OBS_BUILD_WEBSOCKET"))
    if recording and not wire_recording:
        command.append("--startrecording")
    app = subprocess.Popen(command, env=env, cwd=runtime, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log = bytearray()
    drained = False
    try:
        deadline = time.monotonic() + 45
        ready = False
        while time.monotonic() < deadline:
            if app.poll() is not None:
                raise RuntimeError(f"frontend exited during startup: {app.returncode}")
            if select.select([app.stdout], [], [], 0.2)[0]:
                chunk = os.read(app.stdout.fileno(), 65536)
                log.extend(chunk)
                (runtime / "application.log").write_bytes(log)
            if b"error: cc::Build" in log:
                raise RuntimeError(f"frontend GUI probe failed; see {runtime / 'application.log'}")
            tools_ready = not (os.environ.get("OBS_BUILD_FRONTEND_TOOLS") or os.environ.get("OBS_BUILD_WEBSOCKET")) or b"cc::Build frontend Tools dialogs verified" in log
            if b"==== Startup complete" in log and tools_ready and (not recording or wire_recording or b"==== Recording Start" in log):
                found = subprocess.run([str(args.inspector), "find", str(app.pid)], env=env, capture_output=True)
                if found.returncode == 0:
                    ready = True
                    break
        if not ready:
            raise RuntimeError("frontend did not finish startup and map a normal main window")
        maps = Path(f"/proc/{app.pid}/maps").read_text()
        (runtime / "application.maps").write_text(maps)
        for name in ["libobs.so.30", "libobs-frontend-api.so.30", "libobs-opengl.so.33"]:
            assert str(runtime / "lib64" / name) in maps, f"staged {name} was not loaded"
        for name in json.loads((runtime / "expected-modules.json").read_text()):
            assert str(runtime / "lib64/obs-modules/core" / (name + ".so")) in maps, f"module {name} was not loaded"
        if os.environ.get("OBS_BUILD_BROWSER"):
            assert str(runtime / "lib64/obs-modules/core/libcef.so") in maps, "staged CEF was not loaded"
        if os.environ.get("OBS_BUILD_VLC"):
            if os.environ.get("OBS_BUILD_VLC_ABSENT"):
                assert "libvlc.so.5" not in maps, "VLC absence fixture found a runtime"
                assert b"VLC video source disabled" in log, "missing VLC was not handled gracefully"
            else:
                assert "libvlc.so.5" in maps, "VLC runtime was not loaded dynamically"
        if os.environ.get("OBS_BUILD_NVENC"):
            assert str(runtime / "lib64/obs-modules/core/obs-nvenc.so") in maps, "staged NVENC was not loaded"
        if scripting:
            for relative in [scripting["library"], *scripting["modules"]]:
                assert str(runtime / relative) in maps, f"staged scripting artifact was not loaded: {relative}"
            for language in scripting["languages"]:
                assert f"native-gui-{language}-loaded".encode() in log, f"saved {language} script did not run"
        if os.environ.get("OBS_BUILD_FRONTEND_TOOLS"):
            for name in ["tools-scene-switcher.png", "tools-output-timer.png", *(["tools-scripts.png"] if scripting else [])]:
                assert (runtime / name).is_file(), f"missing Tools dialog evidence: {name}"
        if os.environ.get("OBS_BUILD_WEBSOCKET"):
            for name in ["websocket-settings.png", "websocket-connect-info.png"]:
                assert (runtime / name).is_file(), f"missing websocket dialog evidence: {name}"
            wire = ["uv", "run", "--no-project", str(Path(__file__).with_name("websocket_probe.py")),
                    "--runtime", str(runtime)]
            if recording:
                wire.append("--recording")
            subprocess.run(wire, env=env, check=True, timeout=60)
        if os.environ.get("OBS_BUILD_BROWSER"):
            verify_browser_helpers(runtime, app.pid)
        assert b"Native white" in log, "saved color source was not restored"
        if recording:
            # Keep the actual frontend output running long enough to decode a
            # meaningful recording. Window-close must finalize the mux helper.
            time.sleep(3)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "x11grab", "-video_size", "1280x720",
                        "-i", env["DISPLAY"], "-frames:v", "1", str(runtime / "application.png")], env=env,
                       check=True, capture_output=True, timeout=15)
        subprocess.run([str(args.inspector), "close", str(app.pid)], env=env, check=True, capture_output=True)
        remainder, _ = app.communicate(timeout=30)
        log.extend(remainder)
        drained = True
        (runtime / "application.log").write_bytes(log)
        assert app.returncode == 0, f"frontend shutdown failed: {app.returncode}"
        assert b"==== Shutting down" in log, "frontend did not enter normal shutdown"
        saved = json.loads((config / "basic/scenes/NativeSmoke.json").read_text())
        assert any(source.get("name") == "Native white" for source in saved["sources"]), "saved scene was lost"
        if os.environ.get("OBS_BUILD_VLC") and not os.environ.get("OBS_BUILD_VLC_ABSENT"):
            source = next(source for source in saved["sources"] if source.get("name") == "Native VLC")
            assert source["settings"]["playlist"][0]["value"] == str(runtime / "vlc-white.mkv"), "VLC playlist was not saved"
        if os.environ.get("OBS_BUILD_BROWSER"):
            source = next(source for source in saved["sources"] if source.get("name") == "Native browser")
            assert source["settings"]["local_file"] == str(runtime / "browser-source.html")
            if not os.environ.get("OBS_BUILD_DISABLE_BROWSER_PANELS"):
                settings = configparser.ConfigParser(interpolation=None)
                settings.read(config / "user.ini")
                docks = json.loads(settings["BasicWindow"]["ExtraBrowserDocks"])
                assert any(dock["title"] == "Native CEF Panel" and dock["url"] == (runtime / "browser-panel.html").as_uri() for dock in docks), "browser dock was not saved"
        if scripting:
            scripts = saved["modules"]["scripts-tool"]
            assert len(scripts) == len(scripting["languages"]), "saved script list changed"
            for script in scripts:
                assert script["settings"]["updated"] == 42 and script["settings"]["saved"] == 43, "script callbacks did not persist settings"
            for language in scripting["languages"]:
                assert f"native-gui-{language}-unloaded".encode() in log, f"{language} unload callback did not run"
        if recording:
            if os.environ.get("OBS_BUILD_NVENC"):
                assert b"[obs-nvenc: 'simple_video_recording'] settings:" in log, "frontend did not use NVENC"
            recordings = list((runtime / "recordings").glob("*.mkv"))
            assert len(recordings) == 1, "frontend did not create the requested recording"
            verify_recording(recordings[0], env)
        print(f"OBS normal window, staged libraries/modules, saved scene, screenshot and clean shutdown: {runtime}")
    finally:
        if not drained:
            if app.poll() is None:
                app.terminate()
            try:
                remainder, _ = app.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
                remainder, _ = app.communicate(timeout=5)
            log.extend(remainder)
        (runtime / "application.log").write_bytes(log)


def verify_browser_helpers(runtime: Path, parent: int) -> None:
    descendants = {parent}
    found = {}
    for _ in range(8):
        children = set()
        for pid in descendants:
            for entry in Path(f"/proc/{pid}/task").glob("*/children"):
                try:
                    children.update(int(value) for value in entry.read_text().split())
                except FileNotFoundError:
                    pass
        new = children - descendants
        descendants.update(new)
        if not new:
            break
    helper = str(runtime / "lib64/obs-modules/core/obs-browser-page")
    for pid in descendants - {parent}:
        try:
            command = Path(f"/proc/{pid}/cmdline").read_bytes().replace(b"\0", b" ").decode()
            if helper in command and "--type=renderer" in command:
                maps = Path(f"/proc/{pid}/maps").read_text()
                assert str(runtime / "lib64/obs-modules/core/libcef.so") in maps, "renderer used an unstaged CEF"
                found[pid] = command
        except FileNotFoundError:
            pass
    assert found, "staged browser renderer helper was not running"
    (runtime / "browser-helpers.json").write_text(json.dumps(found, indent=2))


def verify_recording(path: Path, env: dict[str, str]) -> None:
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-count_frames", "-show_streams", "-show_format", "-of", "json", str(path)],
        env=env, check=True, capture_output=True, timeout=15,
    )
    report = json.loads(result.stdout)
    path.with_suffix(".ffprobe.json").write_text(json.dumps(report, indent=2))
    video = next(stream for stream in report["streams"] if stream["codec_type"] == "video")
    audio = next(stream for stream in report["streams"] if stream["codec_type"] == "audio")
    assert video["codec_name"] == "h264" and (video["width"], video["height"]) == (640, 360)
    assert int(video["nb_read_frames"]) >= 60, "frontend recording has too few decoded frames"
    assert audio["codec_name"] == "aac" and int(audio["sample_rate"]) == 48000 and audio["channels"] == 2
    assert 2 <= float(report["format"]["duration"]) <= 15, "unexpected frontend recording duration"
    frame = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", str(path), "-vf", "scale=1:1", "-frames:v", "1",
         "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"],
        env=env, check=True, capture_output=True, timeout=15,
    ).stdout
    assert len(frame) == 3 and min(frame) >= 240, "frontend did not record the restored white source"
    print(f"Validated actual frontend H264/AAC recording and white frame: {path}")


LUA_SCRIPT = """
obs = obslua
function script_description() return 'Native GUI Lua' end
function script_defaults(settings) obs.obs_data_set_default_int(settings, 'seed', 41) end
function script_update(settings) obs.obs_data_set_int(settings, 'updated', obs.obs_data_get_int(settings, 'seed') + 1) end
function script_save(settings) obs.obs_data_set_int(settings, 'saved', obs.obs_data_get_int(settings, 'updated') + 1) end
function script_load(settings) obs.script_log(obs.LOG_INFO, 'native-gui-lua-loaded') end
function script_unload() obs.script_log(obs.LOG_INFO, 'native-gui-lua-unloaded') end
function script_properties()
    local props = obs.obs_properties_create()
    obs.obs_properties_add_int(props, 'seed', 'Seed', 0, 100, 1)
    return props
end
"""

PYTHON_SCRIPT = """
import obspython as obs
def script_description(): return 'Native GUI Python'
def script_defaults(settings): obs.obs_data_set_default_int(settings, 'seed', 41)
def script_update(settings): obs.obs_data_set_int(settings, 'updated', obs.obs_data_get_int(settings, 'seed') + 1)
def script_save(settings): obs.obs_data_set_int(settings, 'saved', obs.obs_data_get_int(settings, 'updated') + 1)
def script_load(settings): obs.script_log(obs.LOG_INFO, 'native-gui-python-loaded')
def script_unload(): obs.script_log(obs.LOG_INFO, 'native-gui-python-unloaded')
def script_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_int(props, 'seed', 'Seed', 0, 100, 1)
    return props
"""


if __name__ == "__main__":
    main()
