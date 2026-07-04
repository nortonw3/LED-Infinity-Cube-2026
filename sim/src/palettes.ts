// Port of src/palettes.h — 10 hand-tuned 16-entry palettes + Rotate mode.
import { CRGB, rgb, clamp, blend, millis } from './fastled';

const B = () => rgb(0, 0, 0);
const W = () => rgb(255, 255, 255);

export const PALETTES: CRGB[][] = [
  // 0 — Embers
  [B(), B(), B(), B(), rgb(60,5,0), rgb(160,20,0), rgb(255,69,0), rgb(255,80,0),
   B(), B(), B(), rgb(40,0,0), rgb(120,10,0), rgb(255,0,0), rgb(255,60,0), B()],
  // 1 — Ice Splinter
  [B(), B(), B(), B(), B(), rgb(0,60,80), rgb(0,160,200), W(),
   B(), B(), B(), B(), rgb(0,40,60), rgb(0,120,180), rgb(180,240,255), B()],
  // 2 — Toxic
  [B(), B(), B(), rgb(10,30,0), rgb(30,80,0), rgb(80,160,0), rgb(160,220,0), rgb(220,255,40),
   B(), B(), rgb(0,20,0), rgb(20,60,0), rgb(60,130,0), rgb(140,200,0), rgb(200,255,20), W()],
  // 3 — Neon Sign
  [B(), B(), B(), B(), rgb(120,0,80), rgb(255,0,255), B(), B(),
   B(), B(), rgb(0,80,120), rgb(0,200,255), B(), B(), B(), B()],
  // 4 — Deep Sea
  [B(), rgb(0,10,15), rgb(0,20,30), rgb(0,35,50), rgb(0,55,70), rgb(0,80,90), B(), B(),
   rgb(0,15,20), rgb(0,30,45), rgb(0,50,65), B(), B(), rgb(0,100,140), rgb(0,180,220), rgb(100,230,255)],
  // 5 — Forge
  [B(), B(), B(), B(), B(), rgb(80,20,0), rgb(180,60,0), rgb(255,120,0),
   rgb(255,200,80), W(), B(), B(), B(), B(), B(), B()],
  // 6 — Aurora
  [B(), B(), rgb(0,20,10), rgb(0,60,20), rgb(0,120,40), rgb(0,180,60), B(), B(),
   B(), rgb(20,0,40), rgb(60,0,100), rgb(120,0,180), rgb(160,40,220), B(), B(), B()],
  // 7 — Stardust
  [B(), B(), B(), rgb(40,30,10), B(), B(), rgb(10,10,60), rgb(80,80,200),
   B(), rgb(60,40,10), rgb(180,140,40), B(), B(), B(), rgb(20,20,80), rgb(200,200,255)],
  // 8 — Blood Moon
  [B(), B(), rgb(20,0,0), rgb(50,0,0), rgb(100,0,0), B(), B(), B(),
   B(), rgb(30,0,0), rgb(80,0,0), rgb(160,10,0), rgb(220,40,0), rgb(255,80,0), B(), B()],
  // 9 — Glitch
  [B(), B(), B(), B(), B(), B(), W(), B(),
   B(), B(), B(), rgb(0,255,255), B(), B(), rgb(255,0,255), B()],
];

export const PALETTE_NAMES = [
  'Embers', 'IceSplnt', 'Toxic', 'NeonSign', 'DeepSea', 'Forge',
  'Aurora', 'Stardust', 'BloodMoon', 'Glitch', 'Rotate',
];
export const NUM_PALETTES_TOTAL = 11;
export const PALETTE_ROTATE_IDX = 10;

// Rotate-mode timing — palettes.h:92-93
const PALETTE_ROTATE_INTERVAL_MS = 30000;
const PALETTE_ROTATE_FADE_MS = 6000;

// ── FastLED ColorFromPalette(pal, index8, 255, LINEARBLEND) ────────
// hi4 selects the entry, lo4 blends 1/16-steps toward the next entry
// (wrapping 15 → 0).
export function colorFromPalette16(pal: CRGB[], index8: number): CRGB {
  const i8 = clamp(Math.round(index8), 0, 255);
  const hi4 = i8 >> 4, lo4 = i8 & 0x0f;
  const a = pal[hi4], b = pal[(hi4 + 1) % 16];
  const f = lo4 / 16;
  return {
    r: Math.round(a.r + (b.r - a.r) * f),
    g: Math.round(a.g + (b.g - a.g) * f),
    b: Math.round(a.b + (b.b - a.b) * f),
  };
}

// ── Palette state — palettes.h:104-109 ─────────────────────────────
let currentPaletteIndex = 0;
let currentPalette: CRGB[] = PALETTES[0];
let previousPalette: CRGB[] = PALETTES[0];
let paletteFadeStart = 0;
const paletteFadeDuration = 2.0;
let paletteFading = false;

export function getPaletteIndex(): number { return currentPaletteIndex; }

// Port of palettes.h:115-121
export function startPaletteFade(newIndex: number): void {
  previousPalette = currentPalette;
  currentPaletteIndex = newIndex;
  currentPalette = PALETTES[newIndex] ?? PALETTES[0];  // Rotate has no fixed palette
  paletteFadeStart = millis() * 0.001;
  paletteFading = true;
}

// Port of palettes.h:123-164
export function applyPalette(intensity: number): CRGB {
  const v = clamp(intensity, 0, 1);

  // Rotate mode: 10 palettes × (30 s display + 6 s crossfade)
  if (currentPaletteIndex === PALETTE_ROTATE_IDX) {
    const slotMs = PALETTE_ROTATE_INTERVAL_MS + PALETTE_ROTATE_FADE_MS;
    const totalMs = slotMs * 10;
    const pos = millis() % totalMs;
    const palIdx = Math.floor(pos / slotMs);
    const phaseMs = pos % slotMs;
    const nextIdx = (palIdx + 1) % 10;
    const a = colorFromPalette16(PALETTES[palIdx], v * 255);
    if (phaseMs >= PALETTE_ROTATE_INTERVAL_MS) {
      const bcol = colorFromPalette16(PALETTES[nextIdx], v * 255);
      const blend8 = Math.floor(((phaseMs - PALETTE_ROTATE_INTERVAL_MS) * 255) / PALETTE_ROTATE_FADE_MS);
      return blend(a, bcol, blend8);
    }
    return a;
  }

  // Normal mode
  if (!paletteFading) return colorFromPalette16(currentPalette, v * 255);

  let p = (millis() * 0.001 - paletteFadeStart) / paletteFadeDuration;
  if (p >= 1.0) { paletteFading = false; p = 1.0; }
  const a = colorFromPalette16(previousPalette, v * 255);
  const b = colorFromPalette16(currentPalette, v * 255);
  return blend(a, b, Math.floor(p * 255));
}
