# SecretariatPro Multicam Replay for OBS

Native C++/Qt plugin for simple multicamera sports replays inside OBS Studio.

Repository: <https://github.com/agarciarenones-source/SecretariatPro-Multicam-Replay>

Version `0.4.0` connects the native OBS replay engine to SecretariatPro and keeps the canvas-aware rendering introduced in v0.3. It creates an independent ISO replay buffer for every configured source, saves all angles from the same event, builds a short cut sequence, supports 100% and 50% playback, lets the operator change angle while the replay is on air, and returns automatically to the previous live scene.

## What is included

- Two or more simultaneous camera sources selected from the current OBS source collection.
- Dynamic **Add camera** and **Remove last** controls with no software camera-count ceiling.
- Independent H.264 ISO encoders and OBS `replay_buffer` outputs.
- Configurable 10–60 second rolling buffer.
- Configurable replay window and per-camera bitrate.
- Synchronized event capture from every configured camera.
- Sequence of up to six consecutive shots.
- Per-shot speed at 100% or 50%.
- Live angle cut between any configured cameras while replay is playing.
- Automatic `SP Replay` scene and `SP Replay Output` source creation.
- Automatic return to the scene that was live before the replay.
- Persistent settings.
- Configurable OBS hotkeys for buffer, mark, replay, return to live and previous/next camera.
- Automatic full-canvas replay rendering for any input resolution or aspect ratio.
- Automatic SecretariatPro bridge for buffer controls, event marks, saved multicamera paths and replay status.
- Spanish operator interface.
- Compact top programme bar and scrollable dock that can be tabbed with other OBS docks.
- Windows x64 and macOS universal build definitions.

## Recommended production settings

- OBS canvas and output: 1920×1080 at 50 fps.
- Camera inputs: 1080p50.
- Buffer: 30 seconds.
- Replay window: 10 seconds.
- Bitrate: 12,000 kbps per camera.
- Encoder: a hardware H.264 encoder when the machine offers one; `x264` is the universal fallback.

Every camera uses its own H.264 encoder and replay-buffer output. The plugin does not impose an arbitrary maximum, but the practical limit is the available GPU/CPU encoder capacity, memory bandwidth and capture hardware. Add cameras progressively and watch OBS rendering/encoding statistics.

At 50 fps, 50% playback is suitable for a first sports replay implementation. A genuinely fluid 25% replay requires 100/120 fps acquisition or frame interpolation and is deliberately outside this release.

## Operator workflow

1. Add the camera feeds to OBS as normal video sources.
2. Open **Docks → SecretariatPro Replay**.
3. Open **Settings → Hotkeys** and search for `SecretariatPro Replay` to assign the optional keyboard controls.
4. Keep the initial CAM 1 and CAM 2 rows or press **+ AÑADIR CÁMARA** for additional angles. Assign a different source to every row.
5. Select an H.264 encoder and press **ACTIVAR BÚFER**.
6. After the relevant play, press **MARCAR**.
7. Wait for `Repetición preparada`.
8. Either press **LANZAR REPLAY** in the top bar to use the complete window from CAM 1, or select a camera, duration and speed and add up to six shots.
9. While replay is on air, the CAM buttons or previous/next camera hotkeys perform a synchronized angle change.
10. The plugin returns to the previous scene when the sequence ends. **DIRECTO** performs an immediate manual return.

Sequence shots consume the replay window from oldest to newest. For example, with a 10-second window, adding CAM 1 for 4 seconds, CAM 3 for 3 seconds and CAM 2 for 3 seconds creates a complete three-shot replay.

The panel is registered through the official OBS dock API. Drag its title bar over another OBS dock until the tab target appears, then release it. OBS remembers the resulting tabbed layout.

Replay video uses a centred **cover** fit: it always fills the OBS base canvas without distortion. Sources with a different aspect ratio are proportionally enlarged and cropped equally at the opposing edges.

## Build

This project follows the official OBS plugin template and pins the corresponding OBS development dependencies in `buildspec.json`.

### Windows x64

