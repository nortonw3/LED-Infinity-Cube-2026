# LED Infinity Cube — 3D Web Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A static-site 3D simulator of the LED Infinity Cube — all 22 firmware animations on the real cube geometry, glowing wireframe + bloom, driven by system audio / demo track / synthetic audio, with auto-cycle demo reel and manual controls.

**Architecture:** Hand-port of the firmware's animation math, geometry, palettes, and audio engine to TypeScript modules. The firmware in `src/` is the single source of truth — every port cites exact source lines. Three.js renders 384 instanced glowing quads; Web Audio feeds a ported `AudioBus`; a controller owns the frame loop, crossfades, and UI.

**Tech Stack:** Vite + TypeScript + Three.js (latest, `three/addons/` import paths), Vitest for unit tests, Web Audio API, `getDisplayMedia` for system audio. No backend.

## Global Constraints

- Everything lives in `sim/` at the repo root. **Never modify** `src/`, `platformio.ini`, or any firmware file.
- Firmware reference files (read-only sources of truth): `src/cube.h`, `src/palettes.h`, `src/audio_engine.h`, `src/animations.h`, `src/LEDCube3-30-26.ino`, `src/config.h`.
- Constants (from `src/config.h`): `NUM_EDGES=12`, `LEDS_PER_EDGE=32`, `NUM_LEDS=384`, `TARGET_FPS=45`, `BRIGHTNESS_MIN=5`, `BRIGHTNESS_MAX=255`.
- Animation counts: 8 static, 10 audio, 4 voice. Names must match firmware name tables (`animations.h:1062-1073`).
- 11 palette entries: 10 real + index 10 = "Rotate" (`palettes.h:92-98,170-174`).
- All animation functions use signature `(buf: CRGB[], t: number) => void` where `t` is seconds — matching firmware `AnimFunc(CRGB*, float)`.
- Exact bit-identity with FastLED is **not** required (e.g. `inoise8`); visual character parity **is** required. Geometry and palette math must match exactly.
- Every task ends with `npm test` and `npm run build` passing in `sim/`, then a commit.
- Commit messages end with: `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`

## C++ → TypeScript porting rules (apply in every animation port)

| Firmware | TypeScript |
|---|---|
| `float`, `sinf/cosf/expf/sqrtf/atan2f/fabsf/fmodf/powf` | `number`, `Math.sin/cos/exp/sqrt/atan2/abs`, `fmodf(a,b)` → `((a % b) + b) % b` when operand may be negative, else `a % b` (firmware `fmodf` args are always ≥0 after the `+2.0f`/`+1.0f` offsets — keep those offsets) |
| `constrain(x, lo, hi)` | `clamp(x, lo, hi)` from `fastled.ts` |
| `min/max` | `Math.min/Math.max` |
| `random(n)` | `randomInt(n)` from `fastled.ts` (0..n-1) |
| `random(a,b)` | `randomInt(b - a) + a` |
| `millis()` | `millis()` from `fastled.ts` (ms since app start) |
| `inoise8(x,y,z)` / `inoise8(x,y)` | `inoise8(x,y,z)` / `inoise8(x,y)` from `fastled.ts` (returns 0..255) |
| `CRGB` | `CRGB` = `{r,g,b}` (0..255 numbers) from `fastled.ts` |
| `qadd8(a,b)` | `qadd8(a,b)` (saturating 8-bit add) |
| `fill_solid(buf, N, CRGB::Black)` | `fillBlack(buf)` |
| `applyPalette(v)` | `applyPalette(v)` from `palettes.ts` |
| `audio.sub()` … `audio.brilliance()` | `audio.band[0]` … `audio.band[6]` (or helper getters on the bus object) |
| `rdNeighborL[i]` / `rdNeighborR[i]` / `graphL(i)` / `graphR(i)` | `graphL[i]` / `graphR[i]` arrays from `geometry.ts` |
| `voxels[i].x/y/z` | `voxels[i].x/y/z` from `geometry.ts` |
| file-scope `static` animation state | module-scope `let`/arrays in that animation's file, plus an exported `reset()` if the anim has an `inited` flag |
| `uint8_t`/`uint16_t`/`uint32_t` wrap tricks | keep values as plain numbers; apply `& 0xFF` / `& 0xFFFF` only where firmware relies on wrap (noted per animation) |
| `#define X v` | `const X = v` |

## File structure (all under `sim/`)

```
sim/
  index.html
  package.json / tsconfig.json / vite.config.ts
  public/demo-track.mp3          (royalty-free, added in Task 9)
  src/
    geometry.ts        voxels[384], graphL/graphR — port of cube.h
    fastled.ts         CRGB, qadd8, clamp, randomInt, millis, inoise8, blend
    palettes.ts        10 palettes + Rotate + applyPalette + startPaletteFade
    engine.ts          crossfade (renderFrame), brightness, anim registry, demo reel
    renderer.ts        Three.js scene: instanced LEDs + bloom + OrbitControls
    audio/bus.ts       AudioBus type + zeroed default
    audio/engine.ts    FFT bins → bands → AGC → beat/tempo → AudioBus
    audio/sources.ts   system audio | demo track | synthetic — feed one AnalyserNode
    animations/statics.ts   8 static anims
    animations/audios.ts    10 audio anims
    animations/voices.ts    4 voice anims
    ui.ts              overlay controls
    main.ts            bootstrap + frame loop
  tests/
    geometry.test.ts  fastled.test.ts  palettes.test.ts  engine.test.ts  audio-engine.test.ts
```

---

### Task 1: Scaffold the sim project

**Files:**
- Create: `sim/package.json`, `sim/tsconfig.json`, `sim/vite.config.ts`, `sim/index.html`, `sim/src/main.ts`, `sim/tests/smoke.test.ts`
- Modify: `.gitignore` (repo root — add `sim/node_modules/`, `sim/dist/`)

**Interfaces:**
- Produces: a working `npm run dev` / `npm run build` / `npm test` toolchain every later task relies on.

- [ ] **Step 1: Create package.json**

`sim/package.json`:
```json
{
  "name": "led-cube-sim",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc --noEmit && vite build",
    "preview": "vite preview",
    "test": "vitest run"
  },
  "dependencies": {
    "three": "^0.170.0"
  },
  "devDependencies": {
    "@types/three": "^0.170.0",
    "typescript": "^5.6.0",
    "vite": "^6.0.0",
    "vitest": "^2.1.0"
  }
}
```

- [ ] **Step 2: Create tsconfig.json**

`sim/tsconfig.json`:
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "noUnusedLocals": false,
    "skipLibCheck": true,
    "types": ["vite/client"]
  },
  "include": ["src", "tests"]
}
```

- [ ] **Step 3: Create vite.config.ts**

`sim/vite.config.ts`:
```ts
import { defineConfig } from 'vite';

export default defineConfig({
  base: './',   // relative paths so it works on GitHub Pages subpaths
});
```

- [ ] **Step 4: Create index.html and a placeholder main.ts**

`sim/index.html`:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>LED Infinity Cube</title>
  <style>
    html, body { margin: 0; height: 100%; background: #000; overflow: hidden; }
    #app { width: 100%; height: 100%; }
  </style>
</head>
<body>
  <div id="app"></div>
  <script type="module" src="/src/main.ts"></script>
</body>
</html>
```

`sim/src/main.ts`:
```ts
console.log('LED Infinity Cube sim — scaffold OK');
```

- [ ] **Step 5: Write a smoke test**

`sim/tests/smoke.test.ts`:
```ts
import { describe, it, expect } from 'vitest';

describe('toolchain', () => {
  it('runs tests', () => {
    expect(1 + 1).toBe(2);
  });
});
```

- [ ] **Step 6: Update repo .gitignore**

Append to the repo-root `.gitignore` (create if missing):
```
sim/node_modules/
sim/dist/
```

- [ ] **Step 7: Install and verify**

Run (from `sim/`): `npm install`, then `npm test`, then `npm run build`
Expected: install succeeds; vitest reports `1 passed`; build emits `dist/` with no TS errors.

- [ ] **Step 8: Commit**

```bash
git add sim/ .gitignore
git commit -m "feat(sim): scaffold Vite + TypeScript + Three.js project"
```

---

### Task 2: Geometry — voxels + edge graph

**Files:**
- Create: `sim/src/geometry.ts`
- Test: `sim/tests/geometry.test.ts`
- Reference: `src/cube.h:43-111` (read-only)

**Interfaces:**
- Produces: `NUM_EDGES: 12`, `LEDS_PER_EDGE: 32`, `NUM_LEDS: 384`, `voxels: Voxel[]` (length 384, `Voxel = {x,y,z}` numbers), `graphL: number[]`, `graphR: number[]` (welded edge-graph neighbors), `CUBE_CORNERS: Voxel[8]`, `EQUATOR_VERTS: Voxel[6]`.

- [ ] **Step 1: Write the failing tests**

`sim/tests/geometry.test.ts`:
```ts
import { describe, it, expect } from 'vitest';
import { NUM_LEDS, NUM_EDGES, LEDS_PER_EDGE, voxels, graphL, graphR } from '../src/geometry';

describe('geometry', () => {
  it('has 384 voxels on 12 edges of 32', () => {
    expect(NUM_EDGES).toBe(12);
    expect(LEDS_PER_EDGE).toBe(32);
    expect(NUM_LEDS).toBe(384);
    expect(voxels.length).toBe(384);
  });

  it('edge 0 runs from A(0,0,0) to B(1,0,0)', () => {
    expect(voxels[0]).toEqual({ x: 0, y: 0, z: 0 });
    expect(voxels[31]).toEqual({ x: 1, y: 0, z: 0 });
    expect(voxels[16].x).toBeCloseTo(16 / 31, 6);
    expect(voxels[16].y).toBe(0);
  });

  it('every voxel coordinate is within [0,1]', () => {
    for (const v of voxels) {
      expect(v.x).toBeGreaterThanOrEqual(0); expect(v.x).toBeLessThanOrEqual(1);
      expect(v.y).toBeGreaterThanOrEqual(0); expect(v.y).toBeLessThanOrEqual(1);
      expect(v.z).toBeGreaterThanOrEqual(0); expect(v.z).toBeLessThanOrEqual(1);
    }
  });

  it('interior LEDs chain along their own edge', () => {
    expect(graphL[5]).toBe(4);
    expect(graphR[5]).toBe(6);
  });

  it('vertex welds connect different edges (endpoint neighbor is NOT on own edge)', () => {
    // LED 0 is edge 0's base at vertex A; after welding, its L-neighbor must be
    // an endpoint of another edge meeting at A — not LED 31 of its own edge.
    const ownEdge = new Set(Array.from({ length: 32 }, (_, i) => i));
    expect(ownEdge.has(graphL[0])).toBe(false);
  });

  it('every LED has valid, non-self neighbors', () => {
    for (let i = 0; i < NUM_LEDS; i++) {
      expect(graphL[i]).toBeGreaterThanOrEqual(0); expect(graphL[i]).toBeLessThan(NUM_LEDS);
      expect(graphR[i]).toBeGreaterThanOrEqual(0); expect(graphR[i]).toBeLessThan(NUM_LEDS);
      expect(graphL[i]).not.toBe(i);
      expect(graphR[i]).not.toBe(i);
    }
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run (from `sim/`): `npx vitest run tests/geometry.test.ts`
Expected: FAIL — cannot resolve `../src/geometry`.

- [ ] **Step 3: Implement geometry.ts (direct port of cube.h)**

`sim/src/geometry.ts`:
```ts
// Direct port of src/cube.h:43-111 (buildCubeGeometry + buildRDNeighbors).
// The firmware is the source of truth — do not "improve" the math.

export const NUM_EDGES = 12;
export const LEDS_PER_EDGE = 32;
export const NUM_LEDS = NUM_EDGES * LEDS_PER_EDGE;

