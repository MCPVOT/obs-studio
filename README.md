# obs-studio-rust

> [!IMPORTANT]
> **We’re unifying OBS into one hybrid Rust/C/C++ build.**
> Prioritize migrating code to Rust and retiring the C/C++ implementations it replaces.
> Each completed port becomes part of the application, with compatibility and parity
> checks supporting the transition.

[![OBS Rust Discord Server](https://img.shields.io/badge/Discord-OBS%20Rust-5865F2?logo=discord&logoColor=white)](https://discord.gg/6MWWX4x9pf)
[![Reddit: r/ObsRust](https://img.shields.io/badge/Reddit-r%2FObsRust-FF4500?logo=reddit&logoColor=white)](https://www.reddit.com/r/ObsRust/)
[![Discussions: got an idea?](docs/images/discussions.svg)](https://github.com/obs-rust/obs-studio/discussions)
[![Contributor leaderboard workflow status](https://github.com/obs-rust/obs-studio/actions/workflows/leaderboard.yml/badge.svg?branch=master)](https://github.com/obs-rust/obs-studio/actions/workflows/leaderboard.yml)
[![Merge queue for master](docs/images/merge-queue.svg)](https://github.com/obs-rust/obs-studio/queue/master)

<p align="center"><img src="docs/images/obs-rust-logo.png" alt="obs-rust logo" width="128"></p>

This is an independent rewrite of the obsproject's obs-studio.

Same obs studio, but without the bugs. AI generated issues and PRs welcome.

Got an idea? [Start a conversation](https://github.com/obs-rust/obs-studio/discussions).

> [!TIP]
> **Come talk to us on [Discord](https://discord.gg/6MWWX4x9pf).** We're a
> new project, and the people building it are there. Ask a question, tell us
> what works or breaks on your setup (AppImage, Flatpak, your plug-ins and
> capture devices), suggest an idea, or follow the port as it lands. No
> question is too small.

# Features

What OBS-Studio-Rust does differently from upstream OBS Studio, for a user:
new features, changed behavior and fixed upstream bugs, by area, with links
to the issues and pull requests. See the
[features ledger](docs/features.md).

# Fixing bugs with AI + Rust

Want to fix one yourself? Contributors, human or AI-assisted, chat about
ports and pull requests on [Discord](https://discord.gg/6MWWX4x9pf).

https://github.com/user-attachments/assets/4d9af616-bd76-468e-93ac-ef8ef44a0efb




# Launch speed

4.3× faster to the main window and 19% less memory on an NVIDIA streaming
rig (#1168, #1170, #1171, #1172).

<p align="center"><img src="docs/images/launch-speed.png" alt="OBS-Rust launch speed: 4.09 s to 0.95 s, 307 to 250 MiB" width="600"></p>

# Contributors leaderboard

Ranked by merged PRs only, credited to the original author even when a
maintainer re-landed the work (counted once). Updated every 4 hours.

[![obs-rust contributors leaderboard](https://raw.githubusercontent.com/obs-rust/obs-studio/leaderboard/leaderboard.jpg)](https://obs-rust.github.io/obs-studio/)

[Open the live leaderboard](https://obs-rust.github.io/obs-studio/)


# How CI works here

Lots of people and AI agents work on this repo at the same time, so CI is
built to do as little repeated work as possible:

- **Every pull request gets a quick check.** It runs the Rust and C tests on
  Linux, plus macOS or Windows only when your change touches those
  platforms. It does not build every app package.
- **The full build runs before each release.** Once a day, and before
  anything is published, CI builds and tests everything: every platform,
  the full app, Flatpak and the release packages. A beta only ships if
  all of it passes.
- **You can prove your change locally first.** One command (`ci-lint
  local-gate run`) runs the same Linux checks on your machine and signs
  your commit. CI sees the signature and skips the checks you already
  ran.
- **Builds are cached and shared.** Rust builds go through `soldr`, which
  reuses compiled code across branches and machines.

The full guide, with the tools, rules and commands, is in
[docs/ci-tooling.md](docs/ci-tooling.md). If you're contributing, please
read it before you push: it explains how to avoid overloading the
shared machines and CI.

# Q&A

**Where can I get help, or report something that isn't quite a bug?**

Join us on [Discord](https://discord.gg/6MWWX4x9pf): it's the quickest way to
reach the people working on OBS Rust. For a reproducible bug, please also
[file an issue](https://github.com/obs-rust/obs-studio/issues/new/choose); for
longer ideas, [start a discussion](https://github.com/obs-rust/obs-studio/discussions).

**Does OBS Rust support all of the plug-ins?**

Yes. OBS Rust is fully backwards compatible with OBS Studio — the
plug-ins you already have should work as-is. If you find one that
doesn't, that's a bug: please [file an issue](https://github.com/obs-rust/obs-studio/issues/new/choose)
and link it here so we can track it.

**How do I install plug-ins for the AppImage?**

The AppImage loads plug-ins from the same per-user folders as OBS Studio on
Linux, so a plug-in you already installed for your own account is picked up
as-is. To add one, unpack its Linux build into either of these folders:

- `~/.local/share/obs-studio/plugins/<plugin>/`, with the module at
  `<plugin>/<plugin>.so` and its files under `<plugin>/data/` (the current
  layout).
- `~/.config/obs-studio/plugins/<plugin>/`, with the module at
  `<plugin>/bin/64bit/<plugin>.so` and its files under `<plugin>/data/` (the
  older layout that many plug-in downloads still use).

Then restart OBS Rust. **Tools → Plugin Manager** lists what loaded.

Plug-ins installed system-wide by a distribution package (under
`/usr/lib/obs-plugins` or `/usr/lib/<arch>/obs-plugins`) are **not** loaded.
They were built against that distribution's OBS and libraries, not the ones
inside the AppImage. Use the plug-in's own Linux download instead.

A plug-in with its own Qt windows must be built for a Qt 6 release no newer
than the one in the AppImage. If a plug-in sits in one of those folders but
doesn't load, run the AppImage from a terminal and the log will say why.
Then file an issue as above.

Want to see the original README from OBS Project? [Click here](README.upstream.md).

# 🏆 OBS Project User Abuse Hall of Fame

Bugs they confirmed, then closed. Features you relied on, then removed.
Twelve upstream threads where users showed up in numbers and were turned
away anyway. Six of them were closed as stale on a single afternoon.

| | Issue | 💬 | Fate |
|:-:|---|--:|---|
| 🥇 | [Preview Stutter that appears in Streams/Recordings](https://github.com/obsproject/obs-studio/issues/4191) | 184 | locked (off-topic) |
| 🥈 | [OBS Virtual Camera may affect built-in webcams](https://github.com/obsproject/obs-studio/issues/3555) | 87 | closed not planned, locked |
| 🥉 | [Removal of --disable-shutdown-check breaks automated background workflows](https://github.com/obsproject/obs-studio/issues/12650) | 64 | closed not planned, locked |
| 4 | [Dock Sizes are Reset the next time you open OBS.](https://github.com/obsproject/obs-studio/issues/2949) | 34 | closed not planned |
| 5 | [Pause and Resume Recording Doubles or Triples CPU usage](https://github.com/obsproject/obs-studio/issues/3148) | 34 | closed not planned |

**In their own words:**

> **“No amount of "I don't like your answer" will change it.”**
> — [#9285](https://github.com/obsproject/obs-studio/issues/9285#issuecomment-1677822114), to a user who said a downgrade fixed their dropped frames

> **“However, if you refuse to follow the process, there is nothing we can do, and this report is not actionable.”**
> — [#13685](https://github.com/obsproject/obs-studio/issues/13685#issuecomment-5052743359), in the same comment that admitted 32.2 had raised the driver minimum

> **“[…] that decision has already been made.”**
> — [#12650](https://github.com/obsproject/obs-studio/issues/12650#issuecomment-3368592277), on removing a flag users' setups depended on

**[See all 12, every quote, and why each one made the cut →](docs/hall-of-fame.md)**
