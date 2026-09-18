# Architecture

## Capture path

Each selected camera is assigned to its own `obs_view_t`. `obs_view_add` attaches that view to the OBS render loop using the current canvas dimensions and frame rate. The view is connected to a dedicated H.264 video encoder and AAC audio encoder. Those encoders feed a separate OBS `replay_buffer` output.

This approach is source-agnostic. A camera can be a capture card, USB video source, NDI source, scene, or another OBS source with video output. It also avoids retaining uncompressed 1080p50 frames in memory.

All configured buffers are started together and use the same OBS video clock. A mark operation calls the `save` procedure on every output. Each buffer emits `saved`; the engine waits for every file path before declaring the event ready.

## Playback path

The saved MKV files are opened as private `ffmpeg_source` instances. All are active, muted and controlled by the replay engine. `SP Replay Output` is a custom video source that renders whichever private media source is currently selected.

The output source always reports the active OBS base-canvas dimensions. Its renderer calculates a centred proportional cover rectangle, applies translation and scale through the OBS graphics matrix, and renders the selected private source. Equal-aspect inputs scale exactly; different-aspect inputs fill the canvas with symmetric edge cropping rather than distortion or letterboxing.

The plugin creates an `SP Replay` scene on first use, places the previous live scene underneath it to preserve live programme audio, adds `SP Replay Output` above it, and stretches the replay picture to the current OBS base canvas. The engine switches to `SP Replay`, plays the sequence and restores the retained live scene at the end or when the operator presses OUT.

## Timeline model

A timeline is a maximum of six `Segment` values:

```text
camera index, source in, source out, speed percent
```

Segments consume source time consecutively from the beginning of the configured replay window. Playback duration is calculated as `(out - in) × 100 / speedPercent`. Only 100% and 50% are valid in v0.3.

## Threading

- OBS owns rendering and encoder threads.
- Replay-buffer `saved` callbacks may arrive outside the Qt UI thread.
- `IsoCapture` forwards saved paths to `ReplayEngine` through a queued Qt invocation.
- UI, scene switching and media controls run on the Qt UI thread.
- The custom replay source takes a short-lived OBS reference under a mutex before rendering, so media replacement cannot invalidate an in-flight render pointer.

## Hotkeys

Six frontend hotkeys are registered with stable OBS identifiers: buffer toggle, mark, take, out, next camera and previous camera. Callbacks are queued onto the Qt UI thread before touching the replay engine. Bindings are saved in the plugin configuration directory and restored when the dock is created.

## Resource ownership

- Each `IsoCapture` owns one view, one video encoder, one audio encoder and one replay-buffer output.
- The replay engine owns one private media source per configured camera and balances their showing/active references.
- The normal OBS source/scene collection owns `SP Replay` and `SP Replay Output` after creation.
- The previous live scene is retained only while replay is in programme.

## Failure behaviour

- Buffer activation is atomic from the operator's perspective: failure of any camera stops all of them.
- A mark is rejected while a previous mark is still being written.
- An event becomes ready only after every angle can be opened and reports a positive duration.
- A missing encoder, missing `obs-ffmpeg` replay output, duplicate camera selection, invalid scene name or failed media open is shown in the dock and OBS log.
