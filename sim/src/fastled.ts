// Minimal FastLED/Arduino compatibility layer — only the surface the
// firmware animations actually use.

import { NUM_LEDS } from './geometry';

export interface CRGB { r: number; g: number; b: number; }

export function rgb(r: number, g: number, b: number): CRGB { return { r, g, b }; }

export function clamp(x: number, lo: number, hi: number): number {
  return x < lo ? lo : x > hi ? hi : x;
}

export function qadd8(a: number, b: number): number {
  const s = a + b;
  return s > 255 ? 255 : s;
}

// FastLED blend(a, b, amount): amount 0..255 blends from a to b.
export function blend(a: CRGB, b: CRGB, amount: number): CRGB {
  const f = clamp(amount, 0, 255) / 255;
  return {
    r: Math.round(a.r + (b.r - a.r) * f),
    g: Math.round(a.g + (b.g - a.g) * f),
    b: Math.round(a.b + (b.b - a.b) * f),
  };
}

export function randomInt(n: number): number {
  return Math.floor(Math.random() * n);
}

const t0 = performance.now();
export function millis(): number { return performance.now() - t0; }

export function fillBlack(buf: CRGB[]): void {
  for (let i = 0; i < buf.length; i++) { buf[i].r = 0; buf[i].g = 0; buf[i].b = 0; }
}
export function newBuffer(): CRGB[] {
  return Array.from({ length: NUM_LEDS }, () => ({ r: 0, g: 0, b: 0 }));
}

// ── inoise8: 3D gradient noise returning 0..255 ────────────────────
// FastLED's inoise8 uses a fixed-point lattice where one cell = 256
// input units. We reproduce that scale with classic Perlin noise.
// Deterministic (fixed-seed permutation) so tests are stable.
const PERM = new Uint8Array(512);
(() => {
  const p = new Uint8Array(256);
  for (let i = 0; i < 256; i++) p[i] = i;
  let seed = 1337;                          // fixed LCG shuffle
  for (let i = 255; i > 0; i--) {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    const j = seed % (i + 1);
    const tmp = p[i]; p[i] = p[j]; p[j] = tmp;
  }
  for (let i = 0; i < 512; i++) PERM[i] = p[i & 255];
})();

function fade(t: number): number { return t * t * t * (t * (t * 6 - 15) + 10); }
function lerp(a: number, b: number, t: number): number { return a + (b - a) * t; }
function grad(hash: number, x: number, y: number, z: number): number {
  const h = hash & 15;
  const u = h < 8 ? x : y;
  const v = h < 4 ? y : (h === 12 || h === 14 ? x : z);
  return ((h & 1) === 0 ? u : -u) + ((h & 2) === 0 ? v : -v);
}

function perlin3(x: number, y: number, z: number): number {
  const X = Math.floor(x) & 255, Y = Math.floor(y) & 255, Z = Math.floor(z) & 255;
  x -= Math.floor(x); y -= Math.floor(y); z -= Math.floor(z);
  const u = fade(x), v = fade(y), w = fade(z);
  const A = PERM[X] + Y, AA = PERM[A] + Z, AB = PERM[A + 1] + Z;
  const B = PERM[X + 1] + Y, BA = PERM[B] + Z, BB = PERM[B + 1] + Z;
  return lerp(
    lerp(
      lerp(grad(PERM[AA], x, y, z), grad(PERM[BA], x - 1, y, z), u),
      lerp(grad(PERM[AB], x, y - 1, z), grad(PERM[BB], x - 1, y - 1, z), u), v),
    lerp(
      lerp(grad(PERM[AA + 1], x, y, z - 1), grad(PERM[BA + 1], x - 1, y, z - 1), u),
      lerp(grad(PERM[AB + 1], x, y - 1, z - 1), grad(PERM[BB + 1], x - 1, y - 1, z - 1), u), v),
    w);
}

export function inoise8(x: number, y = 0, z = 0): number {
  const n = perlin3(x / 256, y / 256, z / 256);      // ≈ -1..1
  return clamp(Math.round((n * 0.5 + 0.5) * 255), 0, 255);
}
