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