export interface Voxel { x: number; y: number; z: number; }

export const CUBE_CORNERS: Voxel[] = [
  { x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 },
  { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 1 }, { x: 1, y: 1, z: 1 }, { x: 0, y: 1, z: 1 },
];
export const EQUATOR_VERTS: Voxel[] = [
  { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 },
  { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 1 }, { x: 0, y: 1, z: 1 },
];

export const voxels: Voxel[] = Array.from({ length: NUM_LEDS }, () => ({ x: 0, y: 0, z: 0 }));
export const graphL: number[] = new Array(NUM_LEDS).fill(0);
export const graphR: number[] = new Array(NUM_LEDS).fill(0);

function mapEdge(offset: number, x1: number, y1: number, z1: number,
                 x2: number, y2: number, z2: number): void {
  for (let i = 0; i < LEDS_PER_EDGE; i++) {
    const t = i / (LEDS_PER_EDGE - 1);
    voxels[offset + i].x = x1 + (x2 - x1) * t;
    voxels[offset + i].y = y1 + (y2 - y1) * t;
    voxels[offset + i].z = z1 + (z2 - z1) * t;
  }
}

function buildCubeGeometry(): void {
  const A = [0, 0, 0], B = [1, 0, 0], C = [1, 1, 0], D = [0, 1, 0];
  const E = [0, 0, 1], F = [1, 0, 1], G = [1, 1, 1], H = [0, 1, 1];
  // Same edge order as cube.h:57-68
  const edges = [
    [A, B], [B, F], [B, C], [C, G], [C, D], [D, H],
    [D, A], [A, E], [E, F], [F, G], [G, H], [H, E],
  ];
  edges.forEach(([p, q], e) =>
    mapEdge(e * LEDS_PER_EDGE, p[0], p[1], p[2], q[0], q[1], q[2]));
}

// Port of buildRDNeighbors (cube.h:78-106): chain LEDs along each edge, then
// weld the endpoints meeting at each of the 8 vertices into a ring so fields
// flow through corners.
function buildEdgeGraph(): void {
  const EF = [0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 6, 7]; // "from" vertex of each edge
  const ET = [1, 5, 2, 6, 3, 7, 0, 4, 5, 6, 7, 4]; // "to" vertex of each edge
  for (let e = 0; e < NUM_EDGES; e++) {
    const base = e * LEDS_PER_EDGE, last = base + LEDS_PER_EDGE - 1;
    for (let i = base; i <= last; i++) {
      graphL[i] = i > base ? i - 1 : last;
      graphR[i] = i < last ? i + 1 : base;
    }
  }
  for (let v = 0; v < 8; v++) {
    const eps: { led: number; edge: number }[] = [];
    for (let e = 0; e < NUM_EDGES; e++) {
      const base = e * LEDS_PER_EDGE, tip = base + LEDS_PER_EDGE - 1;
      if (EF[e] === v) eps.push({ led: base, edge: e });
      if (ET[e] === v) eps.push({ led: tip, edge: e });
    }
    if (eps.length < 2) continue;
    for (let a = 0; a < eps.length; a++) {
      const b = (a + 1) % eps.length;
      const ledA = eps[a].led, ledB = eps[b].led;
      const aIsBase = ledA === eps[a].edge * LEDS_PER_EDGE;
      const bIsBase = ledB === eps[b].edge * LEDS_PER_EDGE;
      if (aIsBase) graphL[ledA] = ledB; else graphR[ledA] = ledB;
      if (bIsBase) graphL[ledB] = ledA; else graphR[ledB] = ledA;
    }
  }
}

buildCubeGeometry();
buildEdgeGraph();
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run tests/geometry.test.ts`
Expected: PASS (6 tests).

- [ ] **Step 5: Commit**

```bash
git add sim/src/geometry.ts sim/tests/geometry.test.ts
git commit -m "feat(sim): port cube geometry and welded edge graph from cube.h"
```

---

### Task 3: FastLED/Arduino shims

**Files:**
- Create: `sim/src/fastled.ts`
- Test: `sim/tests/fastled.test.ts`

**Interfaces:**
- Produces:
  - `interface CRGB { r: number; g: number; b: number }` (0..255 each)
  - `rgb(r, g, b): CRGB`
  - `clamp(x, lo, hi): number`
  - `qadd8(a, b): number` — saturating add, ≤ 255
  - `blend(a: CRGB, b: CRGB, amount: number): CRGB` — amount 0..255 blends a→b (FastLED `blend()` semantics)
  - `randomInt(n): number` — uniform integer 0..n-1
  - `millis(): number` — ms since module load
  - `inoise8(x, y?, z?): number` — value 0..255; deterministic Perlin with FastLED's input scale (one lattice cell per 256 input units). NOT bit-identical to FastLED — visual character parity only (Global Constraints).
  - `fillBlack(buf: CRGB[]): void`
  - `newBuffer(): CRGB[]` — 384 black pixels

- [ ] **Step 1: Write the failing tests**

`sim/tests/fastled.test.ts`:
```ts
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npx vitest run tests/fastled.test.ts`
Expected: FAIL — cannot resolve `../src/fastled`.

- [ ] **Step 3: Implement fastled.ts**

`sim/src/fastled.ts`:
```ts
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run tests/fastled.test.ts`
Expected: PASS (7 tests).

- [ ] **Step 5: Commit**

```bash
git add sim/src/fastled.ts sim/tests/fastled.test.ts
git commit -m "feat(sim): FastLED/Arduino compatibility shims (CRGB, noise, blend)"
```

---

### Task 4: Palettes + applyPalette

**Files:**
- Create: `sim/src/palettes.ts`
- Test: `sim/tests/palettes.test.ts`
- Reference: `src/palettes.h` (entire file, read-only)

**Interfaces:**
- Consumes: `CRGB`, `rgb`, `clamp`, `blend`, `millis` from `fastled.ts`.
- Produces:
  - `PALETTE_NAMES: string[]` — exactly `["Embers","IceSplnt","Toxic","NeonSign","DeepSea","Forge","Aurora","Stardust","BloodMoon","Glitch","Rotate"]`
  - `NUM_PALETTES_TOTAL = 11`, `PALETTE_ROTATE_IDX = 10`
  - `applyPalette(intensity: number): CRGB` — 0..1 in, color out; handles normal / fading / rotate modes exactly like `palettes.h:123-164`
  - `startPaletteFade(newIndex: number): void` — `palettes.h:115-121`
  - `getPaletteIndex(): number`
  - `colorFromPalette16(pal: CRGB[], index8: number): CRGB` — FastLED `ColorFromPalette(..., LINEARBLEND)` (exported for tests)

- [ ] **Step 1: Write the failing tests**

`sim/tests/palettes.test.ts`:
```ts
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npx vitest run tests/palettes.test.ts`
Expected: FAIL — cannot resolve `../src/palettes`.

- [ ] **Step 3: Implement palettes.ts**

Transcribe all 10 palettes from `src/palettes.h:9-90` **exactly**. Named colors map to: `CRGB::Black`=(0,0,0), `CRGB::White`=(255,255,255), `CRGB::OrangeRed`=(255,69,0), `CRGB::Red`=(255,0,0), `CRGB::Magenta`=(255,0,255), `CRGB::Cyan`=(0,255,255).

`sim/src/palettes.ts`:
```ts
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run tests/palettes.test.ts`
Expected: PASS (6 tests).

- [ ] **Step 5: Commit**

```bash
git add sim/src/palettes.ts sim/tests/palettes.test.ts
git commit -m "feat(sim): port 10 palettes + applyPalette with fade and rotate modes"
```

---

### Task 5: Three.js renderer — glowing LEDs + bloom + orbit

**Files:**
- Create: `sim/src/renderer.ts`
- Modify: `sim/src/main.ts` (temporary test pattern to verify visually)

**Interfaces:**
- Consumes: `voxels`, `NUM_LEDS` from `geometry.ts`; `CRGB` from `fastled.ts`.
- Produces:
  - `class CubeRenderer { constructor(container: HTMLElement); setColors(buf: CRGB[]): void; render(): void; resize(): void; dispose(): void }`
  - LEDs drawn as 384 instanced spheres centered on `voxel - 0.5` (cube centered at origin), plus faint dark edge lines for structure.
  - Bloom via `EffectComposer` + `RenderPass` + `UnrealBloomPass` + `OutputPass`.
  - `OrbitControls` with `autoRotate = true` (pauses on user interaction via built-in behavior: set `autoRotate = false` on `start` event, re-enable after 3 s idle).

No unit test — this is the visual layer; it is verified by eye with a test pattern (Step 3) and by `npm run build` type-checking.

- [ ] **Step 1: Implement renderer.ts**

`sim/src/renderer.ts`:
```ts
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { voxels, NUM_LEDS, NUM_EDGES, LEDS_PER_EDGE } from './geometry';
import type { CRGB } from './fastled';

const LED_RADIUS = 0.012;

export class CubeRenderer {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera: THREE.PerspectiveCamera;
  private controls: OrbitControls;
  private composer: EffectComposer;
  private leds: THREE.InstancedMesh;
  private color = new THREE.Color();
  private idleTimer = 0;

  constructor(container: HTMLElement) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.setSize(container.clientWidth, container.clientHeight);
    container.appendChild(this.renderer.domElement);

    this.camera = new THREE.PerspectiveCamera(
      45, container.clientWidth / container.clientHeight, 0.1, 100);
    this.camera.position.set(1.6, 1.2, 1.6);

    this.scene.background = new THREE.Color(0x000000);

    // ── 384 instanced LED spheres ────────────────────────────
    const geo = new THREE.SphereGeometry(LED_RADIUS, 8, 8);
    const mat = new THREE.MeshBasicMaterial({ toneMapped: false });
    this.leds = new THREE.InstancedMesh(geo, mat, NUM_LEDS);
    const m = new THREE.Matrix4();
    for (let i = 0; i < NUM_LEDS; i++) {
      m.setPosition(voxels[i].x - 0.5, voxels[i].z - 0.5, voxels[i].y - 0.5);
      this.leds.setMatrixAt(i, m);           // note: firmware z = up → three.js y
      this.leds.setColorAt(i, this.color.setRGB(0, 0, 0));
    }
    this.scene.add(this.leds);

    // ── Faint edge lines for structure ───────────────────────
    const linePts: THREE.Vector3[] = [];
    for (let e = 0; e < NUM_EDGES; e++) {
      const a = voxels[e * LEDS_PER_EDGE], b = voxels[e * LEDS_PER_EDGE + LEDS_PER_EDGE - 1];
      linePts.push(new THREE.Vector3(a.x - 0.5, a.z - 0.5, a.y - 0.5));
      linePts.push(new THREE.Vector3(b.x - 0.5, b.z - 0.5, b.y - 0.5));
    }
    const lineGeo = new THREE.BufferGeometry().setFromPoints(linePts);
    const lineMat = new THREE.LineBasicMaterial({ color: 0x14141c, toneMapped: false });
    this.scene.add(new THREE.LineSegments(lineGeo, lineMat));

    // ── Controls ─────────────────────────────────────────────
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.autoRotate = true;
    this.controls.autoRotateSpeed = 0.8;
    this.controls.addEventListener('start', () => {
      this.controls.autoRotate = false;
      window.clearTimeout(this.idleTimer);
    });
    this.controls.addEventListener('end', () => {
      window.clearTimeout(this.idleTimer);
      this.idleTimer = window.setTimeout(() => { this.controls.autoRotate = true; }, 3000);
    });

