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
