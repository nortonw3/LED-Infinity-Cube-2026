// Ports of the VOICE animations from firmware animations.h:942-1056.
// The firmware is the source of truth — constants and math match verbatim.
import { voxels, graphL, graphR, NUM_LEDS } from '../geometry';
import { applyPalette } from '../palettes';
import { clamp, qadd8, randomInt, inoise8 } from '../fastled';
import { registerAnimations, type AnimFunc } from '../engine';
import { audio } from '../audio/bus';

const INV_SQRT3 = 0.57735;

// ── Voice0: Breathe — animations.h:942-952 ─────────────────────────
const voiceBreathe: AnimFunc = (buf, t) => {
  const speechEnergy = audio.speech;
  const high = audio.presence() + audio.brilliance();
  const breath = 0.06 + 0.06 * Math.sin(t * 1.1);
  const swell = speechEnergy * 2.5;
  const tipFlash = high * 3.0 * Math.exp(-high * 2.0);
  for (let i = 0; i < NUM_LEDS; i++) {
    const h = (voxels[i].x + voxels[i].y + voxels[i].z) * INV_SQRT3;
    const c = applyPalette(clamp(breath + swell * (0.4 + h * 0.6) + tipFlash * h * h, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Voice1: Formant — animations.h:955-973 ─────────────────────────
// Two spring-driven formant bands slide up/down the diagonal; syllable
// onsets kick them apart.
const BAND_W = 0.12;
let posA = 0.15, posB = 0.85, velA = 0, velB = 0;

const voiceFormant: AnimFunc = (buf, _t) => {
  const speechEnergy = audio.speech;
  const syllableOnset = audio.syllableOnset;
  const sylEnv = audio.sylEnv;
  const comp = speechEnergy * 1.8;
  const tA = 0.15 + comp * 0.25, tB = 0.85 - comp * 0.25;
  if (syllableOnset) { velA += 0.04; velB -= 0.04; }
  velA += (tA - posA) * 0.08; velB += (tB - posB) * 0.08;
  velA *= 0.75; velB *= 0.75;
  posA += velA; posB += velB;
  posA = clamp(posA, 0.05, 0.95); posB = clamp(posB, 0.05, 0.95);
  const br = 0.3 + speechEnergy * 2.0 + sylEnv * 0.5;
  for (let i = 0; i < NUM_LEDS; i++) {
    const h = (voxels[i].x + voxels[i].y + voxels[i].z) * INV_SQRT3;
    const vA = Math.exp(-Math.abs(h - posA) * Math.abs(h - posA) / (BAND_W * BAND_W) * 4.0);
    const vB = Math.exp(-Math.abs(h - posB) * Math.abs(h - posB) / (BAND_W * BAND_W) * 4.0);
    const between = (h > posA && h < posB) ? speechEnergy * 0.4 * (1.0 - Math.abs(h - 0.5) * 2.0) : 0;
    const c = applyPalette(clamp((vA + vB) * br + between, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Voice2: Harmonic Rings — animations.h:977-994 ──────────────────
// Rings expand from the cube center on each syllable onset.
const HRING_COUNT = 6;
interface HarmonicRing { radius: number; env: number; speed: number; active: boolean; }
const hrings: HarmonicRing[] = Array.from({ length: HRING_COUNT },
  () => ({ radius: 0, env: 0, speed: 0, active: false }));

const voiceHarmonicRings: AnimFunc = (buf, t) => {
  const syllableOnset = audio.syllableOnset;
  const speechEnergy = audio.speech;
  const voiceLevel = audio.level;
  if (syllableOnset) {
    for (let i = 0; i < HRING_COUNT; i++) {
      if (!hrings[i].active) {
        // firmware struct literal order: {radius, env, speed, active}
        hrings[i] = { radius: 0.0, env: 0.6 + speechEnergy * 1.5, speed: 0.4 + speechEnergy * 0.8, active: true };
        break;
      }
    }
  }
  for (let i = 0; i < HRING_COUNT; i++) {
    if (!hrings[i].active) continue;
    hrings[i].radius += hrings[i].speed * 0.011;
    hrings[i].env *= 0.93;
    if (hrings[i].radius > 1.5 || hrings[i].env < 0.02) hrings[i].active = false;
  }
  const hum = voiceLevel * 0.3 * (Math.sin(t * 4.0) * 0.5 + 0.5);
  for (let i = 0; i < NUM_LEDS; i++) {
    const dx = voxels[i].x - 0.5, dy = voxels[i].y - 0.5, dz = voxels[i].z - 0.5;
    const d = Math.sqrt(dx * dx + dy * dy + dz * dz);
    let v = hum;
    for (let r = 0; r < HRING_COUNT; r++) {
      if (!hrings[r].active) continue;
      const dr = d - hrings[r].radius;
      v += hrings[r].env * (dr < 0 ? Math.exp(-dr * dr * 60.0) : Math.exp(-dr * dr * 180.0) * 1.4);
    }
    const c = applyPalette(clamp(v, 0, 1));
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
};

// ── Voice3: Syllable Sparks — animations.h:996-1056 ────────────────
// Sparks walk the welded edge graph, flowing across corners, with a
// fading tail trailing behind each head.
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
  const syllableOnset = audio.syllableOnset;
  const speechEnergy = audio.speech;
  const sylEnv = audio.sylEnv;
  const high = audio.presence() + audio.brilliance();
  const voiceLevel = audio.level;

  if (syllableOnset) fireSyllableSparks(speechEnergy, sylEnv);

  // Ambient shimmer sampled at each LED's 3D position → continuous across corners.
  const ti = Math.floor(t * 80.0);
  for (let i = 0; i < NUM_LEDS; i++) {
    const n = inoise8(voxels[i].x * 140.0 + ti, voxels[i].y * 140.0, voxels[i].z * 140.0);
    const c = applyPalette((n / 255) * voiceLevel * 0.35);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }

  // Advance + render each spark walking the graph, with a fading tail.
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

registerAnimations('voice',
  [voiceBreathe, voiceFormant, voiceHarmonicRings, voiceSyllableSparks],
  ['Breathe', 'Formant', 'Rings', 'Sparks']);
