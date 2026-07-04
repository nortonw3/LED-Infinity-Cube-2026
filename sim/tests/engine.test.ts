import { describe, it, expect, beforeEach, vi } from 'vitest';
import {
  registerAnimations, selectAnimation, renderFrame, setBrightness,
  getState, setDemo, type AnimFunc,
} from '../src/engine';
import { newBuffer, type CRGB } from '../src/fastled';

const solid = (v: number): AnimFunc => (buf) => {
  for (const p of buf) { p.r = v; p.g = v; p.b = v; }
};

describe('engine', () => {
  beforeEach(() => {
    registerAnimations('static', [solid(100), solid(200)], ['A', 'B']);
    setBrightness(255);
    setDemo(false);
    selectAnimation('static', 0, true);   // immediate — no crossfade in tests
  });

  it('renders the current animation', () => {
    const out = newBuffer();
    renderFrame(out, 10.0);
    expect(out[0].r).toBe(100);
  });

  it('crossfade blends between animations mid-transition', () => {
    const out = newBuffer();
    selectAnimation('static', 1);         // starts 2 s wall-clock transition
    const state = getState();
    expect(state.index).toBe(1);
    // immediately after select, blend p≈0 → output near anim0's 100
    renderFrame(out, 100.0);
    expect(out[0].r).toBeGreaterThanOrEqual(100);
    expect(out[0].r).toBeLessThan(110);
  });

  it('brightness scales output', () => {
    const out = newBuffer();
    setBrightness(128);
    renderFrame(out, 500.0);
    // 100 * (128/255) ≈ 50
    expect(out[0].r).toBeGreaterThan(45);
    expect(out[0].r).toBeLessThan(56);
  });

  it('brightness is clamped to 5..255', () => {
    setBrightness(0);
    expect(getState().brightness).toBe(5);
    setBrightness(999);
    expect(getState().brightness).toBe(255);
  });

  it('demo toggle is reflected in state', () => {
    setDemo(true);
    expect(getState().demo).toBe(true);
    setDemo(false);
    expect(getState().demo).toBe(false);
  });

  it('demo cycles through the reel: stays in-range per mode and never repeats immediately', () => {
    // Register the sim's real animation counts (8 static + 10 audio + 4
    // voice = 22 demo entries) so any out-of-range demoList index would
    // actually throw / read undefined here.
    registerAnimations('static', Array.from({ length: 8 }, (_, i) => solid(i)),
      Array.from({ length: 8 }, (_, i) => `S${i}`));
    registerAnimations('audio', Array.from({ length: 10 }, (_, i) => solid(i)),
      Array.from({ length: 10 }, (_, i) => `A${i}`));
    registerAnimations('voice', Array.from({ length: 4 }, (_, i) => solid(i)),
      Array.from({ length: 4 }, (_, i) => `V${i}`));
    selectAnimation('static', 0, true);

    const countsByMode: Record<string, number> = { static: 8, audio: 10, voice: 4 };
    const DEMO_INTERVAL_MS = 30000;   // must match engine.ts's demo interval
    let mockTime = 10_000_000;
    vi.spyOn(performance, 'now').mockImplementation(() => mockTime);

    try {
      setDemo(true);
      const out = newBuffer();
      const seen: string[] = [];
      for (let cycle = 0; cycle < 8; cycle++) {
        mockTime += DEMO_INTERVAL_MS + 1;   // push millis() past the demo interval
        renderFrame(out, cycle);
        const state = getState();
        // in-range: this is the check that would catch an out-of-range demo entry
        expect(state.index).toBeGreaterThanOrEqual(0);
        expect(state.index).toBeLessThan(countsByMode[state.mode]);
        const key = `${state.mode}:${state.index}`;
        if (seen.length > 0) {
          expect(key).not.toBe(seen[seen.length - 1]);   // no-immediate-repeat guarantee
        }
        seen.push(key);
      }
      expect(new Set(seen).size).toBeGreaterThan(1);   // selection actually changes
    } finally {
      vi.restoreAllMocks();
      setDemo(false);
      selectAnimation('static', 0, true);   // re-establish known state for later tests
    }
  });
});
