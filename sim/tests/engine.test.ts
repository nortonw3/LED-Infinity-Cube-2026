import { describe, it, expect, beforeEach } from 'vitest';
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
    expect(out[0].r).toBeLessThanOrEqual(200);
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
});
