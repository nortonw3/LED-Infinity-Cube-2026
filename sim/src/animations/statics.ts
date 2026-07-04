// Ports of the STATIC animations from src/animations.h. Source lines are
// cited per function — the firmware is the source of truth.
//
// NOTE on NoiseWorms and Coral: the task brief (docs/superpowers/plans/
// 2026-07-04-led-cube-web-sim.md, Task 8) gives complete, self-contained
// formulas for these two that do not match the current firmware content at
// their cited line ranges (firmware's NoiseWorms is a simpler per-edge-index
// noise fill, animations.h:429-439, and firmware has no Coral animation at
// all). Both formulas below are ported verbatim from the brief's explicit
// spec rather than from firmware, since firmware has no matching source for
// them. Flagged for human review — see task-8-report.md.
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

// ── Static1: Lissajous Tracer — animations.h:43-121 ─────────────────
// Three oscillators at irrational frequency ratios drive a point through
// 3D space; a fading trail of history positions renders as an additive
// halo along the trail.
const LISS_HISTORY = 80;        // trail length in frames
const LISS_HALO = 0.12;         // radius in voxel space for LED illumination

interface LissPoint { x: number; y: number; z: number; }
const lissHistory: LissPoint[] =
  Array.from({ length: LISS_HISTORY }, () => ({ x: 0.5, y: 0.5, z: 0.5 }));
let lissHead = 0;
let lissInited = false;

const staticLissajous: AnimFunc = (buf, t) => {
  if (!lissInited) {
    for (let i = 0; i < LISS_HISTORY; i++) lissHistory[i] = { x: 0.5, y: 0.5, z: 0.5 };
    lissInited = true;
  }

  fillBlack(buf);

  // Oscillator frequencies — irrational ratios so the figure never repeats.
  // φ = 1.618..., δ = 1.324... (plastic constant)
  const fA = 1.000, fB = 1.618, fC = 1.324;
  const phA = 0.0, phB = 0.7854, phC = 1.5708; // 0, π/4, π/2
  const drift = t * 0.007; // slow drift — figure morphs over minutes

  let px = 0.5 + 0.50 * Math.sin(fA * t * 1.3 + phA + drift);
  let py = 0.5 + 0.50 * Math.sin(fB * t * 1.3 + phB + drift * 1.3);
  let pz = 0.5 + 0.50 * Math.sin(fC * t * 1.3 + phC + drift * 0.7);

  px = clamp(px, 0.0, 1.0);
  py = clamp(py, 0.0, 1.0);
  pz = clamp(pz, 0.0, 1.0);

  lissHistory[lissHead] = { x: px, y: py, z: pz };
  lissHead = (lissHead + 1) % LISS_HISTORY;

  const halo2 = LISS_HALO * LISS_HALO;
  for (let s = 0; s < LISS_HISTORY; s++) {
    const idx = ((lissHead - 1 - s) % LISS_HISTORY + LISS_HISTORY) % LISS_HISTORY;
    const age = s / LISS_HISTORY;
    let bright = 1.0 - age; // linear fade
    bright = bright * bright; // quadratic — head is much brighter
    if (bright < 0.01) continue;

    const hx = lissHistory[idx].x, hy = lissHistory[idx].y, hz = lissHistory[idx].z;

    for (let j = 0; j < NUM_LEDS; j++) {
      const dx = voxels[j].x - hx, dy = voxels[j].y - hy, dz = voxels[j].z - hz;
      const d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > halo2 * 4.0) continue;
      let b = bright * Math.exp(-d2 / (halo2 * 0.5));
      b = clamp(b, 0.0, 1.0);
      const c = applyPalette(b);
      buf[j].r = qadd8(buf[j].r, c.r);
      buf[j].g = qadd8(buf[j].g, c.g);
      buf[j].b = qadd8(buf[j].b, c.b);
    }
  }
};

