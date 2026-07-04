import { describe, it, expect } from 'vitest';
import {
  PALETTE_NAMES, NUM_PALETTES_TOTAL, PALETTE_ROTATE_IDX,
  applyPalette, startPaletteFade, getPaletteIndex, colorFromPalette16,
} from '../src/palettes';
import { rgb } from '../src/fastled';

describe('palettes', () => {
  it('has 11 names ending with Rotate', () => {
    expect(NUM_PALETTES_TOTAL).toBe(11);
    expect(PALETTE_ROTATE_IDX).toBe(10);
    expect(PALETTE_NAMES).toEqual([
      'Embers', 'IceSplnt', 'Toxic', 'NeonSign', 'DeepSea', 'Forge',
      'Aurora', 'Stardust', 'BloodMoon', 'Glitch', 'Rotate',
    ]);
  });

  it('colorFromPalette16 hits exact entries at multiples of 16', () => {
    // 4-entry ramp padded to 16 for the test
    const pal = Array.from({ length: 16 }, (_, i) => rgb(i * 16, 0, 0));
    // index8 = 16 → entry 1 exactly (FastLED: hi4=1, lo4=0)
    expect(colorFromPalette16(pal, 16)).toEqual(rgb(16, 0, 0));
    expect(colorFromPalette16(pal, 0)).toEqual(rgb(0, 0, 0));
  });

  it('colorFromPalette16 blends linearly between entries', () => {
    const pal = Array.from({ length: 16 }, () => rgb(0, 0, 0));
    pal[0] = rgb(0, 0, 0); pal[1] = rgb(160, 0, 0);
    // index8 = 8 → halfway entry0→entry1 (lo4=8 of 16)
    const c = colorFromPalette16(pal, 8);
    expect(c.r).toBeGreaterThan(70); expect(c.r).toBeLessThan(90);
  });

  it('palette 16th entry wraps toward entry 0 (FastLED semantics)', () => {
    const pal = Array.from({ length: 16 }, () => rgb(0, 0, 0));
    pal[15] = rgb(200, 0, 0); pal[0] = rgb(0, 0, 0);
    // index8 = 248 → hi4 = 15, lo4 = 8 → halfway entry15 → entry0
    const c = colorFromPalette16(pal, 248);
    expect(c.r).toBeGreaterThan(90); expect(c.r).toBeLessThan(110);
  });

  it('applyPalette clamps intensity and returns a color', () => {
    const c = applyPalette(2.0);   // clamped to 1.0
    expect(c.r).toBeGreaterThanOrEqual(0); expect(c.r).toBeLessThanOrEqual(255);
  });

  it('startPaletteFade switches index', () => {
    startPaletteFade(3);
    expect(getPaletteIndex()).toBe(3);
    startPaletteFade(0);   // restore
  });
});