    // ── Bloom pipeline ───────────────────────────────────────
    this.composer = new EffectComposer(this.renderer);
    this.composer.addPass(new RenderPass(this.scene, this.camera));
    const bloom = new UnrealBloomPass(
      new THREE.Vector2(container.clientWidth, container.clientHeight),
      1.4,   // strength
      0.5,   // radius
      0.0);  // threshold — every lit LED blooms
    this.composer.addPass(bloom);
    this.composer.addPass(new OutputPass());

    window.addEventListener('resize', () => this.resize());
  }

  setColors(buf: CRGB[]): void {
    for (let i = 0; i < NUM_LEDS; i++) {
      this.leds.setColorAt(i, this.color.setRGB(
        buf[i].r / 255, buf[i].g / 255, buf[i].b / 255));
    }
    this.leds.instanceColor!.needsUpdate = true;
  }

  render(): void {
    this.controls.update();
    this.composer.render();
  }

  resize(): void {
    const el = this.renderer.domElement.parentElement!;
    this.camera.aspect = el.clientWidth / el.clientHeight;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(el.clientWidth, el.clientHeight);
    this.composer.setSize(el.clientWidth, el.clientHeight);
  }

  dispose(): void { this.renderer.dispose(); }
}
```

- [ ] **Step 2: Wire a temporary test pattern in main.ts**

Replace `sim/src/main.ts`:
```ts
import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { voxels, NUM_LEDS } from './geometry';
import { applyPalette } from './palettes';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

