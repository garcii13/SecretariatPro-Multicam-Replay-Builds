# Changelog

## 0.4.0 — SecretariatPro bridge

- Added an atomic JSON state bridge so SecretariatPro can detect the plugin, follow multicamera saves and register every saved angle.
- Exposed buffer, event, playback, camera and error state while keeping control on OBS WebSocket's standard hotkey API.
- Kept centred cover rendering for lower-resolution and non-16:9 cameras, with the existing layout regression tests.

## 0.3.0 — hotkeys and full-canvas replay

- Added native, configurable OBS hotkeys for buffer toggle, mark, launch replay, return to live and previous/next camera.
- Persisted hotkey assignments in the plugin configuration directory.
- Changed replay output dimensions to follow the active OBS base canvas.
- Added proportional centred cover scaling so lower-resolution and non-16:9 cameras fill the programme canvas.
- Added layout tests for 720p, 4:3, portrait and unavailable-source cases.

## 0.2.1 — upload-safe GitHub workflows

- Run macOS and Linux build/package scripts explicitly through Zsh, so GitHub uploads do not depend on executable file permissions.
- Run format checks directly through their shared Zsh implementation, avoiding both executable-bit and symbolic-link issues.

## 0.2.0 — dynamic multicamera dock

- Added dynamic camera rows with a two-camera minimum and no fixed software maximum.
- Converted ISO capture, synchronized saving, private media playback and live angle switching to dynamic collections.
- Migrated existing CAM 1/CAM 2 settings automatically to the new camera array format.
- Moved **LANZAR REPLAY** beside the buffer controls in a permanent top programme bar.
- Added a scrollable, compact OBS dock and scalable camera selectors/buttons.
- Documented how to tab the replay dock with other OBS panels.

## 0.1.1 — cross-platform build fix

- Fixed the Windows OBS logging declaration conflict.
- Fixed strict Apple Clang initialization warnings in replay scene layout.
- Manual GitHub Actions builds now create downloadable Windows and macOS packages.

## 0.1.0 — engineering build

- Added native OBS dock and replay output source.
- Added two independent ISO replay buffers.
- Added selectable H.264 encoder and per-camera bitrate.
- Added synchronized dual-angle event saving.
- Added six-shot timeline with 100% and 50% playback.
- Added live camera changes during replay.
- Added automatic replay scene creation and return to live.
- Added persistent configuration and Spanish operator UI.
- Added cross-platform Windows/macOS build definitions and timeline tests.
