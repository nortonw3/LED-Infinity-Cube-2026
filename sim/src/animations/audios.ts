// Ports of the AUDIO animations from src/animations.h:425-935 (see
// .superpowers/sdd/firmware-ref/animations.h — the authoritative firmware
// snapshot; the worktree's committed src/animations.h is stale).
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

// ── Audio1: Impact — animations.h:475-537 ───────────────────────────
// Shockwave spheres spawn on beat/bass transient; sparse per-LED spark
// envelope fires on high-freq transients. Ambient "seethe" from mid noise.
interface Shockwave { radius: number; speed: number; cx: number; cy: number; cz: number; env: number; active: boolean; }
const SHOCK_COUNT = 4;
const shocks: Shockwave[] = Array.from({ length: SHOCK_COUNT }, () => (
  { radius: 0, speed: 0, cx: 0, cy: 0, cz: 0, env: 0, active: false }));
let shockLastBass = 0, highLast = 0, shocksInited = false;
const sparkEnvs = new Array(NUM_LEDS).fill(0); // firmware global (globals.h:46), module-local here

const audioImpact: AnimFunc = (buf, t) => {
  if (!shocksInited) {
    for (let s = 0; s < SHOCK_COUNT; s++) shocks[s].active = false;
    for (let i = 0; i < NUM_LEDS; i++) sparkEnvs[i] = 0;
    shocksInited = true;
  }

  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, tempoConfidence } = audio;

  const spawnShock = (beatFired && tempoConfidence > 0.25 && bass > 0.02) ||
                      (bass > shockLastBass * 1.35 && bass > 0.05);
  if (spawnShock) {
    for (let s = 0; s < SHOCK_COUNT; s++) {
      if (!shocks[s].active) {
        shocks[s].radius = 0;
        shocks[s].speed = 0.55 + bass * 0.8;
        shocks[s].cx = randomInt(2);
        shocks[s].cy = randomInt(2);
        shocks[s].cz = randomInt(2);
        shocks[s].env = 0.6 + bass * 1.2;
        shocks[s].active = true;
        break;
      }
    }
  }
  shockLastBass = bass;

  if (high > highLast * 1.4 && high > 0.04) {
    const cnt = 3 + Math.floor(high * 20.0);
    for (let k = 0; k < cnt; k++) sparkEnvs[randomInt(NUM_LEDS)] = 0.8 + high * 0.5;
  }
  highLast = high;

  for (let s = 0; s < SHOCK_COUNT; s++) {
    if (!shocks[s].active) continue;
    shocks[s].radius += shocks[s].speed * 0.011;
    shocks[s].env *= 0.94;
    if (shocks[s].radius > 2.0 || shocks[s].env < 0.02) shocks[s].active = false;
  }
  for (let i = 0; i < NUM_LEDS; i++) sparkEnvs[i] *= 0.78;

  for (let i = 0; i < NUM_LEDS; i++) {
    const { x, y, z } = voxels[i];
    const nx = inoise8(x * 110 + mid * 180, y * 110, z * 110 + t * 18) / 255.0;
    const ny = inoise8(y * 110 + 50, z * 110, x * 110 + t * 15) / 255.0;
    const seethe = ((nx + ny) * 0.5) * mid * 2.2;
    let shockV = 0;
    for (let s = 0; s < SHOCK_COUNT; s++) {
      if (!shocks[s].active) continue;
      const dx = x - shocks[s].cx, dy = y - shocks[s].cy, dz = z - shocks[s].cz;
      const d = Math.sqrt(dx * dx + dy * dy + dz * dz), dr = d - shocks[s].radius;
      shockV += shocks[s].env * Math.exp(-dr * dr * 35.0);
    }
    const c = applyPalette(clamp(seethe + shockV + sparkEnvs[i], 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio2: Cellular Automaton — animations.h:543-604 ───────────────
// Seeds infect neighbours along the welded graph; barPhase breathes the
// infection strength over a 4-beat bar.
const CELL_COUNT = NUM_LEDS;
const cellState = new Array(CELL_COUNT).fill(0);
const cellNext = new Array(CELL_COUNT).fill(0);
let cellLastBass = 0, cellLastHigh = 0, cellInited = false;

const audioCell: AnimFunc = (buf, _t) => {
  if (!cellInited) {
    for (let i = 0; i < CELL_COUNT; i++) cellState[i] = 0;
    cellInited = true;
  }

  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, barPhase, tempoConfidence } = audio;

  const doSeed = (beatFired && tempoConfidence > 0.25) || (bass > cellLastBass * 1.3 && bass > 0.04);
  if (doSeed) {
    const seedStr = beatFired ? (0.6 + bass * 0.6) : (0.5 + bass * 0.5);
    const seeds = beatFired ? (5 + Math.floor(bass * 25.0)) : (3 + Math.floor(bass * 20.0));
    for (let k = 0; k < seeds; k++) {
      const idx = randomInt(NUM_LEDS);
      cellState[idx] = seedStr;
      const L = graphL[idx], R = graphR[idx];
      cellState[L] = Math.max(cellState[L], seedStr * 0.7);
      cellState[R] = Math.max(cellState[R], seedStr * 0.7);
    }
  }
  cellLastBass = bass;

  if (high > cellLastHigh * 1.3 && high > 0.03) {
    const mutations = 2 + Math.floor(high * 15.0);
    for (let k = 0; k < mutations; k++) cellState[randomInt(NUM_LEDS)] = 0.4 + high * 0.5;
  }
  cellLastHigh = high;

  const barMod = tempoConfidence > 0.25 ? (0.85 + 0.3 * Math.sin(barPhase * 6.2832)) : 1.0;

  const infectThresh = 0.25 + mid * 0.25;
  const infectStrength = (0.55 + mid * 0.35) * barMod;
  const decay = 0.964 - mid * 0.018;

  for (let i = 0; i < NUM_LEDS; i++) {
    const s = cellState[i];
    const L = graphL[i], R = graphR[i];
    const spread = s > infectThresh ? s * infectStrength : 0;
    let next = Math.max(s, Math.max(cellState[L], cellState[R]) * infectStrength);
    next = Math.max(next, spread);
    next *= decay;
    cellNext[i] = clamp(next, 0.0, 1.0);
  }
  for (let i = 0; i < NUM_LEDS; i++) cellState[i] = cellNext[i];
  for (let i = 0; i < NUM_LEDS; i++) {
    const c = applyPalette(cellState[i]);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio4: Bass Bloom — animations.h:612-647 ───────────────────────
// Sphere expands from center on each beat; expansion speed matched to
// tempo so the bloom fills the cube by the next beat boundary.
let bloomRadius = 0, bloomEnv = 0, bloomLastBass = 0;

const audioBassBloom: AnimFunc = (buf, t) => {
  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, tempoConfidence, bpm } = audio;
  const beatPeriodMs = 60000.0 / bpm;

  const doBloom = (beatFired && tempoConfidence > 0.25 && bass > 0.02) ||
                   (bass > bloomLastBass * 1.3 && bass > 0.04);
  if (doBloom) {
    bloomRadius = 0.0;
    bloomEnv = 0.5 + bass * 1.5;
  }
  bloomLastBass = bass;

  const expandSpeed = tempoConfidence > 0.25 ?
    (1.8 / (beatPeriodMs * 0.001)) * 0.011 : 0.018;
  bloomRadius += expandSpeed;
  bloomEnv *= 0.91;

  for (let i = 0; i < NUM_LEDS; i++) {
    const { x, y, z } = voxels[i];
    const dx = x - 0.5, dy = y - 0.5, dz = z - 0.5;
    const d = Math.sqrt(dx * dx + dy * dy + dz * dz), dr = d - bloomRadius;
    const bloom = bloomEnv * Math.exp(-dr * dr * 50.0);
    const hum = mid * 0.4 * (Math.sin(t * 3.0 + d * 8.0) * 0.5 + 0.5);
    const spark = (inoise8(i * 13, t * 90.0) / 255.0) * high * 2.0;
    const c = applyPalette(clamp(bloom + hum + spark, 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio5: Vortex — animations.h:653-687 ───────────────────────────
// Spiral arms; spin locks to one revolution per beat once tempo
// confidence is high, otherwise free-spins with the mids.
const audioVortex: AnimFunc = (buf, t) => {
  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatPhase, bpm, tempoConfidence } = audio;

  const spinBase = tempoConfidence > 0.5 ? bpm / 60.0 : 1.5 + mid * 8.0;
  const spin = t * spinBase;

  const beatPulse = tempoConfidence > 0.25 ? (1.0 - 0.3 * beatPhase) : 1.0;
  const armWidth = Math.max((0.25 + bass * 0.4) * beatPulse, 0.01);

  for (let i = 0; i < NUM_LEDS; i++) {
    const x = voxels[i].x - 0.5, y = voxels[i].y - 0.5, z = voxels[i].z;
    const r2 = x * x + y * y;
    const normAng = r2 < 1e-6 ? 0.0 : (Math.atan2(y, x) / 6.2832 + 0.5);
    const armPhase = (normAng - z * 0.5 - spin * 0.05 + 2.0) % 1.0;
    const dArm = armPhase - 0.5;
    const v = Math.exp(-dArm * dArm / (armWidth * armWidth));
    const spark = (inoise8(i * 17, t * 80.0) / 255.0) * high * 1.5;
    const c = applyPalette(clamp(v * (0.1 + bass * 0.5) + spark, 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio7: Pulse Web — animations.h:695-754 ────────────────────────
// Rings spawn from random cube corners on beat/bass transient; speed
// tempo-matched like BassBloom so a pulse crosses the cube in one beat.
interface PulseWeb { active: boolean; radius: number; env: number; speed: number; cx: number; cy: number; cz: number; }
const PWEB_COUNT = 8;
const pwebs: PulseWeb[] = Array.from({ length: PWEB_COUNT }, () => (
  { active: false, radius: 0, env: 0, speed: 0, cx: 0, cy: 0, cz: 0 }));
let pwebLastBass = 0, pwebInited = false;

const audioPulseWeb: AnimFunc = (buf, t) => {
  if (!pwebInited) {
    for (let i = 0; i < PWEB_COUNT; i++) pwebs[i].active = false;
    pwebInited = true;
  }

  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, tempoConfidence, bpm } = audio;
  const beatPeriodMs = 60000.0 / bpm;

  const doSpawn = (beatFired && tempoConfidence > 0.25 && bass > 0.02) ||
                  (bass > pwebLastBass * 1.25 && bass > 0.04);
  if (doSpawn) {
    for (let i = 0; i < PWEB_COUNT; i++) {
      if (!pwebs[i].active) {
        const corner = CUBE_CORNERS[randomInt(8)];
        const spd = tempoConfidence > 0.25 ?
          (1.8 / (beatPeriodMs * 0.001)) * 0.011 : 0.45 + bass * 0.6;
        pwebs[i].active = true;
        pwebs[i].radius = 0.0;
        pwebs[i].env = 0.5 + bass * 1.2;
        pwebs[i].speed = spd;
        pwebs[i].cx = corner.x; pwebs[i].cy = corner.y; pwebs[i].cz = corner.z;
        break;
      }
    }
  }
  pwebLastBass = bass;

  for (let i = 0; i < PWEB_COUNT; i++) {
    if (!pwebs[i].active) continue;
    pwebs[i].radius += pwebs[i].speed;
    pwebs[i].env *= 0.93;
    if (pwebs[i].radius > 2.0 || pwebs[i].env < 0.02) pwebs[i].active = false;
  }

  const hum = mid * 0.2;
  for (let i = 0; i < NUM_LEDS; i++) {
    const { x, y, z } = voxels[i];
    let v = hum;
    for (let w = 0; w < PWEB_COUNT; w++) {
      if (!pwebs[w].active) continue;
      const dx = x - pwebs[w].cx, dy = y - pwebs[w].cy, dz = z - pwebs[w].cz;
      const d = Math.sqrt(dx * dx + dy * dy + dz * dz), dr = d - pwebs[w].radius;
      v += pwebs[w].env * Math.exp(-dr * dr * 40.0);
    }
    const spark = (inoise8(i * 11, t * 90.0) / 255.0) * high * 1.5;
    const c = applyPalette(clamp(v + spark, 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio8: Spectrum Helix — animations.h:760-798 ───────────────────
// A rotating helix stripe; height selects which frequency band lights
// that stripe segment (bottom=sub … top=brilliance). Beat flares brightness.
let helixFlare = 0.0;

const audioSpectrumHelix: AnimFunc = (buf, t) => {
  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const { beatFired, bpm, tempoConfidence } = audio;

  const spinRateBase = tempoConfidence > 0.5 ? bpm / 60.0 : 0.8 + mid * 5.0;
  const spinRate = spinRateBase * 0.08;
  const spin = t * spinRateBase;

  if (beatFired && tempoConfidence > 0.25) helixFlare = 0.5 + bass * 0.5;
  helixFlare *= 0.82;

  const ti = t * 50.0;

  for (let i = 0; i < NUM_LEDS; i++) {
    const x = voxels[i].x - 0.5, y = voxels[i].y - 0.5, z = voxels[i].z;
    const r = Math.sqrt(x * x + y * y);
    const theta = r < 0.001 ? 0.0 : (Math.atan2(y, x) / 6.2832 + 0.5);
    const helixTheta = (z * 1.5 + spin * spinRate + 1.0) % 1.0;
    let dTheta = Math.abs(theta - helixTheta);
    if (dTheta > 0.5) dTheta = 1.0 - dTheta;
    const onHelix = Math.exp(-dTheta * dTheta * 80.0) * Math.exp(-r * r * 12.0);

    // Height selects the perceptual band: bottom=sub … top=brilliance
    const band = clamp(Math.floor(z * NUM_BANDS), 0, NUM_BANDS - 1);
    const fv = audio.band[band];
    const amb = (inoise8(i * 13, ti) / 255.0) * clamp(audio.level * 2.0 + 0.15, 0.05, 0.4);
    const stripe = onHelix * (0.3 + fv * 0.7) + helixFlare * onHelix;
    const c = applyPalette(clamp(amb + stripe, 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio9: Earthquake — animations.h:804-840 ───────────────────────
// Ground wave rises on beats; barPhase drives a 4-beat tension/release
// shudder cycle; upward sparks scale with height.
let quakeLastBass = 0, quakeRumbleEnv = 0;

const audioEarthquake: AnimFunc = (buf, t) => {
  const bass = audio.sub() * 0.6 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid() + audio.highMid() * 0.5;
  const high = audio.presence() + audio.brilliance();
  const { beatFired, barPhase, tempoConfidence } = audio;

  if (beatFired && tempoConfidence > 0.25) {
    quakeRumbleEnv = clamp(quakeRumbleEnv + 0.4 + bass * 1.2, 0.0, 1.5);
  } else if (bass > quakeLastBass * 1.2 && bass > 0.03) {
    quakeRumbleEnv = clamp(quakeRumbleEnv + bass * 1.5, 0.0, 1.5);
  }

  quakeLastBass = bass;
  quakeRumbleEnv *= 0.97;

  const barTension = tempoConfidence > 0.25 ? (0.5 + 0.5 * Math.sin(barPhase * 3.14159)) : 1.0;

  const ti = t * 55.0;
  for (let i = 0; i < NUM_LEDS; i++) {
    const { x, y, z } = voxels[i];
    const waveFront = quakeRumbleEnv * 0.6;
    const groundV = Math.exp(-(z - waveFront) * (z - waveFront) * 20.0) * quakeRumbleEnv;
    const nx = inoise8(x * 80 + ti, y * 80) / 255.0;
    const ny = inoise8(y * 80 + ti + 100, z * 80) / 255.0;
    const shudder = ((nx + ny) * 0.5) * mid * 2.0 * barTension;
    let upSpark = 0;
    if (high > 0.04) upSpark = (inoise8(i * 19, ti * 2) / 255.0) * high * 2.5 * z;
    const c = applyPalette(clamp(groundV + shudder + upSpark, 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio: Flame — animations.h:846-880 ─────────────────────────────
// Beat/bass injects heat at graph nodes; heat diffuses along the welded
// edge graph (graphBlur) and cools, so fire crawls around the wireframe
// and through corners.
const flameHeat = new Array(NUM_LEDS).fill(0);
let flameInit = false, flameLastBass = 0;

const audioFlame: AnimFunc = (buf, _t) => {
  const bass = audio.sub() * 0.7 + audio.bass();
  const mid = audio.lowMid() * 0.5 + audio.mid();
  const { beatFired, tempoConfidence } = audio;

  if (!flameInit) { for (let i = 0; i < NUM_LEDS; i++) flameHeat[i] = 0; flameInit = true; }

  const inject = (beatFired && tempoConfidence > 0.25) || (bass > flameLastBass * 1.3 && bass > 0.05);
  flameLastBass = bass;
  if (inject) {
    const seeds = 2 + Math.floor(bass * 8.0);
    for (let k = 0; k < seeds; k++) {
      const idx = randomInt(NUM_LEDS);
      flameHeat[idx] = clamp(flameHeat[idx] + 0.7 + bass * 0.6, 0.0, 1.5);
      flameHeat[graphL[idx]] = Math.max(flameHeat[graphL[idx]], 0.5);
      flameHeat[graphR[idx]] = Math.max(flameHeat[graphR[idx]], 0.5);
    }
  }
  // Sparse embers from the mids keep it flickering between beats.
  if (mid > 0.1 && randomInt(100) < Math.floor(mid * 40.0)) flameHeat[randomInt(NUM_LEDS)] += mid * 0.5;

  // Spread along the graph, then cool (louder mids = more sustain).
  graphBlur(flameHeat, 1, 0.28);
  const cool = 0.90 - clamp(mid * 0.03, 0.0, 0.05);
  for (let i = 0; i < NUM_LEDS; i++) {
    flameHeat[i] *= cool;
    const c = applyPalette(clamp(flameHeat[i], 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Audio: Dendrite — animations.h:887-935 ──────────────────────────
// Beats spawn charges that walk the welded graph and fork, branching
// around corners like lightning; a decaying charge field leaves fading
// trails behind each tip.
interface DendTip { active: boolean; node: number; fwd: boolean; life: number; env: number; }
const DEND_TIPS = 24;
const dendTips: DendTip[] = Array.from({ length: DEND_TIPS }, () => (
  { active: false, node: 0, fwd: true, life: 0, env: 0 }));
const dendCharge = new Array(NUM_LEDS).fill(0);
let dendInit = false;

function dendSpawn(node: number, fwd: boolean, env: number, life: number): void {
  for (let i = 0; i < DEND_TIPS; i++) {
    if (!dendTips[i].active) {
      dendTips[i].active = true;
      dendTips[i].node = node;
      dendTips[i].fwd = fwd;
      dendTips[i].life = life;
      dendTips[i].env = env;
      return;
    }
  }
}

const audioDendrite: AnimFunc = (buf, _t) => {
  const bass = audio.sub() * 0.7 + audio.bass();
  const high = audio.presence() + audio.brilliance();
  const { beatFired, tempoConfidence } = audio;

  if (!dendInit) {
    for (let i = 0; i < DEND_TIPS; i++) dendTips[i].active = false;
    for (let i = 0; i < NUM_LEDS; i++) dendCharge[i] = 0;
    dendInit = true;
  }

  // Strike on a confident beat (or a big bass hit): several branches
  // from one node, each heading a random way around the graph.
  const strike = (beatFired && tempoConfidence > 0.25 && bass > 0.03) || (bass > 0.6);
  if (strike) {
    const startNode = randomInt(NUM_LEDS);
    const branches = 2 + Math.floor(bass * 3.0);
    const life = 12 + Math.floor(bass * 10.0);
    for (let b = 0; b < branches; b++) dendSpawn(startNode, randomInt(2) === 0, 0.8 + bass * 0.5, life);
  }

  // Advance tips along the graph; occasionally fork the other way.
  for (let i = 0; i < DEND_TIPS; i++) {
    if (!dendTips[i].active) continue;
    const tp = dendTips[i];
    dendCharge[tp.node] = Math.max(dendCharge[tp.node], tp.env + high * 0.4);
    tp.node = tp.fwd ? graphR[tp.node] : graphL[tp.node];
    tp.life--;
    if (tp.life > 3 && randomInt(100) < 18) dendSpawn(tp.node, !tp.fwd, tp.env * 0.7, Math.floor(tp.life / 2));
    if (tp.life <= 0) tp.active = false;
  }

  // Decay the charge field → fading dendrite trails.
  for (let i = 0; i < NUM_LEDS; i++) {
    dendCharge[i] *= 0.80;
    const c = applyPalette(clamp(dendCharge[i], 0.0, 1.0));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

registerAnimations('audio',
  [audioTriAxis, audioImpact, audioCell, audioBassBloom, audioVortex,
   audioPulseWeb, audioSpectrumHelix, audioEarthquake, audioFlame, audioDendrite],
  ['TriAxis', 'Impact', 'CellAuto', 'BassBloom', 'Vortex',
   'PulseWeb', 'SpectrHlix', 'Earthquake', 'Flame', 'Dendrite']);