// TEMP test pattern: diagonal rainbow sweep (replaced in Task 6)
function frame(nowMs: number) {
  const t = nowMs * 0.001;
  for (let i = 0; i < NUM_LEDS; i++) {
    const h = (voxels[i].x + voxels[i].y + voxels[i].z) / 3;
    const v = (Math.sin(h * 6.28 + t * 2) + 1) * 0.5;
    const c = applyPalette(v);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
  cube.setColors(buf);
  cube.render();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
```

- [ ] **Step 3: Verify visually**

Run: `npm run dev` and open the printed URL in a browser.
Expected: a dark scene with a glowing wireframe cube; an Embers-colored wave flows diagonally; bloom halo around lit LEDs; drag orbits; release → auto-rotate resumes after 3 s.

- [ ] **Step 4: Verify build + tests still pass**

Run: `npm test && npm run build`
Expected: all tests pass; build clean.

- [ ] **Step 5: Commit**

```bash
git add sim/src/renderer.ts sim/src/main.ts
git commit -m "feat(sim): Three.js renderer with instanced LEDs, bloom, orbit controls"
```

---

### Task 6: Engine — crossfade, brightness, animation registry, demo reel

**Files:**
- Create: `sim/src/engine.ts`
- Test: `sim/tests/engine.test.ts`
- Reference: `src/LEDCube3-30-26.ino:195-222` (crossfade), `:172-178` (brightness), `:504-540` (demo mode), `src/config.h:43-46` (counts)

**Interfaces:**
- Consumes: `CRGB`, `newBuffer`, `fillBlack`, `blend`, `clamp`, `randomInt`, `millis` from `fastled.ts`; `startPaletteFade`, `NUM_PALETTES_TOTAL` from `palettes.ts`.
- Produces:
  - `type AnimFunc = (buf: CRGB[], t: number) => void`
  - `type Mode = 'static' | 'audio' | 'voice'`
  - `registerAnimations(mode: Mode, anims: AnimFunc[], names: string[]): void` — called once per mode by the animation modules
  - `getAnimNames(mode: Mode): string[]`
  - `getState(): { mode: Mode; index: number; brightness: number; demo: boolean }`
  - `selectAnimation(mode: Mode, index: number, immediate?: boolean): void` — starts a 2 s crossfade (`startTransition` port); `immediate: true` skips the fade (used by tests)
  - `setBrightness(b: number): void` — 5..255, scales final output like `FastLED.setBrightness`
  - `setDemo(on: boolean): void` — auto-cycle; every 30 s picks a random different entry from the full 22-entry demo list (`LEDCube3-30-26.ino:510-533`) AND starts a random palette fade
  - `renderFrame(out: CRGB[], t: number): void` — port of `renderFrame` (`.ino:204-222`): runs current anim, or blends bufferA/bufferB during transition, then applies brightness scaling; call `tickDemo()` internally each frame
- Register placeholder anims initially so the engine works before Tasks 8/10/11 fill the registry.

- [ ] **Step 1: Write the failing tests**

`sim/tests/engine.test.ts`:
```ts
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npx vitest run tests/engine.test.ts`
Expected: FAIL — cannot resolve `../src/engine`.

- [ ] **Step 3: Implement engine.ts**

`sim/src/engine.ts`:
```ts
// Port of the firmware's crossfade engine, brightness scaling, and demo
// reel (LEDCube3-30-26.ino:195-222, 172-178, 504-540).
import { CRGB, newBuffer, fillBlack, blend, clamp, randomInt, millis } from './fastled';
import { startPaletteFade } from './palettes';

export type AnimFunc = (buf: CRGB[], t: number) => void;
export type Mode = 'static' | 'audio' | 'voice';

const placeholder: AnimFunc = (buf, t) => {
  const v = Math.round((Math.sin(t * 1.2) + 1) * 0.5 * 0.3 * 255);
  for (const p of buf) { p.r = v; p.g = v; p.b = v; }
};

const registry: Record<Mode, { anims: AnimFunc[]; names: string[] }> = {
  static: { anims: [placeholder], names: ['--'] },
  audio: { anims: [placeholder], names: ['--'] },
  voice: { anims: [placeholder], names: ['--'] },
};

export function registerAnimations(mode: Mode, anims: AnimFunc[], names: string[]): void {
  registry[mode] = { anims, names };
}
export function getAnimNames(mode: Mode): string[] { return registry[mode].names; }

// ── State ───────────────────────────────────────────────────────────
let currentMode: Mode = 'static';
let currentIndex = 0;
let brightness = 128;                 // .ino:29 default
let demoEnabled = false;

let currentAnim: AnimFunc = placeholder;
let nextAnim: AnimFunc = placeholder;
let transitioning = false;
let transitionStart = 0;
const transitionDuration = 2.0;       // .ino:62

const bufferA = newBuffer();
const bufferB = newBuffer();

export function getState() {
  return { mode: currentMode, index: currentIndex, brightness, demo: demoEnabled };
}

// Port of startTransition (.ino:198-202)
function startTransition(target: AnimFunc): void {
  nextAnim = target;
  transitioning = true;
  transitionStart = millis() * 0.001;
  fillBlack(bufferA);
  fillBlack(bufferB);
}

export function selectAnimation(mode: Mode, index: number, immediate = false): void {
  const { anims } = registry[mode];
  currentMode = mode;
  currentIndex = clamp(index, 0, anims.length - 1);
  if (immediate) {
    currentAnim = anims[currentIndex];
    transitioning = false;
  } else {
    startTransition(anims[currentIndex]);
  }
}

export function setBrightness(b: number): void {
  brightness = clamp(Math.round(b), 5, 255);   // BRIGHTNESS_MIN..MAX (config.h)
}

// ── Demo reel — port of demoUpdate (.ino:507-533) ───────────────────
const DEMO_INTERVAL_MS = 30000;
const demoList: [Mode, number][] = [
  ...Array.from({ length: 8 }, (_, i) => ['static', i] as [Mode, number]),
  ...Array.from({ length: 10 }, (_, i) => ['audio', i] as [Mode, number]),
  ...Array.from({ length: 4 }, (_, i) => ['voice', i] as [Mode, number]),
];
let demoLastChange = 0;
let demoStep = -1;

export function setDemo(on: boolean): void {
  demoEnabled = on;
  if (on) { demoLastChange = millis() - DEMO_INTERVAL_MS; demoStep = -1; }
}

function tickDemo(): void {
  if (!demoEnabled) return;
  const now = millis();
  if (now - demoLastChange < DEMO_INTERVAL_MS) return;
  demoLastChange = now;
  let next: number;
  do { next = randomInt(demoList.length); } while (next === demoStep && demoList.length > 1);
  demoStep = next;
  startPaletteFade(randomInt(11));     // random(NUM_PALETTES)=11 incl. Rotate, .ino:527
  const [mode, idx] = demoList[demoStep];
  selectAnimation(mode, idx);
}

// ── Frame render — port of renderFrame (.ino:204-222) ───────────────
export function renderFrame(out: CRGB[], t: number): void {
  tickDemo();
  fillBlack(out);
  if (!transitioning) {
    currentAnim(out, t);
  } else {
    currentAnim(bufferA, t);
    nextAnim(bufferB, t);
    const p = (millis() * 0.001 - transitionStart) / transitionDuration;
    if (p >= 1.0) {
      transitioning = false;
      currentAnim = nextAnim;
      fillBlack(bufferA); fillBlack(bufferB);
      currentAnim(out, t);
    } else {
      const ba = Math.floor(p * 255);
      for (let i = 0; i < out.length; i++) {
        const c = blend(bufferA[i], bufferB[i], ba);
        out[i].r = c.r; out[i].g = c.g; out[i].b = c.b;
      }
    }
  }
  // FastLED.setBrightness equivalent — scale final output
  const bf = brightness / 255;
  if (bf < 1) {
    for (const p of out) {
      p.r = Math.round(p.r * bf); p.g = Math.round(p.g * bf); p.b = Math.round(p.b * bf);
    }
  }
}
```

Note for the implementer: the firmware computes the transition progress from the render-time `t` (`(t - transitionStart) / duration` with both on the millis clock). The sim's `t` may come from a different clock than `millis()`, so the engine reads `millis()` directly for transition progress — same behavior, immune to clock mismatch.

- [ ] **Step 4: Run tests to verify they pass**

Run: `npx vitest run tests/engine.test.ts`
Expected: PASS (5 tests).

- [ ] **Step 5: Commit**

```bash
git add sim/src/engine.ts sim/tests/engine.test.ts
git commit -m "feat(sim): crossfade engine, brightness, registry, demo reel"
```

---

### Task 7: AudioBus + audio engine port

**Files:**
- Create: `sim/src/audio/bus.ts`, `sim/src/audio/engine.ts`
- Test: `sim/tests/audio-engine.test.ts`
- Reference: `src/audio_engine.h` (entire file, read-only)

**Interfaces:**
- Consumes: `clamp` from `fastled.ts`.
- Produces (`bus.ts`):
  - `NUM_BANDS = 7`
  - `interface AudioBus { band: number[]; flux: number[]; level: number; centroid: number; beatPhase: number; barPhase: number; beatFired: boolean; bpm: number; tempoConfidence: number; speech: number; syllableOnset: boolean; sylEnv: number; sub(): number; bass(): number; lowMid(): number; mid(): number; highMid(): number; presence(): number; brilliance(): number }`
  - `audio: AudioBus` — the shared singleton every animation reads (mirrors firmware global `audio`)
- Produces (`engine.ts`):
  - `audioEngineUpdate(fftBins: Float32Array, nowMs: number): void` — mutates the `audio` singleton. `fftBins` is 512 magnitudes (0..~1) matching firmware `fftBins[FFT_BINS]` layout at ~43 Hz/bin.
  - `knobs = { reactivity: 1.0, beatSensitivity: 1.0, bandTilt: 0.0 }` — mutable export
  - `resetAudioEngine(): void` — clears AGC/tempo state (for tests)

- [ ] **Step 1: Write the failing tests**

`sim/tests/audio-engine.test.ts`:
```ts
import { describe, it, expect, beforeEach } from 'vitest';
import { audio, NUM_BANDS } from '../src/audio/bus';
import { audioEngineUpdate, resetAudioEngine } from '../src/audio/engine';

// Build a 512-bin frame with given per-band energy.
// Band bin ranges (audio_engine.h:30-31): lo=[1,2,4,9,23,58,116] hi=[2,4,9,23,58,116,250]
const LO = [1, 2, 4, 9, 23, 58, 116], HI = [2, 4, 9, 23, 58, 116, 250];
function frame(bandLevels: number[]): Float32Array {
  const f = new Float32Array(512);
  for (let b = 0; b < 7; b++)
    for (let i = LO[b]; i <= HI[b]; i++) f[i] = bandLevels[b];
  return f;
}

describe('audio engine', () => {
  beforeEach(() => resetAudioEngine());

  it('silence → all bands 0', () => {
    let t = 2000;                      // past AGC_WARMUP_MS
    for (let k = 0; k < 60; k++) { audioEngineUpdate(frame([0,0,0,0,0,0,0]), t); t += 22; }
    for (let b = 0; b < NUM_BANDS; b++) expect(audio.band[b]).toBe(0);
    expect(audio.level).toBe(0);
  });

  it('a loud bass burst after silence raises the bass band only', () => {
    // NOTE: use true zeros for the quiet phase — the AGC normalizes any
    // steady nonzero signal toward 1.0 over time (that is its job), so a
    // "quiet 0.01" floor would make every band read high.
    let t = 2000;
    for (let k = 0; k < 100; k++) { audioEngineUpdate(frame([0,0,0,0,0,0,0]), t); t += 22; }
    // loud bass hit, silence elsewhere
    for (let k = 0; k < 10; k++) { audioEngineUpdate(frame([0.9,0.9,0,0,0,0,0]), t); t += 22; }
    expect(audio.band[1]).toBeGreaterThan(0.3);   // bass
    expect(audio.band[5]).toBeLessThan(0.2);      // presence stays silent
  });

  it('warmup gate: bands forced to 0 before 1500 ms', () => {
    audioEngineUpdate(frame([0.9,0.9,0.9,0.9,0.9,0.9,0.9]), 100);
    for (let b = 0; b < NUM_BANDS; b++) expect(audio.band[b]).toBe(0);
  });

  it('periodic bass pulses at 120 BPM produce beats and a plausible bpm', () => {
    let t = 2000;
    const quiet = frame([0.02,0.02,0.02,0.02,0.02,0.02,0.02]);
    const hit = frame([0.95,0.95,0.05,0.05,0.05,0.05,0.05]);
    let beats = 0;
    // 30 s of 120 BPM (hit every 500 ms), engine stepped every 22 ms
    for (let ms = 0; ms < 30000; ms += 22) {
      const inBeat = (ms % 500) < 44;          // ~2 frames of hit
      audioEngineUpdate(inBeat ? hit : quiet, t + ms);
      if (audio.beatFired) beats++;
    }
    expect(beats).toBeGreaterThan(30);          // PLL fires roughly every period
    expect(audio.bpm).toBeGreaterThan(60);
    expect(audio.bpm).toBeLessThan(200);
    expect(audio.beatPhase).toBeGreaterThanOrEqual(0);
    expect(audio.beatPhase).toBeLessThanOrEqual(1);
  });

  it('speech energy tracks mid + highMid', () => {
    let t = 2000;
    for (let k = 0; k < 100; k++) { audioEngineUpdate(frame([0,0,0,0,0,0,0]), t); t += 22; }
    for (let k = 0; k < 10; k++) { audioEngineUpdate(frame([0,0,0,0.8,0.8,0,0]), t); t += 22; }
    expect(audio.speech).toBeGreaterThan(0.2);
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `npx vitest run tests/audio-engine.test.ts`
Expected: FAIL — cannot resolve `../src/audio/bus`.

- [ ] **Step 3: Implement bus.ts**

`sim/src/audio/bus.ts`:
```ts
// Port of the AudioBus struct (audio_engine.h:34-59).
export const NUM_BANDS = 7;

export interface AudioBus {
  band: number[];          // room-adapted, normalized 0..1
  flux: number[];          // positive change per band
  level: number;           // overall loudness 0..1
  centroid: number;        // spectral brightness 0..1
  beatPhase: number;       // 0..1, resets each beat
  barPhase: number;        // 0..1 over 4 beats
  beatFired: boolean;      // true exactly one frame per beat
  bpm: number;
  tempoConfidence: number; // 0..1
  speech: number;
  syllableOnset: boolean;
  sylEnv: number;
  sub(): number; bass(): number; lowMid(): number; mid(): number;
  highMid(): number; presence(): number; brilliance(): number;
}

export const audio: AudioBus = {
  band: new Array(NUM_BANDS).fill(0),
  flux: new Array(NUM_BANDS).fill(0),
  level: 0, centroid: 0,
  beatPhase: 0, barPhase: 0, beatFired: false,
  bpm: 120, tempoConfidence: 0,
  speech: 0, syllableOnset: false, sylEnv: 0,
  sub() { return this.band[0]; },
  bass() { return this.band[1]; },
  lowMid() { return this.band[2]; },
  mid() { return this.band[3]; },
  highMid() { return this.band[4]; },
  presence() { return this.band[5]; },
  brilliance() { return this.band[6]; },
};
```

- [ ] **Step 4: Implement engine.ts (line-faithful port of audioEngineUpdate)**

`sim/src/audio/engine.ts` — port `audio_engine.h:61-263` with these mechanical changes and NOTHING else:
- file-scope `static` state → module-scope `let`/arrays (all listed below)
- `millis()` → the `nowMs` parameter (tests drive time explicitly)
- `constrain` → `clamp`; `sqrtf` → `Math.sqrt`; `fmodf(a,b)` → `a % b` (operands non-negative here)
- `FFT_BINS` → `fftBins.length`

```ts
// Port of src/audio_engine.h:61-263. Keep constants and update order
// identical to the firmware — this is tuned DSP, do not refactor.
import { clamp } from '../fastled';
import { audio, NUM_BANDS } from './bus';

const BAND_LO = [1, 2, 4, 9, 23, 58, 116];
const BAND_HI = [2, 4, 9, 23, 58, 116, 250];

export const knobs = { reactivity: 1.0, beatSensitivity: 1.0, bandTilt: 0.0 };

// AGC constants (audio_engine.h:74-79)
const FLOOR_RISE = 0.0008, FLOOR_FALL = 0.03;
const CEIL_RISE = 0.30, CEIL_DECAY = 0.004;
const BAND_GATE = 0.06;
const AGC_WARMUP_MS = 1500;

// Beat/tempo constants (audio_engine.h:90-93)
const ONSET_LEN = 200;
const BPM_MIN = 60.0, BPM_MAX = 200.0;
const AUTOCORR_EVERY = 8;

// ── State (audio_engine.h:81-103) ──────────────────────────────────
let smoothBand = new Array(NUM_BANDS).fill(0);
let bandFloor = new Array(NUM_BANDS).fill(0);
let bandCeil = new Array(NUM_BANDS).fill(0);
let prevBand = new Array(NUM_BANDS).fill(0);
let onsetEnv = new Array(ONSET_LEN).fill(0);
let onsetHead = 0;
let avgFrameMs = 22.0;
let beatPeriodMsA = 500.0;
let engBeatAccumMs = 0.0;
let engBarAccumBeats = 0.0;
let autocorrCounter = 0;
let lastEngineMs = 0;
let lastOnsetMs = 0;
let speechAvg = 0, speechLast = 0;
let startMs = -1;                    // sim addition: warmup measured from first frame

export function resetAudioEngine(): void {
  smoothBand.fill(0); bandFloor.fill(0); bandCeil.fill(0); prevBand.fill(0);
  onsetEnv.fill(0); onsetHead = 0;
  avgFrameMs = 22.0; beatPeriodMsA = 500.0;
  engBeatAccumMs = 0; engBarAccumBeats = 0;
  autocorrCounter = 0; lastEngineMs = 0; lastOnsetMs = 0;
  speechAvg = 0; speechLast = 0; startMs = -1;
  audio.band.fill(0); audio.flux.fill(0);
  audio.level = 0; audio.centroid = 0;
  audio.beatPhase = 0; audio.barPhase = 0; audio.beatFired = false;
  audio.bpm = 120; audio.tempoConfidence = 0;
  audio.speech = 0; audio.syllableOnset = false; audio.sylEnv = 0;
}

// Port of estimateTempo (audio_engine.h:112-143)
function estimateTempo(): void {
  let minLag = Math.floor((60000.0 / BPM_MAX) / avgFrameMs);
  let maxLag = Math.floor((60000.0 / BPM_MIN) / avgFrameMs);
  minLag = clamp(minLag, 4, ONSET_LEN / 2);
  maxLag = clamp(maxLag, minLag + 1, ONSET_LEN - 1);

  let best = 0, bestLag = 0, sum = 0, lags = 0;
  for (let lag = minLag; lag <= maxLag; lag++) {
    let corr = 0;
    for (let k = 0; k < ONSET_LEN - lag; k++) {
      const i = (onsetHead - 1 - k + ONSET_LEN * 2) % ONSET_LEN;
      const j = (onsetHead - 1 - k - lag + ONSET_LEN * 2) % ONSET_LEN;
      corr += onsetEnv[i] * onsetEnv[j];
    }
    corr /= (ONSET_LEN - lag);
    sum += corr; lags++;
    if (corr > best) { best = corr; bestLag = lag; }
  }
  if (bestLag < 1 || best <= 0) return;

  const mean = sum / Math.max(lags, 1);
  const prominence = (best - mean) / (best + 1e-6);
  const targetPeriod = bestLag * avgFrameMs;
  const targetBpm = clamp(60000.0 / targetPeriod, BPM_MIN, BPM_MAX);

  const newBpm = audio.bpm * 0.8 + targetBpm * 0.2;
  audio.bpm = clamp(newBpm, BPM_MIN, BPM_MAX);
  beatPeriodMsA = 60000.0 / audio.bpm;
  audio.tempoConfidence = audio.tempoConfidence * 0.7 +
    clamp(prominence * 2.0, 0, 1) * 0.3;
}

// Port of audioEngineUpdate (audio_engine.h:149-263)
export function audioEngineUpdate(fftBins: Float32Array, nowMs: number): void {
  if (startMs < 0) startMs = nowMs;
  let dt = lastEngineMs > 0 ? nowMs - lastEngineMs : 22.0;
  dt = clamp(dt, 1.0, 50.0);
  lastEngineMs = nowMs;
  avgFrameMs = avgFrameMs * 0.95 + dt * 0.05;

  const warm = nowMs - startMs > AGC_WARMUP_MS || nowMs > AGC_WARMUP_MS;

  // 1. Perceptual bands
  for (let b = 0; b < NUM_BANDS; b++) {
    let sum = 0;
    for (let i = BAND_LO[b]; i <= BAND_HI[b] && i < fftBins.length; i++) sum += fftBins[i];
    sum /= (BAND_HI[b] - BAND_LO[b] + 1);
    smoothBand[b] = smoothBand[b] * 0.6 + sum * 0.4;
  }

  // 2. Room-noise AGC + features
  let centNum = 0, centDen = 0, levelSum = 0;
  for (let b = 0; b < NUM_BANDS; b++) {
    const r = smoothBand[b];
    const df = r - bandFloor[b];
    bandFloor[b] += df < 0 ? df * FLOOR_FALL : df * FLOOR_RISE;
    const dc = r - bandCeil[b];
    bandCeil[b] += dc > 0 ? dc * CEIL_RISE : dc * CEIL_DECAY;
    if (bandCeil[b] < bandFloor[b] + 1e-5) bandCeil[b] = bandFloor[b] + 1e-5;

    const span = bandCeil[b] - bandFloor[b];
    let norm = span > 1e-6 ? (r - bandFloor[b]) / span : 0.0;
    norm = clamp(norm, 0, 1);
    norm = norm < BAND_GATE ? 0.0 : (norm - BAND_GATE) / (1.0 - BAND_GATE);

    const pos = b / (NUM_BANDS - 1) - 0.5;
    const tiltGain = 1.0 + knobs.bandTilt * pos * 2.0;
    norm = clamp(norm * knobs.reactivity * tiltGain, 0, 1);
    if (!warm) norm = 0.0;

    audio.band[b] = norm;
    const fx = norm - prevBand[b];
    audio.flux[b] = fx > 0 ? fx : 0.0;
    prevBand[b] = norm;

    centNum += norm * b; centDen += norm; levelSum += norm;
  }
  audio.centroid = centDen > 1e-6 ? (centNum / centDen) / (NUM_BANDS - 1) : 0.0;
  audio.level = clamp(levelSum / NUM_BANDS, 0, 1);

  // Voice features
  audio.speech = clamp(audio.mid() * 0.7 + audio.highMid() * 0.7, 0, 1);
  speechAvg = speechAvg * 0.98 + audio.speech * 0.02;
  audio.syllableOnset = false;
  if (audio.speech > speechAvg * 1.6 && audio.speech > 0.08 &&
      audio.speech > speechLast * 1.2) {
    audio.syllableOnset = true;
    audio.sylEnv = 1.0;
  }
  speechLast = audio.speech;
  audio.sylEnv *= 0.91;

  // 3. Onset envelope
  let onset = audio.flux[0] + audio.flux[1];
  let broad = 0;
  for (let b = 2; b < NUM_BANDS; b++) broad += audio.flux[b];
  onset += broad * 0.3;
  onsetEnv[onsetHead] = onset;
  onsetHead = (onsetHead + 1) % ONSET_LEN;

  // 4. Tempo estimate (periodic)
  if (++autocorrCounter >= AUTOCORR_EVERY) {
    autocorrCounter = 0;
    estimateTempo();
  }
  audio.tempoConfidence = clamp(audio.tempoConfidence - 0.0005, 0, 1);

  // 5. Adaptive onset detection
  let mean = 0;
  const win = Math.min(45, ONSET_LEN);
  for (let k = 0; k < win; k++)
    mean += onsetEnv[(onsetHead - 1 - k + ONSET_LEN) % ONSET_LEN];
  mean /= win;
  let variance = 0;
  for (let k = 0; k < win; k++) {
    const d = onsetEnv[(onsetHead - 1 - k + ONSET_LEN) % ONSET_LEN] - mean;
    variance += d * d;
  }
  const sd = Math.sqrt(variance / win);
  const thresh = mean + sd * (1.6 / knobs.beatSensitivity);
  const onsetHit = onset > thresh && onset > 0.004 && nowMs - lastOnsetMs > 90;
  if (onsetHit) lastOnsetMs = nowMs;

  // 6. PLL beat phase
  audio.beatFired = false;
  engBeatAccumMs += dt;
  if (engBeatAccumMs >= beatPeriodMsA) {
    engBeatAccumMs -= beatPeriodMsA;
    audio.beatFired = true;
    engBarAccumBeats = (engBarAccumBeats + 1.0) % 4.0;
  }
  if (onsetHit && audio.tempoConfidence > 0.4) {
    const ph = engBeatAccumMs / beatPeriodMsA;
    if (ph < 0.18) engBeatAccumMs *= 0.5;
    else if (ph > 0.82) engBeatAccumMs += (beatPeriodMsA - engBeatAccumMs) * 0.5;
  }
  audio.beatPhase = engBeatAccumMs / beatPeriodMsA;
  audio.barPhase = (engBarAccumBeats + audio.beatPhase) / 4.0;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `npx vitest run tests/audio-engine.test.ts`
Expected: PASS (5 tests). If the 120 BPM test is flaky on thresholds, loosen only the beat-count bound (≥ 20), never the engine constants.

- [ ] **Step 6: Commit**

```bash
git add sim/src/audio/ sim/tests/audio-engine.test.ts
git commit -m "feat(sim): port AudioBus and audio engine (bands, AGC, beat/tempo PLL)"
```

---

### Task 8: Static animations (8)

**Files:**
- Create: `sim/src/animations/statics.ts`
- Modify: `sim/src/main.ts` (render statics instead of the temp pattern)
- Reference: `src/animations.h:18-419` (read-only)

**Interfaces:**
- Consumes: `voxels`, `graphL`, `graphR`, `NUM_LEDS`, `NUM_EDGES`, `LEDS_PER_EDGE` from `geometry.ts`; `applyPalette` from `palettes.ts`; `clamp`, `qadd8`, `randomInt`, `millis`, `inoise8`, `fillBlack`, `CRGB` from `fastled.ts`; `registerAnimations`, `AnimFunc` from `engine.ts`.
- Produces: module side-effect `registerAnimations('static', [...8 anims], ['DiagFlow','Lissajous','GravPart','RD','MobiusBraid','Plasma','NoiseWorms','Coral'])` — order and names exactly as `animations.h:1062-1065` (names list has RD at index 3 and Coral at index 7; the firmware anim array order matching those names is authoritative).

Every animation writes colors via `applyPalette(v)` where `v` is the computed 0..1 intensity, exactly like firmware. Apply the **C++ → TypeScript porting rules** table (top of this plan) mechanically. Two ports are given in full below as the worked patterns (pure-field and stateful-simulation); the remaining six are ported the same way from their cited lines.

- [ ] **Step 1: Create statics.ts with the two worked ports**

`sim/src/animations/statics.ts` (start):
```ts
// Ports of the STATIC animations from src/animations.h. Source lines are
// cited per function — the firmware is the source of truth.
import { voxels, graphL, graphR, NUM_LEDS, NUM_EDGES, LEDS_PER_EDGE } from '../geometry';
import { applyPalette } from '../palettes';
import { CRGB, clamp, qadd8, randomInt, millis, inoise8, fillBlack } from '../fastled';
import { registerAnimations, type AnimFunc } from '../engine';

// ── Static0: Diagonal Flow — animations.h:19-31 ────────────────────
const staticDiagonalFlow: AnimFunc = (buf, t) => {
  const invSqrt3 = 0.57735;
  const speed = 0.4, width = 0.18;
  const bandPos = (t * speed) % 1.0;
  for (let i = 0; i < NUM_LEDS; i++) {
    const h = (voxels[i].x + voxels[i].y + voxels[i].z) * invSqrt3;
    const d0 = Math.abs(h - bandPos);
    const d1 = Math.abs(h - bandPos + 1.0);
    const d2 = Math.abs(h - bandPos - 1.0);
    const d = Math.min(d0, Math.min(d1, d2));
    const c = applyPalette(Math.exp((-d * d) / (width * width) * 4.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Static3: Reaction Diffusion — animations.h:235-297 ─────────────
const RD_DA = 0.2, RD_DB = 0.08, RD_FEED = 0.023, RD_KILL = 0.049;
const RD_DT = 0.25, RD_STEPS = 6;
const rdA = new Array(NUM_LEDS).fill(1.0);
const rdB = new Array(NUM_LEDS).fill(0.0);
const rdA2 = new Array(NUM_LEDS).fill(0.0);
const rdB2 = new Array(NUM_LEDS).fill(0.0);
let rdReady = false;
let rdLastNudge = 0, rdLastRandSeed = 0;

function rdSeedEdges(): void {
  for (let i = 0; i < NUM_LEDS; i++) { rdA[i] = 1.0; rdB[i] = 0.0; }
  for (let e = 0; e < NUM_EDGES; e++) {
    const base = e * LEDS_PER_EDGE, last = base + LEDS_PER_EDGE - 1;
    const seeds = 2 + randomInt(3);
    for (let s = 0; s < seeds; s++) {
      const cx = base + 3 + randomInt(LEDS_PER_EDGE - 6);
      for (let d = -3; d <= 3; d++) {
        const idx = clamp(cx + d, base, last);
        rdA[idx] = 0.3; rdB[idx] = 0.4;
      }
    }
  }
}

function rdNudgeIfDead(): void {           // animations.h:258-274
  const now = millis();
  if (now - rdLastRandSeed >= 10000) {
    rdLastRandSeed = now;
    const e = randomInt(NUM_EDGES), base = e * LEDS_PER_EDGE;
    const cx = base + 3 + randomInt(LEDS_PER_EDGE - 6);
    for (let d = -2; d <= 2; d++) {
      const idx = clamp(cx + d, base, base + LEDS_PER_EDGE - 1);
      rdA[idx] = 0.3; rdB[idx] = 0.4;
    }
  }
  if (now - rdLastNudge < 3000) return;
  rdLastNudge = now;
  let total = 0;
  for (let i = 0; i < NUM_LEDS; i++) total += rdB[i];
  if (total < 1.5) {
    for (let e = 0; e < NUM_EDGES; e++) {
      const base = e * LEDS_PER_EDGE;
      const cx = base + 3 + randomInt(LEDS_PER_EDGE - 6);
      for (let d = -2; d <= 2; d++) {
        const idx = clamp(cx + d, base, base + LEDS_PER_EDGE - 1);
        rdA[idx] = 0.3; rdB[idx] = 0.4;
      }
    }
  }
}

function rdStepAll(): void {               // animations.h:278-290
  for (let i = 0; i < NUM_LEDS; i++) {
    const a = rdA[i], b = rdB[i];
    const L = graphL[i], R = graphR[i];
    const lapA = rdA[L] + rdA[R] - 2.0 * a;
    const lapB = rdB[L] + rdB[R] - 2.0 * b;
    const rxn = a * b * b;
    rdA2[i] = clamp(a + RD_DT * (RD_DA * lapA - rxn + RD_FEED * (1.0 - a)), 0, 1);
    rdB2[i] = clamp(b + RD_DT * (RD_DB * lapB + rxn - (RD_KILL + RD_FEED) * b), 0, 1);
  }
  for (let i = 0; i < NUM_LEDS; i++) { rdA[i] = rdA2[i]; rdB[i] = rdB2[i]; }
}

const staticReactionDiffusion: AnimFunc = (buf, _t) => {
  if (!rdReady) { rdSeedEdges(); rdReady = true; }
  rdNudgeIfDead();
  for (let s = 0; s < RD_STEPS; s++) rdStepAll();
  for (let i = 0; i < NUM_LEDS; i++) {
    const c = applyPalette(clamp(rdB[i] * 3.5, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};
```

- [ ] **Step 2: Port the remaining six statics from their cited source**

Port each function below from `src/animations.h` into the same file, applying the porting-rules table. Per-animation notes (state vars become module-scope `let`; everything else is mechanical):

1. **`staticLissajous` — `animations.h:51-121`.** State: `lissHistory: {x,y,z}[80]`, `lissHead`, `lissInited`. Constants `LISS_HISTORY=80`, `LISS_HALO=0.12`. Uses `fillBlack(buf)` then additive `qadd8` per channel — keep the `d2 > halo2*4` early-skip and the quadratic fade.
2. **`staticGravityParticles` — `animations.h:129-231`.** State: `gravP[24]` particle structs, `gravInited`, `gravLastSpawn`, `gravLastT`. `millis()` drives spawn timing (900 ms). Keep dt cap `(dt<=0 || dt>0.1) → 0.016`. `random(30)-15` → `randomInt(30)-15`. Additive `qadd8` render with `d2 > 0.04` skip.
3. **`staticMobiusBraid` — `animations.h:351-388`.** Stateless. Three wound waves, quartic sharpening, smoothstep lift — direct math port.
4. **`staticPlasmaCube` — `animations.h:393-403`.** Stateless. Direct math port.
5. **`staticNoiseWorms` — `animations.h:410-419`.** Stateless except `ti = Math.floor(t*40)`. `inoise8(voxels[i].x*160 + ti, voxels[i].y*160 + ti/2, voxels[i].z*160)` — note firmware truncates `ti/2` to integer (uint16 cast); use `Math.floor(ti/2)`. Gamma: `v = v*v`.
6. **`staticCoral` — `animations.h:299-348`.** Same structure as RD but with its own fields `coralA/coralB/coralA2/coralB2`, constants `CORAL_FEED=0.0545`, `CORAL_KILL=0.0620`, and the 3 s reseed watchdog (`total < 1.5 → coralSeed()`); seed function is `animations.h:314-324`.

- [ ] **Step 3: Register the animations (end of statics.ts)**

```ts
registerAnimations('static',
  [staticDiagonalFlow, staticLissajous, staticGravityParticles,
   staticReactionDiffusion, staticMobiusBraid, staticPlasmaCube,
   staticNoiseWorms, staticCoral],
  ['DiagFlow', 'Lissajous', 'GravPart', 'RD', 'MobiusBraid',
   'Plasma', 'NoiseWorms', 'Coral']);
```

- [ ] **Step 4: Switch main.ts to the engine**

Replace the temp pattern in `sim/src/main.ts`:
```ts
import './animations/statics';
import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { renderFrame, selectAnimation } from './engine';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

selectAnimation('static', 0);

// 45 FPS cap to match TARGET_FPS (config.h) — animation speeds are tuned to it
const FRAME_MS = 1000 / 45;
let lastFrame = 0;
function frame(nowMs: number) {
  requestAnimationFrame(frame);
  if (nowMs - lastFrame < FRAME_MS) return;
  lastFrame = nowMs;
  renderFrame(buf, nowMs * 0.001);
  cube.setColors(buf);
  cube.render();
}
requestAnimationFrame(frame);
```

- [ ] **Step 5: Verify each animation visually**

Run: `npm run dev`. Temporarily change the `selectAnimation('static', N)` index 0→7 (or use the browser console once Task 12's UI exists — for now, edit + hot-reload). Check against firmware intent:
- DiagFlow: a soft band sweeping corner-to-corner diagonally
- Lissajous: a bright tracer with a fading trail weaving through the volume
- GravPart: particles falling and bouncing, dimming out
- RD: living spots/waves crawling along edges and THROUGH corners
- MobiusBraid: three interleaved bright ridges braiding around the cube
- Plasma: smooth multi-wave interference over the whole frame
- NoiseWorms: worm-like bright crests flowing continuously across corners
- Coral: branching maze/labyrinth texture, denser than RD

- [ ] **Step 6: Verify tests + build**

Run: `npm test && npm run build`
Expected: all pass, build clean.

- [ ] **Step 7: Commit**

```bash
git add sim/src/animations/statics.ts sim/src/main.ts
git commit -m "feat(sim): port all 8 static animations"
```

---

### Task 9: Audio sources — system audio, demo track, synthetic

**Files:**
- Create: `sim/src/audio/sources.ts`
- Create: `sim/public/demo-track.mp3` (royalty-free — see Step 1)
- Modify: `sim/src/main.ts` (drive `audioEngineUpdate` each frame)

**Interfaces:**
- Consumes: `audioEngineUpdate` from `audio/engine.ts`.
- Produces:
  - `type SourceKind = 'none' | 'demo' | 'system' | 'synthetic'`
  - `startDemoTrack(): Promise<void>` — plays `/demo-track.mp3` (looped) through an `AnalyserNode` and speakers
  - `startSystemAudio(): Promise<void>` — `getDisplayMedia({video: true, audio: true})`; stops the video track immediately; routes the audio track to the analyser ONLY (not to speakers — it is already playing on the system). Throws with a friendly message if the user shared without audio.
  - `startSynthetic(): void` — no AudioContext; a code-generated FFT-frame producer (120 BPM kick pattern + mid/high texture)
  - `stopSource(): void`
  - `getSourceKind(): SourceKind`
  - `updateAudioFrame(nowMs: number): void` — called once per render frame: pulls the analyser FFT (or synthetic frame) into a 512-bin `Float32Array` shaped like firmware `fftBins` (~43 Hz/bin) and calls `audioEngineUpdate(bins, nowMs)`

- [ ] **Step 1: Add the demo track**

Download one royalty-free electronic track with a clear beat (CC0). Source: https://pixabay.com/music/ (CC0, no attribution required) — pick any ~2-min electronic/house track with strong kicks, save as `sim/public/demo-track.mp3`. Keep it under ~3 MB. Note the track title + page URL in a comment at the top of `sources.ts` for provenance.

- [ ] **Step 2: Implement sources.ts**

`sim/src/audio/sources.ts`:
```ts
// Audio sources — all feed a 512-bin frame shaped like the firmware's
// fftBins (FFT1024 @ 44.1 kHz → ~43 Hz/bin) into audioEngineUpdate.
// Demo track: <TRACK TITLE + URL HERE — fill in when downloading>
import { audioEngineUpdate } from './engine';

export type SourceKind = 'none' | 'demo' | 'system' | 'synthetic';

let ctx: AudioContext | null = null;
let analyser: AnalyserNode | null = null;
let mediaEl: HTMLAudioElement | null = null;
let mediaStream: MediaStream | null = null;
let kind: SourceKind = 'none';

const bins = new Float32Array(512);
let freqData: Uint8Array | null = null;

export function getSourceKind(): SourceKind { return kind; }

function makeAnalyser(): AnalyserNode {
  ctx = ctx ?? new AudioContext();
  const a = ctx.createAnalyser();
  a.fftSize = 2048;                  // 1024 frequency bins
  a.smoothingTimeConstant = 0.0;     // firmware engine does its own smoothing
  freqData = new Uint8Array(a.frequencyBinCount);
  return a;
}

export function stopSource(): void {
  mediaEl?.pause();
  mediaEl = null;
  mediaStream?.getTracks().forEach((tr) => tr.stop());
  mediaStream = null;
  analyser = null;
  kind = 'none';
  bins.fill(0);
}

export async function startDemoTrack(): Promise<void> {
  stopSource();
  analyser = makeAnalyser();
  mediaEl = new Audio('demo-track.mp3');
  mediaEl.loop = true;
  mediaEl.crossOrigin = 'anonymous';
  const src = ctx!.createMediaElementSource(mediaEl);
  src.connect(analyser);
  analyser.connect(ctx!.destination);   // demo track IS audible
  await ctx!.resume();
  await mediaEl.play();
  kind = 'demo';
}

export async function startSystemAudio(): Promise<void> {
  const stream = await navigator.mediaDevices.getDisplayMedia({
    video: true,               // required by the API even though we discard it
    audio: true,
  });
  stream.getVideoTracks().forEach((tr) => tr.stop());
  if (stream.getAudioTracks().length === 0) {
    stream.getTracks().forEach((tr) => tr.stop());
    throw new Error(
      'No audio was shared. Choose "Entire Screen" and tick "Also share system audio", or share a tab with audio.');
  }
  stopSource();
  mediaStream = stream;
  analyser = makeAnalyser();
  const src = ctx!.createMediaStreamSource(stream);
  src.connect(analyser);      // analyser only — do NOT route to speakers (echo)
  await ctx!.resume();
  kind = 'system';
  // if the user clicks the browser's "stop sharing" bar, fall back cleanly
  stream.getAudioTracks()[0].addEventListener('ended', () => stopSource());
}

export function startSynthetic(): void {
  stopSource();
  kind = 'synthetic';
}

// ── Per-frame pump ──────────────────────────────────────────────────
export function updateAudioFrame(nowMs: number): void {
  if (kind === 'none') {
    bins.fill(0);
  } else if (kind === 'synthetic') {
    syntheticFrame(nowMs);
  } else if (analyser && freqData) {
    // AnalyserNode: 1024 bins over 0..sampleRate/2. Firmware: 512 bins over
    // 0..~22 kHz at 43.07 Hz/bin. Resample by frequency so band edges land
    // on the same Hz as the firmware's BAND_LO/HI tables.
    analyser.getByteFrequencyData(freqData);
    const hzPerAnalyserBin = ctx!.sampleRate / analyser.fftSize;
    for (let i = 0; i < 512; i++) {
      const hz = i * 43.066;                          // firmware bin center
      const j = Math.min(Math.round(hz / hzPerAnalyserBin), freqData.length - 1);
      bins[i] = freqData[j] / 255;                    // 0..1 magnitude
    }
  }
  audioEngineUpdate(bins, nowMs);
}

// 120 BPM kick + offbeat hats + mid wash, shaped directly into bins.
function syntheticFrame(nowMs: number): void {
  bins.fill(0);
  const beatMs = 500;                                  // 120 BPM
  const ph = (nowMs % beatMs) / beatMs;                // 0..1 within beat
  const kick = Math.exp(-ph * 14);                     // sharp decay after beat
  const bar = Math.floor(nowMs / beatMs) % 4;
  for (let i = 1; i <= 4; i++) bins[i] = 0.85 * kick;  // sub+bass bins
  const hatPh = ((nowMs + beatMs / 2) % beatMs) / beatMs;
  const hat = Math.exp(-hatPh * 20) * (bar === 3 ? 1.0 : 0.6);
  for (let i = 116; i <= 250; i++) bins[i] = 0.5 * hat;
  const wash = 0.15 + 0.1 * Math.sin(nowMs * 0.0007);
  for (let i = 9; i <= 58; i++) bins[i] = wash + 0.25 * kick;
}
```

- [ ] **Step 3: Pump audio in main.ts**

In `sim/src/main.ts` add to imports: `import { updateAudioFrame, startSynthetic } from './audio/sources';`
Inside `frame()` before `renderFrame(...)` add: `updateAudioFrame(nowMs);`
After `selectAnimation(...)` add: `startSynthetic();` (temporary default until Task 12's UI offers the real choices).

- [ ] **Step 4: Verify**

Run: `npm test && npm run build`, then `npm run dev`.
In the browser console check the bus reacts: switch `selectAnimation('static', 0)` in main.ts to `('audio', 0)` — the placeholder audio anim pulses (audio anims arrive in Task 10). No console errors.

- [ ] **Step 5: Commit**

```bash
git add sim/src/audio/sources.ts sim/src/main.ts sim/public/demo-track.mp3
git commit -m "feat(sim): audio sources — system capture, demo track, synthetic"
```

---

### Task 10: Audio animations (10)

**Files:**
- Create: `sim/src/animations/audios.ts`
- Modify: `sim/src/main.ts` (add `import './animations/audios';`)
- Reference: `src/animations.h:425-935` (read-only)

**Interfaces:**
- Consumes: everything statics consumed, plus `audio` from `audio/bus.ts`, `EQUATOR_VERTS`, `CUBE_CORNERS` from `geometry.ts`, and `graphBlur` (defined in this task).
- Produces: `registerAnimations('audio', [...10], ['TriAxis','Impact','CellAuto','BassBloom','Vortex','PulseWeb','SpectrHlix','Earthquake','Flame','Dendrite'])` — exactly `animations.h:1066-1069`.
- Also produces (top of file, shared): `graphBlur(field: number[], passes?: number, amount?: number): void` — port of `cube.h:165-173` (graph Laplacian smoothing used by Flame).

One worked port below (TriAxis — the audio-bus consumption pattern); the other nine follow identically from their cited lines using the porting-rules table.

- [ ] **Step 1: Create audios.ts with graphBlur + the worked TriAxis port**

`sim/src/animations/audios.ts` (start):
```ts
// Ports of the AUDIO animations from src/animations.h:425-935.
import { voxels, graphL, graphR, NUM_LEDS, EQUATOR_VERTS, CUBE_CORNERS } from '../geometry';
import { applyPalette } from '../palettes';
import { CRGB, clamp, qadd8, randomInt, millis, inoise8, fillBlack } from '../fastled';
import { registerAnimations, type AnimFunc } from '../engine';
import { audio, NUM_BANDS } from '../audio/bus';

// Port of graphBlur (cube.h:165-173) — smear a scalar field along the
// welded edge graph so it flows across corners.
const graphScratch = new Array(NUM_LEDS).fill(0);
export function graphBlur(field: number[], passes = 1, amount = 0.5): void {
  for (let p = 0; p < passes; p++) {
    for (let i = 0; i < NUM_LEDS; i++) {
      const lap = field[graphL[i]] + field[graphR[i]] - 2.0 * field[i];
      graphScratch[i] = field[i] + amount * lap;
    }
    for (let i = 0; i < NUM_LEDS; i++) field[i] = graphScratch[i];
  }
}

// ── Audio0: Tri Axis — animations.h:429-469 ─────────────────────────
let burstX = 0.5, burstY = 0.5, burstZ = 0.5, burstEnv = 0.0, lastHigh = 0.0;

const audioTriAxis: AnimFunc = (buf, _t) => {
  const invSqrt3 = 0.57735;

  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, beatPhase, tempoConfidence } = audio;

  if (beatFired && tempoConfidence > 0.25) {
    const v = EQUATOR_VERTS[randomInt(6)];
    burstX = v.x; burstY = v.y; burstZ = v.z;
    burstEnv = 0.6 + bass * 0.6;
  } else if (high > lastHigh * 1.35 && high > 0.04 && burstEnv < 0.2) {
    const v = EQUATOR_VERTS[randomInt(6)];
    burstX = v.x; burstY = v.y; burstZ = v.z;
    burstEnv = 1.0;
  }
  lastHigh = high; burstEnv *= 0.88;

  const beatMod = tempoConfidence > 0.25 ? 1.0 + 0.4 * (1.0 - beatPhase) : 1.0;

  for (let i = 0; i < NUM_LEDS; i++) {
    const { x, y, z } = voxels[i];
    const h = (x + y + z) * invSqrt3;
    const bW = (0.15 + bass * 0.6) * beatMod;
    const bassV = bass * 2.5 * Math.exp(-(h * h) / (bW * bW));
    const mW = 0.15 + mid * 0.6, mH = 1.0 - h;
    const midV = mid * 2.5 * Math.exp(-(mH * mH) / (mW * mW));
    const dx = x - burstX, dy = y - burstY, dz = z - burstZ;
    const highV = burstEnv * Math.exp(-(dx * dx + dy * dy + dz * dz) * 8.0);
    const c = applyPalette(clamp(bassV + midV + highV, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};
```

- [ ] **Step 2: Port the remaining nine from their cited source**

All read the bus with the same driver pattern as TriAxis (`bass`/`mid`/`high` + beat fields). State vars → module-scope `let`. Per-animation notes:

1. **`audioImpact` — `animations.h:475-537`.** State: `shocks[4]` `{radius,speed,cx,cy,cz,env,active}`, `shockLastBass`, `highLast`, `shocksInited`, plus per-LED `sparkEnvs: number[384]` (firmware global, `globals.h:46` — make it module-local here). Note the noise calls use `mid*180` inside the x-arg and `t*18`/`t*15` time terms.
2. **`audioCell` — `animations.h:543-604`.** State: `cellState[384]`, `cellNext[384]`, `cellLastBass`, `cellLastHigh`, `cellInited`. Uses `graphL/graphR` neighbor seeding + infection spread; barPhase breathing (`0.85 + 0.3*sin(barPhase*2π)`).
3. **`audioBassBloom` — `animations.h:612-647`.** State: `bloomRadius`, `bloomEnv`, `bloomLastBass`. `beatPeriodMs = 60000/audio.bpm`; tempo-matched `expandSpeed`. Noise spark: `inoise8(i*13, t*90)`.
4. **`audioVortex` — `animations.h:653-687`.** Stateless. Tempo-locked spin (`bpm/60` rev/s at confidence > 0.5); `armWidth` floor `0.01`; guard `r2 < 1e-6`.
5. **`audioPulseWeb` — `animations.h:695-754`.** State: `pwebs[8]` `{active,radius,env,speed,cx,cy,cz}`, `pwebLastBass`, `pwebInited`. Spawn corners from `CUBE_CORNERS`; tempo-matched speed like BassBloom.
6. **`audioSpectrumHelix` — `animations.h:760-798`.** State: `helixFlare`. Uses `audio.band[band]` where `band = clamp(Math.floor(z * NUM_BANDS), 0, NUM_BANDS-1)`, plus `audio.level` for ambient noise floor. Note the double-angle wrap: `dTheta > 0.5 → 1 - dTheta`.
7. **`audioEarthquake` — `animations.h:804-840`.** State: `quakeLastBass`, `quakeRumbleEnv`. barPhase tension `0.5 + 0.5*sin(barPhase*π)`; z-weighted upward sparks.
8. **`audioFlame` — `animations.h:846-880`.** State: `flameHeat[384]`, `flameInit`, `flameLastBass`. Uses `graphBlur(flameHeat, 1, 0.28)`; cool factor `0.90 - clamp(mid*0.03, 0, 0.05)`; heat clamp allows overshoot to 1.5 before render clamp. Ember condition: `randomInt(100) < Math.floor(mid*40)`.
9. **`audioDendrite` — `animations.h:887-935`.** State: `dendTips[24]` `{active,node,fwd,life,env}`, `dendCharge[384]`, `dendInit`, plus `dendSpawn(node, fwd, env, life)` helper. Tips walk `graphR`/`graphL`; fork chance `randomInt(100) < 18` while `life > 3`; charge decay `0.80`.

- [ ] **Step 3: Register (end of audios.ts)**

```ts
registerAnimations('audio',
  [audioTriAxis, audioImpact, audioCell, audioBassBloom, audioVortex,
   audioPulseWeb, audioSpectrumHelix, audioEarthquake, audioFlame, audioDendrite],
  ['TriAxis', 'Impact', 'CellAuto', 'BassBloom', 'Vortex',
   'PulseWeb', 'SpectrHlix', 'Earthquake', 'Flame', 'Dendrite']);
```

Add `import './animations/audios';` to `sim/src/main.ts`.

- [ ] **Step 4: Verify each visually against the synthetic source**

Run: `npm run dev`, set `selectAnimation('audio', N)` for N = 0..9 (synthetic 120 BPM source from Task 9 is active). Expect, in time with the synthetic beat:
- TriAxis: bass blob at origin corner pulsing on beat, bursts at vertices
- Impact: expanding shockwave spheres on each kick + high-freq sparkles
- CellAuto: infection blooms seeding on beats, breathing over 4-beat bars
- BassBloom: sphere expanding from center each beat
- Vortex: spiral arms spinning one rev/beat once confidence locks
- PulseWeb: rings expanding from random corners on beats
- SpectrHlix: rotating helix stripe; bottom lights with kick, top with hats
- Earthquake: ground wave rising on beats + shudder texture
- Flame: heat splashing on beats, crawling along edges through corners
- Dendrite: lightning walkers forking around the wireframe on beats

- [ ] **Step 5: Verify tests + build; commit**

Run: `npm test && npm run build` — all pass.

```bash
git add sim/src/animations/audios.ts sim/src/main.ts
git commit -m "feat(sim): port all 10 audio animations"
```

---

### Task 11: Voice animations (4)

**Files:**
- Create: `sim/src/animations/voices.ts`
- Modify: `sim/src/main.ts` (add `import './animations/voices';`)
- Reference: `src/animations.h:941-1056` (read-only)

**Interfaces:**
- Consumes: same as audios (bus fields used: `speech`, `syllableOnset`, `sylEnv`, `level`, `presence()`, `brilliance()`).
- Produces: `registerAnimations('voice', [...4], ['Breathe','Formant','Rings','Sparks'])` — first four of `animations.h:1070-1073`.

One worked port below (SyllableSparks — the graph-walker pattern); the other three are simpler and ported from their cited lines.

- [ ] **Step 1: Create voices.ts with the worked SyllableSparks port**

`sim/src/animations/voices.ts` (start):
```ts
// Ports of the VOICE animations from src/animations.h:941-1056.
import { voxels, graphL, graphR, NUM_LEDS } from '../geometry';
import { applyPalette } from '../palettes';
import { CRGB, clamp, qadd8, randomInt, inoise8 } from '../fastled';
import { registerAnimations, type AnimFunc } from '../engine';
import { audio } from '../audio/bus';

// ── Voice3: Syllable Sparks — animations.h:996-1056 ────────────────
// Sparks walk the welded edge graph, flowing across corners.
const SSPARK_COUNT = 14, SSPARK_TAIL = 7;
interface GraphSpark { active: boolean; node: number; fwd: boolean; accum: number; speed: number; env: number; }
const ssparks: GraphSpark[] = Array.from({ length: SSPARK_COUNT },
  () => ({ active: false, node: 0, fwd: true, accum: 0, speed: 0, env: 0 }));

function fireSyllableSparks(speechEnergy: number, sylEnv: number): void {
  const want = 2 + Math.floor(speechEnergy * 4.0);
  let spawned = 0;
  for (let s = 0; s < SSPARK_COUNT && spawned < want; s++) {
    if (!ssparks[s].active) {
      ssparks[s] = {
        active: true, node: randomInt(NUM_LEDS), fwd: randomInt(2) === 0,
        accum: 0.0, speed: 0.4 + speechEnergy * 0.9, env: 0.7 + sylEnv * 0.5,
      };
      spawned++;
    }
  }
}

const voiceSyllableSparks: AnimFunc = (buf, t) => {
  const { syllableOnset, sylEnv } = audio;
  const speechEnergy = audio.speech;
  const high = audio.presence() + audio.brilliance();
  const voiceLevel = audio.level;

  if (syllableOnset) fireSyllableSparks(speechEnergy, sylEnv);

  // Ambient shimmer sampled at 3D positions — no per-edge seams.
  const ti = Math.floor(t * 80.0);
  for (let i = 0; i < NUM_LEDS; i++) {
    const n = inoise8(voxels[i].x * 140.0 + ti, voxels[i].y * 140.0, voxels[i].z * 140.0);
    const c = applyPalette((n / 255) * voiceLevel * 0.35);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }

  // Advance + render each spark with a fading tail trailing behind it.
  for (let s = 0; s < SSPARK_COUNT; s++) {
    const sp = ssparks[s];
    if (!sp.active) continue;
    sp.accum += sp.speed;
    while (sp.accum >= 1.0) {
      sp.node = sp.fwd ? graphR[sp.node] : graphL[sp.node];
      sp.accum -= 1.0;
    }
    sp.env *= 0.94;
    if (sp.env < 0.03) { sp.active = false; continue; }

    let n = sp.node, b = sp.env;
    for (let k = 0; k < SSPARK_TAIL; k++) {
      const br = b + (k === 0 ? high * 1.5 : 0.0);
      const c = applyPalette(clamp(br, 0, 1));
      buf[n].r = qadd8(buf[n].r, c.r);
      buf[n].g = qadd8(buf[n].g, c.g);
      buf[n].b = qadd8(buf[n].b, c.b);
      n = sp.fwd ? graphL[n] : graphR[n];   // tail trails behind the head
      b *= 0.6;
    }
  }
};
```

- [ ] **Step 2: Port the remaining three from their cited source**

1. **`voiceBreathe` — `animations.h:942-952`.** Stateless. Drivers: `audio.speech`, `high = presence()+brilliance()`. Slow sine breath + speech swell + tip flash weighted by diagonal height.
2. **`voiceFormant` — `animations.h:955-973`.** State: `posA=0.15, posB=0.85, velA=0, velB=0` (spring-driven band positions). `syllableOnset` kicks velocities; two gaussian bands + between-fill.
3. **`voiceHarmonicRings` — `animations.h:977-994`.** State: `hrings[6]` `{radius,env,speed,active}`, `hringInited`. Rings expand from center on syllables; note the asymmetric falloff (`dr<0 → exp(-dr²*60)` else `exp(-dr²*180)*1.4`).

- [ ] **Step 3: Register (end of voices.ts)**

```ts
registerAnimations('voice',
  [voiceBreathe, voiceFormant, voiceHarmonicRings, voiceSyllableSparks],
  ['Breathe', 'Formant', 'Rings', 'Sparks']);
```

Add `import './animations/voices';` to `sim/src/main.ts`.

- [ ] **Step 4: Verify visually**

Run: `npm run dev` with `selectAnimation('voice', N)`. The synthetic source has weak speech-band energy — for a proper check, start the demo track (temporarily call `startDemoTrack()` from a click handler, or wait for Task 12 UI) or talk into system audio. Expect: Breathe swells with vocals; Formant bands converge on syllables; Rings pulse outward per syllable; Sparks streak around edges and corners.

- [ ] **Step 5: Verify tests + build; commit**

Run: `npm test && npm run build` — all pass.

```bash
git add sim/src/animations/voices.ts sim/src/main.ts
git commit -m "feat(sim): port all 4 voice animations"
```

---

### Task 12: UI overlay + final wiring

**Files:**
- Create: `sim/src/ui.ts`
- Modify: `sim/src/main.ts` (final form), `sim/index.html` (UI styles)

**Interfaces:**
- Consumes: `getState`, `selectAnimation`, `setBrightness`, `setDemo`, `getAnimNames`, `Mode` from `engine.ts`; `startPaletteFade`, `PALETTE_NAMES`, `getPaletteIndex` from `palettes.ts`; `startDemoTrack`, `startSystemAudio`, `startSynthetic`, `stopSource`, `getSourceKind` from `audio/sources.ts`.
- Produces: `initUI(): void` — builds the overlay DOM, wires events, keeps controls in sync (call `refresh()` after any engine-driven change; demo-reel changes are picked up by a 500 ms `setInterval` refresh).

- [ ] **Step 1: Add UI styles to index.html**

Add inside `<style>` in `sim/index.html`:
```css
#ui {
  position: fixed; top: 12px; left: 12px; z-index: 10;
  font: 13px/1.5 system-ui, sans-serif; color: #cfd3dc;
  background: rgba(10, 10, 16, 0.72); border: 1px solid #23232e;
  border-radius: 10px; padding: 12px 14px; backdrop-filter: blur(6px);
  transition: opacity 0.4s; user-select: none; max-width: 240px;
}
#ui.faded { opacity: 0.08; }
#ui:hover { opacity: 1; }
#ui label { display: block; margin-top: 8px; font-size: 11px; text-transform: uppercase; letter-spacing: 0.08em; color: #8a8fa3; }
#ui select, #ui input[type=range] { width: 100%; margin-top: 2px; }
#ui select { background: #16161f; color: #cfd3dc; border: 1px solid #2a2a38; border-radius: 6px; padding: 4px; }
#ui .row { display: flex; gap: 6px; margin-top: 10px; }
#ui button {
  flex: 1; background: #1b1b28; color: #cfd3dc; border: 1px solid #2e2e40;
  border-radius: 6px; padding: 6px 4px; cursor: pointer; font-size: 12px;
}
#ui button.active { background: #2d2d4a; border-color: #4a4a7a; color: #fff; }
#ui .err { color: #e08a8a; font-size: 11px; margin-top: 6px; min-height: 14px; }
```

- [ ] **Step 2: Implement ui.ts**

`sim/src/ui.ts`:
```ts
import { getState, selectAnimation, setBrightness, setDemo, getAnimNames, type Mode } from './engine';
import { startPaletteFade, PALETTE_NAMES, getPaletteIndex } from './palettes';
import { startDemoTrack, startSystemAudio, startSynthetic, stopSource, getSourceKind } from './audio/sources';

const MODES: Mode[] = ['static', 'audio', 'voice'];

export function initUI(): void {
  const ui = document.createElement('div');
  ui.id = 'ui';
  ui.innerHTML = `
    <strong>LED Infinity Cube</strong>
    <label>Mode</label><select id="mode"></select>
    <label>Animation</label><select id="anim"></select>
    <label>Palette</label><select id="pal"></select>
    <label>Brightness</label><input id="bright" type="range" min="5" max="255" step="1">
    <div class="row">
      <button id="demoReel">Auto-cycle</button>
    </div>
    <label>Audio source</label>
    <div class="row">
      <button id="srcDemo">Demo track</button>
      <button id="srcSystem">System audio</button>
    </div>
    <div class="row">
      <button id="srcSynth">Synthetic</button>
      <button id="srcNone">Silent</button>
    </div>
    <div class="err" id="err"></div>
  `;
  document.body.appendChild(ui);

  const modeSel = ui.querySelector<HTMLSelectElement>('#mode')!;
  const animSel = ui.querySelector<HTMLSelectElement>('#anim')!;
  const palSel = ui.querySelector<HTMLSelectElement>('#pal')!;
  const bright = ui.querySelector<HTMLInputElement>('#bright')!;
  const demoBtn = ui.querySelector<HTMLButtonElement>('#demoReel')!;
  const err = ui.querySelector<HTMLDivElement>('#err')!;

  MODES.forEach((m) => modeSel.add(new Option(m.toUpperCase(), m)));
  PALETTE_NAMES.forEach((n, i) => palSel.add(new Option(n, String(i))));

  function fillAnims(mode: Mode): void {
    animSel.innerHTML = '';
    getAnimNames(mode).forEach((n, i) => animSel.add(new Option(n, String(i))));
  }

  function refresh(): void {
    const s = getState();
    modeSel.value = s.mode;
    if (animSel.options.length !== getAnimNames(s.mode).length ||
        animSel.options[0]?.text !== getAnimNames(s.mode)[0]) fillAnims(s.mode);
    animSel.value = String(s.index);
    palSel.value = String(getPaletteIndex());
    bright.value = String(s.brightness);
    demoBtn.classList.toggle('active', s.demo);
    ui.classList.toggle('faded', s.demo);
    const kind = getSourceKind();
    ui.querySelector('#srcDemo')!.classList.toggle('active', kind === 'demo');
    ui.querySelector('#srcSystem')!.classList.toggle('active', kind === 'system');
    ui.querySelector('#srcSynth')!.classList.toggle('active', kind === 'synthetic');
    ui.querySelector('#srcNone')!.classList.toggle('active', kind === 'none');
  }

  modeSel.onchange = () => { setDemo(false); fillAnims(modeSel.value as Mode); selectAnimation(modeSel.value as Mode, 0); refresh(); };
  animSel.onchange = () => { setDemo(false); selectAnimation(modeSel.value as Mode, Number(animSel.value)); refresh(); };
  palSel.onchange = () => { startPaletteFade(Number(palSel.value)); };
  bright.oninput = () => { setBrightness(Number(bright.value)); };
  demoBtn.onclick = () => { setDemo(!getState().demo); refresh(); };

  ui.querySelector<HTMLButtonElement>('#srcDemo')!.onclick = async () => {
    err.textContent = '';
    try { await startDemoTrack(); } catch (e) { err.textContent = String((e as Error).message); }
    refresh();
  };
  ui.querySelector<HTMLButtonElement>('#srcSystem')!.onclick = async () => {
    err.textContent = '';
    try { await startSystemAudio(); } catch (e) { err.textContent = String((e as Error).message); }
    refresh();
  };
  ui.querySelector<HTMLButtonElement>('#srcSynth')!.onclick = () => { startSynthetic(); refresh(); };
  ui.querySelector<HTMLButtonElement>('#srcNone')!.onclick = () => { stopSource(); refresh(); };

  setInterval(refresh, 500);   // pick up demo-reel changes
  refresh();
}
```

- [ ] **Step 3: Final main.ts**

Replace `sim/src/main.ts`:
```ts
import './animations/statics';
import './animations/audios';
import './animations/voices';
import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { renderFrame, selectAnimation, setDemo } from './engine';
import { updateAudioFrame, startSynthetic } from './audio/sources';
import { initUI } from './ui';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

// Showcase defaults: auto-cycle on, synthetic audio until the user picks
// a real source (browsers block autoplaying the demo track before a
// user gesture).
selectAnimation('static', 0);
setDemo(true);
startSynthetic();
initUI();

const FRAME_MS = 1000 / 45;   // TARGET_FPS parity (config.h)
let lastFrame = 0;
function frame(nowMs: number) {
  requestAnimationFrame(frame);
  if (nowMs - lastFrame < FRAME_MS) return;
  lastFrame = nowMs;
  updateAudioFrame(nowMs);
  renderFrame(buf, nowMs * 0.001);
  cube.setColors(buf);
  cube.render();
}
requestAnimationFrame(frame);
```

- [ ] **Step 4: Full manual test pass**

Run: `npm run dev`. Verify:
1. Page loads → cube auto-rotating, auto-cycle badge active, UI fades when idle, reappears on hover.
2. Mode/Animation/Palette/Brightness selects all work and kill auto-cycle (mode/anim) appropriately.
3. "Demo track" plays music and the audio anims dance to it.
4. "System audio" opens the share picker; sharing a tab with music works; cancelling shows the friendly error; clicking the browser's "Stop sharing" reverts to Silent without console errors.
5. Auto-cycle changes animation + palette every 30 s with a 2 s crossfade.
6. `npm test && npm run build` — everything passes.

- [ ] **Step 5: Commit**

```bash
git add sim/src/ui.ts sim/src/main.ts sim/index.html
git commit -m "feat(sim): UI overlay, auto-cycle default, audio source controls"
```

---

### Task 13: Deploy to GitHub Pages

**Files:**
- Create: `.github/workflows/deploy-sim.yml`
- Modify: `README.md` (add link once live)

**Interfaces:**
- Consumes: the `sim/` build from all prior tasks.
- Produces: a public URL `https://<user>.github.io/<repo>/` serving `sim/dist`.

- [ ] **Step 1: Create the workflow**

`.github/workflows/deploy-sim.yml`:
```yaml
name: Deploy sim to GitHub Pages

on:
  push:
    branches: [main]
    paths: ['sim/**', '.github/workflows/deploy-sim.yml']
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: true

jobs:
  build-deploy:
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
          cache-dependency-path: sim/package-lock.json
      - run: npm ci
        working-directory: sim
      - run: npm test
        working-directory: sim
      - run: npm run build
        working-directory: sim
      - uses: actions/configure-pages@v5
      - uses: actions/upload-pages-artifact@v3
        with:
          path: sim/dist
      - id: deployment
        uses: actions/deploy-pages@v4
```

- [ ] **Step 2: Enable Pages**

In the GitHub repo: Settings → Pages → Source: **GitHub Actions**. (One-time manual step — note it for the user; they must click this.)

- [ ] **Step 3: Push and verify**

Push to `main` (ask the user first if there are unrelated staged firmware changes — do not push those). Watch the Action complete, open the Pages URL in Chrome, and re-run the Task 12 Step 4 checklist against the live site (system audio requires HTTPS — Pages provides it).

- [ ] **Step 4: Add the link to README.md**

Add under the project intro: a `## Web simulator` section with the live URL and one line: interactive 3D simulation of all cube modes/animations; Chrome/Edge recommended for system-audio capture.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/deploy-sim.yml README.md
git commit -m "ci: deploy sim to GitHub Pages"
```

---

## Verification checklist (whole plan)

- [ ] `npm test` green: geometry, fastled, palettes, engine, audio-engine suites
- [ ] `npm run build` clean
- [ ] All 22 animations visually verified (Tasks 8/10/11 step lists)
- [ ] All three audio sources work; system audio survives "Stop sharing"
- [ ] Auto-cycle + crossfade + palette rotate behave like the firmware demo mode
- [ ] Live on GitHub Pages over HTTPS
- [ ] No firmware files modified (`git status` shows only `sim/`, `.github/`, `README.md`, docs)
