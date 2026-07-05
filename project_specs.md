# LED Infinity Cube — Project Spec

## What it does / who uses it
Firmware for a physical LED "infinity cube" sculpture. It drives a 12-edge, 32-LED-per-edge
WS2812B strip (384 LEDs total) mapped onto 3D cube-edge geometry, with animations that react
to music (via onboard mic + FFT/beat detection) or run as standalone static patterns. Controlled
by a rotary encoder + OLED menu. Built and used by Will for a personal art/hobby project (see
CAD files: https://makerworld.com/en/models/2606835-led-infinity-cube#profileId-2876912).

## Tech stack
- **MCU**: Teensy 4.1 (Arduino framework, PlatformIO project)
- **Language**: C++ (Arduino .ino + headers)
- **Key libraries**: FastLED (LED driving/color), Audio (Teensy Audio Library, I2S mic input/FFT),
  Adafruit_GFX + Adafruit_SSD1306 (OLED menu), EEPROM (settings persistence), FreeStack, Wire, SPI
- **Build tool**: PlatformIO (`platformio.ini`, env `LED Cube 7/2/26`, board `teensy41`)

## Hardware / data model
- `NUM_EDGES=12`, `LEDS_PER_EDGE=32`, `NUM_LEDS=384` — `Voxel{x,y,z}` array maps each LED to a
  3D position on the cube edges (`buildCubeGeometry()` in `cube.h`)
- **Edge graph** (`cube.h`): `rdNeighborL/R[]` built by `buildRDNeighbors()` — a vertex-welded
  1D topology so any scalar field flows continuously along edges AND around corners. This is
  the "native to the edges" foundation; exposed via `graphL/graphR`, `graphBlur/graphDiffuse`,
  plus spatial helpers (`diagonalOf`, `heightOf`, `radiusXY`, `angleZ`, `distToCorner`, …).
- `Mode` enum: `STATIC_MODE, AUDIO_MODE, VOICE_MODE, ARTNET_MODE`. Animation sets:
  `NUM_STATIC_ANIMS=8`, `NUM_AUDIO_ANIMS=10`, `NUM_VOICE_ANIMS=4` (arrays sized 10, extra slots
  are placeholders). Emergent/graph-native additions: `Coral` (2nd reaction-diffusion, static),
  `Flame` (graph heat-diffusion) and `Dendrite` (forking graph lightning) in audio.
- **Audio engine** (`audio_engine.h`): I2S mic → FFT1024 (`fftBins[]`) → 7 log-spaced perceptual
  bands → per-band room-noise AGC (floor/ceiling followers, gated) → derived features (`flux`,
  `level`, `centroid`, `speech`/`syllableOnset`) → beat/tempo (multi-band onset, autocorrelation
  tempo estimate, PLL beat phase). Publishes one `AudioBus audio` struct that all animations
  read (`audio.sub()..brilliance()`, `audio.beatPhase/beatFired/bpm/tempoConfidence`, etc.).
  Three global knobs shape everything: `reactivity`, `beatSensitivity`, `bandTilt`.
- Palette system: `CRGBPalette16` with palette rotate + crossfade; animation crossfade via
  `currentAnim`/`nextAnim` (both preserved from the original).
- UI: rotary encoder + OLED menu (Mode / Anim / Palette / Bright / Audio submenu with the 3
  global knobs). Serial: `react/beatsens/tilt`, `status/save/mode/demo`, `audio` (live band +
  beat monitor).
- Settings persist to EEPROM with a **version byte** (`EEPROM_VERSION`); mismatch loads defaults.

## Third-party services
None — fully standalone embedded device. (Art-Net/Ethernet support exists in `artnet.h` but is
disabled/not compiled in.)

## Build
- Env: `[env:led_cube]` in `platformio.ini` (renamed from an invalid name with spaces/slashes).
- `lib_deps`: FastLED, Adafruit GFX Library, Adafruit SSD1306 (Audio/EEPROM/SPI/Wire/SdFat come
  from the Teensy core). Build with `pio run -e led_cube`.

## Source layout (post-rebuild)
- `src/config.h` — pins + build constants + animation counts
- `src/globals.h` — shared types + externs (slimmed)
- `src/cube.h` — geometry, edge graph, spatial/diffusion toolkit
- `src/audio_engine.h` — bands + room AGC + beat/tempo + `AudioBus` + serial monitor
- `src/palettes.h` — color palettes (unchanged) + palette rotate/crossfade
- `src/animations.h` — curated animations, all on the bus + toolkit
- `src/LEDCube3-30-26.ino` — wiring: EEPROM, OLED menu, crossfade, setup/loop
- (`src/beatdetect.h` removed — superseded by the audio engine)

## What "done" looks like for this task
Rebuild is code-complete and compiles clean (`pio run -e led_cube` → SUCCESS). Remaining
verification is on-hardware (owner is currently away from the device): confirm band AGC adapts
to room level, BPM locks on 90/120/140-BPM tracks, and every animation flows across corners.
Tuning is done live via the `audio` serial monitor and the 3 OLED knobs.
