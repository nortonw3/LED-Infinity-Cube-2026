// One-off generator for sim/public/demo-track.wav — a procedurally
// synthesized 120 BPM loop (kick + closed hats + soft mid pad).
// No license needed: this is code-generated audio, not a licensed track.
// Replace demo-track.wav with any real track if you want a richer demo.
//
// Run with: node scripts/gen-demo-track.mjs
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SAMPLE_RATE = 44100;
const DURATION_S = 8;                 // 8 s loop @ 120 BPM = 16 beats
const BEAT_S = 0.5;                   // 120 BPM
const N = Math.floor(SAMPLE_RATE * DURATION_S);

const samples = new Float32Array(N);

// Simple seeded PRNG for reproducible noise bursts (hats).
let seed = 1234567;
function rand() {
  seed = (seed * 1664525 + 1013904223) >>> 0;
  return (seed / 0xffffffff) * 2 - 1;
}

for (let n = 0; n < N; n++) {
  const t = n / SAMPLE_RATE;
  const beatT = t % BEAT_S;               // time since last beat
  const beatIdx = Math.floor(t / BEAT_S);

  // Kick: decaying ~60 Hz sine on every beat.
  const kickEnv = Math.exp(-beatT * 22);
  const kick = Math.sin(2 * Math.PI * 60 * beatT) * kickEnv * 0.6;

  // Closed hat: short decaying filtered noise burst on the off-beat
  // (halfway between kicks).
  const hatT = (t - BEAT_S / 2) % BEAT_S;
  const hatEnv = Math.exp(-hatT * 60);
  // crude high-pass-ish shaping: differencing white noise emphasizes highs
  const hat = (rand() - rand()) * hatEnv * 0.18;

  // Soft mid pad: slow pair of detuned sines, gently amplitude-modulated
  // per bar (4 beats) for movement.
  const barPhase = (beatIdx % 4) / 4;
  const padAmp = 0.05 + 0.02 * Math.sin(2 * Math.PI * (t / (BEAT_S * 4)));
  const pad =
    (Math.sin(2 * Math.PI * 220 * t) + Math.sin(2 * Math.PI * 277.18 * t)) *
    padAmp * 0.5;

  let s = kick + hat + pad;
  // Soft clip to keep amplitude sane and avoid harsh digital clipping.
  s = Math.tanh(s * 1.2) * 0.85;
  samples[n] = s;
}

// Encode mono 16-bit PCM WAV.
const bytesPerSample = 2;
const dataSize = N * bytesPerSample;
const buffer = Buffer.alloc(44 + dataSize);

buffer.write('RIFF', 0, 'ascii');
buffer.writeUInt32LE(36 + dataSize, 4);
buffer.write('WAVE', 8, 'ascii');
buffer.write('fmt ', 12, 'ascii');
buffer.writeUInt32LE(16, 16);            // fmt chunk size
buffer.writeUInt16LE(1, 20);             // PCM
buffer.writeUInt16LE(1, 22);             // mono
buffer.writeUInt32LE(SAMPLE_RATE, 24);
buffer.writeUInt32LE(SAMPLE_RATE * bytesPerSample, 28); // byte rate
buffer.writeUInt16LE(bytesPerSample, 32);                // block align
buffer.writeUInt16LE(16, 34);            // bits per sample
buffer.write('data', 36, 'ascii');
buffer.writeUInt32LE(dataSize, 40);

for (let n = 0; n < N; n++) {
  const clamped = Math.max(-1, Math.min(1, samples[n]));
  buffer.writeInt16LE(Math.round(clamped * 32767), 44 + n * bytesPerSample);
}

const outPath = join(__dirname, '..', 'public', 'demo-track.wav');
writeFileSync(outPath, buffer);
console.log(`Wrote ${outPath} (${(buffer.length / 1024).toFixed(1)} KB)`);
