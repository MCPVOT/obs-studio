#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["aiortc==1.15.0", "aiohttp==3.14.5", "numpy==2.5.4"]
# ///
"""Receive and decode the real native WHIP fixture while running its command."""

import argparse
import asyncio
import json
import os
from pathlib import Path
import signal
import uuid

from aiohttp import web
from aiortc import RTCConfiguration, RTCPeerConnection, RTCSessionDescription
from aiortc.mediastreams import MediaStreamError
import numpy as np


async def exercise(args):
    sessions = {}
    peers = {}
    consumers = []
    args.output.parent.mkdir(parents=True, exist_ok=True)

    def save():
        args.output.write_text(json.dumps(list(sessions.values()), indent=2) + "\n")

    def authorize(request):
        if request.headers.get("Authorization") != "Bearer cc-build-whip-token":
            raise web.HTTPUnauthorized()

    async def consume(track, state):
        previous = None
        try:
            while True:
                frame = await track.recv()
                if frame.pts is None or (
                    previous is not None and frame.pts <= previous
                ):
                    raise AssertionError(f"{track.kind} timestamp did not advance")
                previous = frame.pts
                if track.kind == "video":
                    if (frame.width, frame.height) != (64, 64):
                        raise AssertionError("unexpected decoded video geometry")
                    rgb = frame.to_ndarray(format="rgb24")
                    expected = 255 if args.color == "white" else 0
                    if np.max(np.abs(rgb.astype(float) - expected)) > 8:
                        raise AssertionError("decoded video differs from fixture scene")
                    state["video_frames"] += 1
                else:
                    if frame.sample_rate != 48000 or len(frame.layout.channels) != 2:
                        raise AssertionError("expected 48 kHz stereo Opus audio")
                    raw = frame.to_ndarray()
                    samples = raw.T if frame.format.is_planar else raw.reshape(-1, 2)
                    samples = samples.astype(float) / (
                        np.iinfo(raw.dtype).max if raw.dtype.kind in "iu" else 1
                    )
                    rms = float(np.sqrt(np.mean(samples**2)))
                    frequencies = np.fft.rfftfreq(len(samples), 1 / 48000)
                    dominant = float(
                        frequencies[np.argmax(np.abs(np.fft.rfft(samples[:, 0])))]
                    )
                    state["audio_frames"] += 1
                    if 0.10 < rms < 0.18 and 950 <= dominant <= 1050:
                        state["tone_frames"] += 1
                        state["tone_rms"] = rms
                        state["tone_frequency"] = dominant
                save()
        except MediaStreamError:
            pass  # The sender closes its real peer before deleting the resource.
        except Exception as error:
            state["errors"].append(f"{type(error).__name__}: {error}")
            save()

    async def offer(request):
        authorize(request)
        if request.content_type != "application/sdp":
            raise web.HTTPUnsupportedMediaType()
        identifier = uuid.uuid4().hex
        peer = RTCPeerConnection(RTCConfiguration(iceServers=[]))
        peers[identifier] = peer
        state = {
            "id": identifier,
            "states": [],
            "video_frames": 0,
            "audio_frames": 0,
            "tone_frames": 0,
            "deleted": False,
            "errors": [],
        }
        sessions[identifier] = state

        @peer.on("connectionstatechange")
        async def changed():
            state["states"].append(peer.connectionState)
            save()

        @peer.on("track")
        def track_received(track):
            consumers.append(asyncio.create_task(consume(track, state)))

        try:
            sdp = await request.text()
            args.output.with_name(
                f"{args.output.stem}-{identifier}-offer.sdp"
            ).write_text(sdp)
            await peer.setRemoteDescription(
                RTCSessionDescription(sdp=sdp, type="offer")
            )
            await peer.setLocalDescription(await peer.createAnswer())
            answer = peer.localDescription.sdp
            args.output.with_name(
                f"{args.output.stem}-{identifier}-answer.sdp"
            ).write_text(answer)
            save()
            return web.Response(
                status=201,
                text=answer,
                content_type="application/sdp",
                headers={"Location": f"/resource/{identifier}"},
            )
        except Exception as error:
            state["errors"].append(f"offer: {type(error).__name__}: {error}")
            save()
            raise

    async def delete(request):
        authorize(request)
        identifier = request.match_info["identifier"]
        if identifier not in peers:
            raise web.HTTPNotFound()
        sessions[identifier]["deleted"] = True
        await peers[identifier].close()
        save()
        return web.Response(status=200)

    app = web.Application()
    app.router.add_post("/whip", offer)
    app.router.add_delete("/resource/{identifier}", delete)
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, "127.0.0.1", 0).start()
    port = runner.addresses[0][1]
    env = dict(os.environ, OBS_BUILD_WHIP_ENDPOINT=f"http://127.0.0.1:{port}/whip")
    process = None
    try:
        process = await asyncio.create_subprocess_exec(
            *args.command, env=env, start_new_session=True
        )
        result = await asyncio.wait_for(process.wait(), timeout=args.timeout)
        if result:
            raise AssertionError(f"native command failed: {result}")
    finally:
        if process is not None and process.returncode is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                await asyncio.wait_for(process.wait(), timeout=5)
            except asyncio.TimeoutError:
                os.killpg(process.pid, signal.SIGKILL)
                await process.wait()
        for peer in peers.values():
            await peer.close()
        await asyncio.gather(*consumers)
        await runner.cleanup()
        save()
    assert len(sessions) == args.sessions, (
        f"expected {args.sessions} streams: {sessions}"
    )
    for state in sessions.values():
        assert not state["errors"], state
        assert "connected" in state["states"] and state["deleted"], state
        assert state["video_frames"] >= 12, state
        assert state["audio_frames"] >= 20 and state["tone_frames"] >= 10, state
    print(
        f"Validated {len(sessions)} real WHIP streams: decoded video, Opus tone, timestamps and DELETE"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--color", choices=["black", "white"], default="black")
    parser.add_argument("--sessions", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    if options.command and options.command[0] == "--":
        options.command.pop(0)
    if options.timeout <= 0:
        parser.error("--timeout must be positive")
    if not options.command:
        parser.error("provide the native test command after --")
    asyncio.run(exercise(options))