Requirements: Visual Studio 2022 with C++ desktop tools, CMake 3.30.5 or compatible, PowerShell 7 and Git.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --prefix release/RelWithDebInfo --config RelWithDebInfo
```

### macOS universal

Requirements: macOS 12 or later, Xcode 16, CMake 3.30.5 or compatible and Git.

```bash
cmake --preset macos
cmake --build --preset macos --config RelWithDebInfo
cmake --install build_macos --prefix release/RelWithDebInfo --config RelWithDebInfo
```

The macOS package must be signed and notarised before public distribution. Unsigned local development builds can be loaded only under the normal macOS development security constraints.

### Automated builds

The included GitHub Actions workflow builds a Windows x64 ZIP and a macOS universal PKG. This is the simplest route when you need both operating systems:

1. Create an empty GitHub repository and upload the complete project, including the `.github` directory. GitHub Desktop is the easiest way to preserve that directory.
2. Create and push a semantic beta tag such as `0.4.0-beta2`.
3. Wait for both **Build for macOS** and **Build for Windows** to finish with a green check.
4. Open the draft version under **Releases** and download the Windows and macOS installers.

Manual builds are intentionally unsigned beta packages. The Windows artifact contains the normal OBS plugin directory; extract it into `%APPDATA%\obs-studio\plugins`. On macOS, open the PKG and, if Gatekeeper blocks this private beta, allow it from **System Settings → Privacy & Security**. Public distribution requires an Apple Developer signature and notarisation.

A semantic version tag such as `0.3.0-beta1` creates a draft release.

### First use in OBS

1. Close OBS before installing the plugin, then reopen it after installation.
2. Add at least two camera inputs to OBS and verify that they show live video.
3. Open **Docks → SecretariatPro Replay**.
4. Open **Settings → Hotkeys**, search for `SecretariatPro Replay` and assign the shortcuts you want.
5. Add the required camera rows, choose a different source in each, set a 30-second buffer and a 10-second replay window, and select a hardware H.264 encoder when available.
6. Press **ACTIVAR BÚFER** and let it fill for at least the replay-window duration.
7. After the action, press **MARCAR** and wait for `Repetición preparada`.
8. Add the desired camera shots and their speed, then press **LANZAR REPLAY**.
9. Use any CAM button or the previous/next camera hotkeys during playback; use **DIRECTO** to end it immediately.

### Core test

The timeline engine has no OBS or Qt dependency:

```bash
bash ./scripts/test-core.sh
```

## SecretariatPro integration

When OBS loads the plugin, it writes `secretariatpro-bridge.json` atomically in its plugin configuration directory. SecretariatPro discovers this file and controls the registered OBS hotkeys through the standard OBS WebSocket API. A marked play is returned to SecretariatPro with the path of every saved camera angle; the app registers the complete event in its replay library and can use the first angle in the highlights export.

OBS WebSocket must be enabled and connected in SecretariatPro. No extra network port or third-party service is used by the bridge.

## Current v0.4 limitations

- The generated MKV clips remain in the plugin configuration directory. Automatic retention and purge policy are planned next.
- The saved files contain programme audio, but replay playback is muted in this release. The previous live scene remains nested beneath the replay picture so its commentary and ambience continue.
- Synchronisation uses OBS's common video clock. It is appropriate for ordinary USB/HDMI capture workflows, but it does not replace camera genlock.
- Camera-specific latency calibration is not yet exposed in the UI.
- Segment changes that also change speed reopen and seek the OBS media decoder. The technical test plan explicitly checks this boundary for visible stalls.
- There are no thumbnail previews or jog/shuttle controls yet.

## Project structure

- `src/iso-capture.*`: independent OBS view, encoder and replay buffer for each camera.
- `src/replay-engine.*`: event saving, media playback, scene switching and state machine.
- `src/replay-timeline.*`: platform-independent six-shot timeline model.
- `src/replay-output-source.*`: native OBS source rendered inside the replay scene.
- `src/replay-hotkeys.*`: native, user-configurable OBS hotkey registration and persistence.
- `src/replay-layout.*`: platform-independent full-canvas cover calculation.
- `src/replay-dock.*`: Qt operator dock.
- `docs/ARCHITECTURE.md`: design and threading notes.
- `docs/TEST_PLAN.md`: target-machine acceptance procedure.

## Licence

GPL-2.0, matching the licence inherited from the official OBS plugin template.