// ── Static2: Gravity Particle System — animations.h:129-231 ─────────
// Full 3D physics — particles spawn near the top vertex, fall under
// gravity, bounce off the six cube faces with energy loss.
const GRAV_COUNT = 24;
const GRAV_GRAVITY = 0.35;       // units/s² downward (z axis)
const GRAV_RESTITUTION = 0.55;   // bounce energy retention
const GRAV_DRAG = 0.995;         // air resistance per frame
const GRAV_SPAWN_RATE = 900;     // ms between spawns

interface GravParticle {
  active: boolean;
  x: number; y: number; z: number;      // position 0..1
  vx: number; vy: number; vz: number;   // velocity units/s
  life: number;    // 1.0 → 0.0 brightness envelope
  decay: number;   // life decay rate per frame
}

const gravP: GravParticle[] = Array.from({ length: GRAV_COUNT }, () => ({
  active: false, x: 0, y: 0, z: 0, vx: 0, vy: 0, vz: 0, life: 0, decay: 0,
}));
let gravInited = false;
let gravLastSpawn = 0;
let gravLastT = 0;

function gravSpawn(): void {
  for (let i = 0; i < GRAV_COUNT; i++) {
    if (!gravP[i].active) {
      // Spawn near top vertex G(1,1,1) with random scatter
      gravP[i].active = true;
      gravP[i].x = 0.85 + (randomInt(30) - 15) * 0.005;
      gravP[i].y = 0.85 + (randomInt(30) - 15) * 0.005;
      gravP[i].z = 0.85 + (randomInt(20) - 10) * 0.005;
      // Initial velocity — mostly downward with random spread
      gravP[i].vx = (randomInt(100) - 50) * 0.003;
      gravP[i].vy = (randomInt(100) - 50) * 0.003;
      gravP[i].vz = -randomInt(50) * 0.002; // downward bias
      gravP[i].life = 1.0;
      gravP[i].decay = 0.004 + randomInt(30) * 0.0001;
      return;
    }
  }
}

