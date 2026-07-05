# LED-Infinity-Cube-202
work in progress

link to CAD files in Makerworld:
https://makerworld.com/en/models/2606835-led-infinity-cube#profileId-2876912

## Web Simulator (`sim/`)

**Live demo:** https://nortonw3.github.io/LED-Infinity-Cube-2026/

A browser-based 3D showcase that re-implements the firmware's geometry,
palettes, audio engine, and all 22 animations in TypeScript + Three.js. It
auto-cycles through every mode/animation, lets you pick any mode, animation,
palette, and brightness by hand, and can react to your computer's system
audio (Chrome/Edge on Windows) or a built-in synthetic/demo signal.

```bash
cd sim
npm install
npm run dev      # local dev server
npm run build    # static bundle in sim/dist
npm test         # unit tests (geometry, palettes, audio engine, engine)
```

**Deploy:** pushing changes under `sim/` to `main` builds and publishes the
site to GitHub Pages via `.github/workflows/deploy-sim.yml`. Enable it once
under **Settings → Pages → Build and deployment → Source: GitHub Actions**.

## Pinout (Teensy 4.1)

| Teensy Pin | Function | Connected To | Notes |
|---|---|---|---|
| 2 | LED data | WS2812B strip (12 edges x 32 LEDs) | `DATA_PIN`; 3.3V logic level, short wire runs work without a level shifter |
| 3 | Encoder CLK (A) | Rotary encoder | `ENC_CLK`, `INPUT_PULLUP`, interrupt on CHANGE |
| 4 | Encoder DT (B) | Rotary encoder | `ENC_DT`, `INPUT_PULLUP`, interrupt on CHANGE |
| 5 | Encoder SW | Rotary encoder push button | `ENC_SW`, `INPUT_PULLUP`, debounced in software |
| 7 | I2S TX | I2S mic breakout | Reserved by the I2S peripheral; typically unused on a mic-only (input) breakout |
| 8 | I2S RX (data/SD) | I2S mic breakout | Audio data line into `i2s1` |
| 9 | Ethernet RST | W5500 module | `ARTNET_RST_PIN` — Art-Net is currently disabled in firmware |
| 10 | Ethernet CS | W5500 module | `ARTNET_CS_PIN` — Art-Net is currently disabled in firmware |
| 11 | SPI MOSI | W5500 module | Hardware SPI |
| 12 | SPI MISO | W5500 module | Hardware SPI |
| 13 | SPI SCK / onboard LED | W5500 module + Teensy built-in LED | Shared pin; LED also blinks on hardfault |
| 20 | I2S LRCLK (WS) | I2S mic breakout | Word/channel select clock |
| 21 | I2S BCLK | I2S mic breakout | Bit clock |
| 23 | I2S MCLK | I2S mic breakout | Reserved by the I2S peripheral; most MEMS mics don't need it wired |
| 24 | I2C2 SDA | OLED SSD1306 (addr `0x3C`) | `Wire2`, 400kHz |
| 25 | I2C2 SCL | OLED SSD1306 (addr `0x3C`) | `Wire2`, 400kHz |
| 5V / GND | LED strip power | Dedicated 5V supply | Not powered from the Teensy — `POWER_LIMIT_MA` is set for 15A; share ground with the Teensy |
| 3.3V / GND | Logic power | Encoder, OLED, I2S mic breakout | Teensy 3.3V rail |

Notes:
- The W5500/Ethernet (Art-Net) wiring is present in the pinout but the feature is currently disabled in firmware (`artnet.h` is not compiled in).
- I2S mic breakout pins assume a bare I2S MEMS microphone (e.g. INMP441/SPH0645) — check your specific module's L/R channel-select pin and tie it to GND or 3.3V as required.
