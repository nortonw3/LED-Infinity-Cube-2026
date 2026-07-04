# LED Infinity Cube — 3D Web Showcase — Design

**Date:** 2026-07-04
**Status:** Approved (design), pending spec review
**Scope:** Phase 1 — glowing-wireframe simulator. Infinity-mirror illusion is Phase 2 (separate spec).

## Purpose

A polished, shareable web page that shows off the LED Infinity Cube and its
animations in an interactive 3D scene. Primary goal is **showcase / share** —
looks matter most. Secondary benefit: previewing animations without flashing
hardware.

The firmware (this repo) is the source of truth for cube geometry, the edge
graph, the animation math, the palette system, and the audio-feature model.
The web app re-implements that logic faithfully in TypeScript.

## Decisions (locked)

| Question | Decision |
|---|---|
| Primary purpose | Showcase / share (looks matter most) |
| Audio source | System audio via `getDisplayMedia` (headline) + bundled demo track (instant-on) + synthetic fallback |
| Animation coverage | All modes: Static (8), Audio (10), Voice (4). Auto-cycle demo reel + manual select + palette control (parity with OLED menu) |
| Cube look | Glowing wireframe + bloom now; infinity-mirror illusion deferred to Phase 2 |
| Implementation approach | **A** — hand-port animations + audio engine to TypeScript, render with Three.js, ship as a static site |
| Location | `sim/` subfolder in this repo, isolated from the PlatformIO build |

### Why hand-port (A) over WASM (B) or baked frames (C)
- **A** ships as a plain static site (trivial to host and share — the point of a
  showcase), is clean and debuggable, and gives full control over the look.
- **B** (Emscripten/WASM) requires stubbing the entire Arduino + FastLED surface
  just to compile, adds toolchain weight, is harder to host/debug — and the audio
  engine must be JS regardless because it consumes the browser's FFT. Too much
  yak-shaving to reuse code that is already frozen.
- **C** (bake and replay frames) kills interactivity and live audio-reactivity —
  which is the entire point of the Audio/Voice modes and the system-audio feature.

## Tech stack

- **Vite + TypeScript + Three.js**
- `three/examples`: `OrbitControls`, `UnrealBloomPass`
- Web Audio API (`AnalyserNode` for FFT; `getDisplayMedia` for system audio)
- No backend. Static build → GitHub Pages / Netlify / Vercel.

## Module architecture

Each module has one job and a well-defined interface so it can be built and
tested independently.

- **`geometry.ts`** — direct port of `cube.h` `buildCubeGeometry()` +
  `buildEdgeGraph()`. Exports `voxels: {x,y,z}[384]` and `graphL/graphR`
  neighbor arrays. LED positions match the device exactly.
- **`fastled.ts`** — the small FastLED/Arduino compatibility shims used by the
  animations: `ColorFromPalette` (16-entry LINEARBLEND), `inoise8` (Perlin),
  `qadd8`, `random`, `millis`, `constrain`, plus math helpers. Everything else
  builds on this layer.
- **`palettes.ts`** — the 10 palettes from `palettes.h` + `applyPalette(intensity)`
  including the palette crossfade logic.
- **`audio/`**
  - `bus.ts` — the `AudioBus` type: `band[7]` (sub/bass/loMid/mid/hiMid/pres/bril),
    `flux[7]`, `level`, `centroid`, `beatPhase`, `barPhase`, `beatFired`, `bpm`,
    `tempoConfidence`, `speech`, `syllableOnset`, `sylEnv`, plus the named
    accessors (`sub()`..`brilliance()`).
  - `engine.ts` — port of `audio_engine.h`: FFT bins → 7 log-spaced perceptual
    bands → per-band room-noise AGC → derived features → beat/tempo (onset +
    autocorrelation + PLL beat phase). Produces one `AudioBus` per frame.
  - `sources.ts` — pluggable audio sources feeding a shared `AnalyserNode`:
    1. **System audio** — `getDisplayMedia({video:true,audio:true})`; use only the
       audio track (video track stopped). User selects "Entire Screen + share
       system audio" or a specific tab.
    2. **Bundled demo track** — a royalty-free track played on load with real FFT,
       so the page is alive instantly with zero setup.
    3. **Synthetic fallback** — code-generated musical signal (pulsing bass, beats,
       sweeps) when no real audio is available.