const staticGravityParticles: AnimFunc = (buf, t) => {
  if (!gravInited) {
    for (let i = 0; i < GRAV_COUNT; i++) gravP[i].active = false;
    gravInited = true;
  }

  // Delta time — capped to prevent physics explosions on lag frames
  let dt = t - gravLastT;
  if (dt <= 0 || dt > 0.1) dt = 0.016;
  gravLastT = t;

  fillBlack(buf);

  // Spawn new particles
  const now = millis();
  if (now - gravLastSpawn > GRAV_SPAWN_RATE) {
    const burst = 1 + randomInt(3);
    for (let b = 0; b < burst; b++) gravSpawn();
    gravLastSpawn = now;
  }

  // Update physics
  for (let i = 0; i < GRAV_COUNT; i++) {
    if (!gravP[i].active) continue;

    // Apply gravity and drag
    gravP[i].vz -= GRAV_GRAVITY * dt;
    gravP[i].vx *= GRAV_DRAG;
    gravP[i].vy *= GRAV_DRAG;
    gravP[i].vz *= GRAV_DRAG;

    // Integrate position
    gravP[i].x += gravP[i].vx * dt;
    gravP[i].y += gravP[i].vy * dt;
    gravP[i].z += gravP[i].vz * dt;

    // Bounce off cube faces [0..1]
    if (gravP[i].x < 0.0) { gravP[i].x = 0.0; gravP[i].vx = Math.abs(gravP[i].vx) * GRAV_RESTITUTION; }
    if (gravP[i].x > 1.0) { gravP[i].x = 1.0; gravP[i].vx = -Math.abs(gravP[i].vx) * GRAV_RESTITUTION; }
    if (gravP[i].y < 0.0) { gravP[i].y = 0.0; gravP[i].vy = Math.abs(gravP[i].vy) * GRAV_RESTITUTION; }
    if (gravP[i].y > 1.0) { gravP[i].y = 1.0; gravP[i].vy = -Math.abs(gravP[i].vy) * GRAV_RESTITUTION; }
    if (gravP[i].z < 0.0) { gravP[i].z = 0.0; gravP[i].vz = Math.abs(gravP[i].vz) * GRAV_RESTITUTION; }
    if (gravP[i].z > 1.0) { gravP[i].z = 1.0; gravP[i].vz = -Math.abs(gravP[i].vz) * GRAV_RESTITUTION; }

    // Decay life
    gravP[i].life -= gravP[i].decay;
    if (gravP[i].life <= 0.0) { gravP[i].active = false; continue; }

    // Illuminate a soft halo on nearby LEDs
    const px = gravP[i].x, py = gravP[i].y, pz = gravP[i].z;
    for (let j = 0; j < NUM_LEDS; j++) {
      const dx = voxels[j].x - px, dy = voxels[j].y - py, dz = voxels[j].z - pz;
      const d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > 0.04) continue; // only LEDs within ~0.2 units
      let b = gravP[i].life * Math.exp(-d2 * 40.0);
      b = clamp(b, 0.0, 1.0);
      const c = applyPalette(b);
      buf[j].r = qadd8(buf[j].r, c.r);
      buf[j].g = qadd8(buf[j].g, c.g);
      buf[j].b = qadd8(buf[j].b, c.b);
    }
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

// ── Static5: Mobius Braid — animations.h:337-374 ────────────────────
// Three wave carriers wound on different axis pairs, quartic-sharpened
// into narrow bright ridges, summed and smoothstep-lifted.
const staticMobiusBraid: AnimFunc = (buf, t) => {
  fillBlack(buf);

  // Frequencies are irrational ratios so the pattern never perfectly repeats.
  const freqs = [1.000, 1.618, 2.414];  // 1, phi, silver ratio
  const speeds = [0.29, 0.19, 0.13];
  const phases = [0.0, 2.094, 4.189];   // 0, 2π/3, 4π/3

  for (let i = 0; i < NUM_LEDS; i++) {
    const x = voxels[i].x, y = voxels[i].y, z = voxels[i].z;

    let v = 0;
    for (let w = 0; w < 3; w++) {
      let arg: number;
      if (w === 0) arg = (x - y) * freqs[w] * 6.2832 + t * speeds[w] * 6.2832 + phases[w];
      else if (w === 1) arg = (y - z) * freqs[w] * 6.2832 + t * speeds[w] * 6.2832 + phases[w];
      else arg = (z - x) * freqs[w] * 6.2832 + t * speeds[w] * 6.2832 + phases[w];

      // Sharpen the sine into a narrow bright ridge
      let s = (Math.sin(arg) + 1.0) * 0.5;
      s = s * s * s * s; // quartic sharpening — tight knot, dark between
      v += s;
    }

    // Normalise to 0..1 — three waves can sum to 3.0 at peak
    v = clamp(v / 3.0, 0.0, 1.0);

    // Lift the brightness curve so dim regions stay visible
    // but peaks still punch to full brightness
    v = v * v * (3.0 - 2.0 * v); // smoothstep

    const c = applyPalette(v);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Static7: Plasma Cube — animations.h:392-402 ─────────────────────
const staticPlasmaCube: AnimFunc = (buf, t) => {
  for (let i = 0; i < NUM_LEDS; i++) {
    const x = voxels[i].x, y = voxels[i].y, z = voxels[i].z;
    let v = Math.sin(x * 4.0 + t * 1.1);
    v += Math.sin(y * 4.0 + t * 0.9 + 1.0);
    v += Math.sin(z * 4.0 + t * 1.3 + 2.0);
    v += Math.sin((x + y + z) * 3.0 + t * 0.7);
    v += Math.sin(Math.sqrt(x * x + y * y + z * z + 0.01) * 6.0 - t * 1.5);
    const c = applyPalette((v / 5.0 + 1.0) * 0.5);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Static9: Noise Worms ────────────────────────────────────────────
// Ported from the task brief's explicit spec (not from firmware — see file
// header note). 3D gradient noise scrolls through voxel space; the z axis
// scrolls at half rate of x, so ridges appear to crawl/wind like worms
// rather than just flicker in place.
const staticNoiseWorms: AnimFunc = (buf, t) => {
  const ti = Math.floor(t * 40);
  const tiHalf = Math.floor(ti / 2); // firmware truncates ti/2 to an integer (uint16 cast)
  for (let i = 0; i < NUM_LEDS; i++) {
    const n = inoise8(voxels[i].x * 160 + ti, voxels[i].y * 160 + tiHalf, voxels[i].z * 160);
    let v = n / 255.0;
    v = v * v; // gamma
    const c = applyPalette(v);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Static10: Coral ──────────────────────────────────────────────────
// Ported from the task brief's explicit spec (not from firmware — see file
// header note; firmware has no Coral animation). Same Gray-Scott
// reaction-diffusion structure as RD (same diffusion rates/timestep/step
// count), but its own chemical fields and a different feed/kill pair that
// produces a denser, branching maze texture instead of RD's isolated spots.
const CORAL_FEED = 0.0545, CORAL_KILL = 0.0620;
const coralA = new Array(NUM_LEDS).fill(1.0);
const coralB = new Array(NUM_LEDS).fill(0.0);
const coralA2 = new Array(NUM_LEDS).fill(0.0);
const coralB2 = new Array(NUM_LEDS).fill(0.0);
let coralReady = false;
let coralLastWatchdog = 0;

function coralSeed(): void {
  for (let i = 0; i < NUM_LEDS; i++) { coralA[i] = 1.0; coralB[i] = 0.0; }
  for (let e = 0; e < NUM_EDGES; e++) {
    const base = e * LEDS_PER_EDGE, last = base + LEDS_PER_EDGE - 1;
    const seeds = 2 + randomInt(3);
    for (let s = 0; s < seeds; s++) {
      const cx = base + 3 + randomInt(LEDS_PER_EDGE - 6);
      for (let d = -3; d <= 3; d++) {
        const idx = clamp(cx + d, base, last);
        coralA[idx] = 0.3; coralB[idx] = 0.4;
      }
    }
  }
}

function coralWatchdog(): void {           // 3s reseed watchdog, per brief
  const now = millis();
  if (now - coralLastWatchdog < 3000) return;
  coralLastWatchdog = now;
  let total = 0;
  for (let i = 0; i < NUM_LEDS; i++) total += coralB[i];
  if (total < 1.5) coralSeed();
}

function coralStepAll(): void {
  for (let i = 0; i < NUM_LEDS; i++) {
    const a = coralA[i], b = coralB[i];
    const L = graphL[i], R = graphR[i];
    const lapA = coralA[L] + coralA[R] - 2.0 * a;
    const lapB = coralB[L] + coralB[R] - 2.0 * b;
    const rxn = a * b * b;
    coralA2[i] = clamp(a + RD_DT * (RD_DA * lapA - rxn + CORAL_FEED * (1.0 - a)), 0, 1);
    coralB2[i] = clamp(b + RD_DT * (RD_DB * lapB + rxn - (CORAL_KILL + CORAL_FEED) * b), 0, 1);
  }
  for (let i = 0; i < NUM_LEDS; i++) { coralA[i] = coralA2[i]; coralB[i] = coralB2[i]; }
}

const staticCoral: AnimFunc = (buf, _t) => {
  if (!coralReady) { coralSeed(); coralReady = true; }
  coralWatchdog();
  for (let s = 0; s < RD_STEPS; s++) coralStepAll();
  for (let i = 0; i < NUM_LEDS; i++) {
    const c = applyPalette(clamp(coralB[i] * 3.5, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

registerAnimations('static',
  [staticDiagonalFlow, staticLissajous, staticGravityParticles,
   staticReactionDiffusion, staticMobiusBraid, staticPlasmaCube,
   staticNoiseWorms, staticCoral],
  ['DiagFlow', 'Lissajous', 'GravPart', 'RD', 'MobiusBraid',
   'Plasma', 'NoiseWorms', 'Coral']);
