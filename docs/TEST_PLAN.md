# v0.3 acceptance test plan

Run this plan independently on Windows x64 and macOS Apple Silicon. Repeat the core playback cases with OBS at 1080p50.

## 1. Installation and loading

- Install the produced package.
- Start OBS and inspect **Help → Log Files → View Current Log**.
- Confirm `secretariatpro-multicam-replay` loads without errors.
- Confirm **Docks → SecretariatPro Replay** exists.
- Restart OBS and confirm saved settings are restored.

## 2. Source selection

- Create at least three different camera or media sources.
- Press **Actualizar lista de fuentes**, add CAM 3 and assign all three.
- Confirm selecting the same source twice is rejected.
- Remove CAM 3, add it again and confirm its row and programme button update correctly.
- Rename a source, refresh the list and confirm selection remains tied to its UUID.

## 3. Hotkeys

- Open **Settings → Hotkeys** and confirm all six `SecretariatPro Replay` actions are listed.
- Assign different shortcuts to buffer toggle, mark, replay, direct, next camera and previous camera.
- Close and reopen OBS and confirm every assignment persists.
- Operate a complete replay without focusing the replay dock.

## 4. ISO buffers

- Use x264 first, a 30-second buffer and 12,000 kbps.
- Activate the buffer and confirm every output starts in the OBS log.
- Leave it armed for five minutes while streaming or recording.
- Confirm no dropped rendering frames attributable to the plugin.
- Repeat with the preferred hardware H.264 encoder.

Pass criterion: all buffers remain active and programme output is unaffected.

## 5. Mark and media preparation

- Show a visible clap or flash to all cameras.
- Wait at least 12 seconds and mark a 10-second replay.
- Confirm one MKV file per camera is created and `Repetición preparada` appears.
- Open all files externally and confirm the clap occurs at substantially the same point.

Pass criterion: the maximum difference between cameras is below two OBS frames before manual latency calibration is added.

## 6. Quick replay

- Leave the timeline empty and press **LANZAR REPLAY**.
- Confirm OBS creates and selects `SP Replay`.
- Confirm CAM 1 plays the configured window.
- Confirm OBS returns to the previous live scene at the end.
- Repeat and press **DIRECTO** halfway through.

## 7. Full-canvas rendering

- Use a 1280×720 source with a 1920×1080 OBS canvas and confirm the replay fills the canvas exactly.
- Repeat with a 4:3 source and confirm proportional centre cropping without distortion.
- Repeat with a portrait source and confirm the centre fills the canvas without an image remaining in a corner.
- Change the OBS base canvas to 1280×720, relaunch a replay and confirm the output follows the new canvas.

## 8. Multi-shot sequence

For a 10-second replay window, add CAM 1 for 4 seconds at 100%, CAM 3 for 3 seconds at 50%, and CAM 2 for 3 seconds at 100%.

Confirm shot order, source-time continuity, slow-motion duration and automatic return. Inspect every segment boundary for a black frame, decoder stall or timestamp jump.

## 9. Live camera cut

- Launch a 10-second single-shot replay.
- Alternate CAM 1, CAM 2 and CAM 3 several times during playback.
- Confirm each cut remains on the same moment of the play, within two frames.

## 10. Stress and recovery

- Mark ten events in succession, waiting for `prepared` between marks.
- Attempt a second mark while the first is saving; confirm it is rejected cleanly.
- Leave one camera row without a source while buffers are stopped; confirm activation reports a clear error.
- Add cameras progressively and record CPU/GPU load to establish the practical hardware ceiling.
- Close OBS while buffers are active; confirm clean shutdown.
- Change scene collections and repeat source assignment.

## Test evidence to retain

- OBS log from each platform.
- CPU/GPU load with no plugin, x264 buffers and hardware buffers.
- Two saved angle files from the sync test.
- Screen recording of quick replay, multi-shot replay and live angle cuts.
- Exact OBS version, OS version, capture devices, encoder and input format.
