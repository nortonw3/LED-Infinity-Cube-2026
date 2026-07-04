// Audio sources — all feed a 512-bin frame shaped like the firmware's
// fftBins (FFT1024 @ 44.1 kHz → ~43 Hz/bin) into audioEngineUpdate.
// Demo track: procedurally-generated 120 BPM placeholder beat (kick +
// closed hats + soft mid pad), synthesized by scripts/gen-demo-track.mjs.
// No license required — it's code-generated audio, not a downloaded
// track. Swap public/demo-track.wav for any real track if desired.
import { audioEngineUpdate } from './engine';

export type SourceKind = 'none' | 'demo' | 'system' | 'synthetic';

let ctx: AudioContext | null = null;
let analyser: AnalyserNode | null = null;
let mediaEl: HTMLAudioElement | null = null;
let mediaStream: MediaStream | null = null;
let kind: SourceKind = 'none';

const bins = new Float32Array(512);
let freqData: Uint8Array<ArrayBuffer> | null = null;

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
  mediaEl = new Audio('demo-track.wav');
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
