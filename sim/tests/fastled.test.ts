import { describe, it, expect } from 'vitest';
import { clamp, qadd8, blend, inoise8, randomInt, newBuffer, rgb } from '../src/fastled';

describe('fastled shims', () => {
  it('clamp', () => {
    expect(clamp(5, 0, 1)).toBe(1);
    expect(clamp(-5, 0, 1)).toBe(0);
    expect(clamp(0.5, 0, 1)).toBe(0.5);
  });

  it('qadd8 saturates at 255', () => {
    expect(qadd8(200, 100)).toBe(255);
    expect(qadd8(10, 20)).toBe(30);
  });

  it('blend interpolates 0..255', () => {
    const a = rgb(0, 0, 0), b = rgb(255, 255, 255);
    expect(blend(a, b, 0)).toEqual(rgb(0, 0, 0));
    expect(blend(a, b, 255).r).toBeGreaterThanOrEqual(254);
    const mid = blend(a, b, 128);
    expect(mid.r).toBeGreaterThan(120); expect(mid.r).toBeLessThan(136);
  });

  it('inoise8 is deterministic, in range, and smooth', () => {
    const a = inoise8(1000, 2000, 3000);
    expect(inoise8(1000, 2000, 3000)).toBe(a);
    expect(a).toBeGreaterThanOrEqual(0); expect(a).toBeLessThanOrEqual(255);
    const c = inoise8(1001, 2000, 3000);
    expect(Math.abs(a - c)).toBeLessThan(40);   // smooth: near samples are close
  });

  it('inoise8 varies across space', () => {
    const vals = new Set<number>();
    for (let i = 0; i < 50; i++) vals.add(inoise8(i * 37, i * 91, i * 53));
    expect(vals.size).toBeGreaterThan(10);
  });

  it('randomInt in range', () => {
    for (let i = 0; i < 100; i++) {
      const v = randomInt(6);
      expect(v).toBeGreaterThanOrEqual(0); expect(v).toBeLessThan(6);
    }
  });

  it('newBuffer is 384 black pixels', () => {
    const buf = newBuffer();
    expect(buf.length).toBe(384);
    expect(buf[100]).toEqual(rgb(0, 0, 0));
  });
});