- **`animations/`** — the ~22 ported functions grouped `static/`, `audio/`,
  `voice/`, each matching the firmware `(buf, t)` signature. Per-animation
  persistent state (RD/Coral chemical fields, gravity particles, shockwaves,
  pulse-web rings, dendrite tips, flame heat, spark walkers) ported 1:1 into
  module scope.
- **`renderer.ts`** — Three.js scene. 384 instanced glowing points/quads on the
  edges, `UnrealBloomPass`, dark backdrop, `OrbitControls` + gentle auto-rotate
  (pauses while dragging).
- **`app.ts`** — controller. Owns the frame loop (45 FPS target, seconds-based `t`
  clock so speeds match the device), current mode/anim/palette/brightness,
  auto-cycle timer + crossfade (`startTransition` port), and wires the UI.

## Data flow (per frame)

```
audio source → AnalyserNode (FFT) → audio/engine → AudioBus
                                                       │
mode/anim selection ─────────────► animation(buf, t) ─┤ reads voxels[], graph, AudioBus
                                                       ▼
                              per-LED intensity/CRGB buffer
                                                       ▼
                        applyPalette + brightness → 384 colors
                                                       ▼
                         renderer (instanced points + bloom)
```

Static-mode animations ignore the `AudioBus`, exactly like the device.

## Animation port layer

- Signature `AnimFunc(buf: Float32Array, t: number)` writing 0..1 intensity per
  LED; color via `applyPalette`. Additive-blend animations (Lissajous, gravity,
  syllable sparks) use a `CRGB`-style path with `qadd8`, matching firmware.
- Ports are grouped and named to match `staticAnimNames` / `audioAnimNames` /
  `voiceAnimNames` (e.g. `DiagFlow`, `Lissajous`, `RD`, `Coral`, `MobiusBraid`,
  `Plasma`, `NoiseWorms`; `TriAxis`, `Impact`, `CellAuto`, `BassBloom`, `Vortex`,
  `PulseWeb`, `SpectrHlix`, `Earthquake`, `Flame`, `Dendrite`; `Breathe`,
  `Formant`, `Rings`, `Sparks`).
- **Verification per animation:** visual check against the behavior documented in
  the firmware source; confirmed before moving to the next.

## UI / showcase behavior

- **Auto-cycle "demo reel"** on by default: advances Mode→Anim on a timer with a
  smooth crossfade. Controls auto-hide during auto-cycle.
- **Manual controls** (overlay, dark, minimal): Mode (Static/Audio/Voice),
  Animation (by name), Palette, Brightness — parity with the OLED menu. Plus audio
  buttons: "Use system audio", "Demo track", mute.
- Mouse: orbit + zoom; auto-rotate pauses while dragging.

## Testing

- **Unit tests** (pure functions with known outputs): geometry/graph
  construction, `ColorFromPalette` interpolation, `inoise8` determinism,
  `qadd8`/`constrain`.
- **Audio engine**: feed known synthetic FFT frames; assert band values, level,
  and beat/tempo outputs.
- **Animations**: verified visually (artistic; no meaningful numeric assertion
  beyond "matches the device look").
- **Build gate**: `npm run build` produces a clean static bundle.

## Deployment

`npm run build` → static bundle → GitHub Pages (fits this repo), shareable by
link. Runs smoothly in Chrome/Edge on Windows (where system-audio capture is
best supported).

## What "done" looks like (Phase 1)

A hosted link where the cube auto-cycles through all modes/animations with bloom,
the viewer can orbit it, manually pick any mode/animation/palette/brightness, and
switch it to react to the computer's system audio — running smoothly in
Chrome/Edge.

## Out of scope (Phase 1)

- Infinity-mirror recursive-reflection illusion (Phase 2, separate spec).
- Editing/persisting animation parameters back to firmware (this is a showcase,
  not the tuning workbench).
- Mobile-first layout and non-Chromium browser audio parity (best-effort only;
  system audio targets Chrome/Edge on Windows).
- Any change to the firmware, `src/`, or `platformio.ini`.
