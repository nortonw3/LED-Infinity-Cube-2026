#pragma once

#include "globals.h"   // fftBins[], FFT_BINS

////////////////////////////////////////////////////////////
// ================= AUDIO ENGINE =================
// Consumes the shared FFT (fftBins[]) and publishes one
// AudioBus that animations read. Three stages:
//   1. Perceptual bands   — log-spaced sums of FFT bins
//   2. Room-noise AGC      — per-band floor/ceiling followers
//                            normalize each band to 0..1 in
//                            ANY room (quiet or loud), gated
//                            to kill hiss
//   3. Beat + tempo        — multi-band onset, autocorrelation
//                            tempo estimate, PLL beat phase
//
// Global knobs shape all animations at once (no per-anim
// rewiring): reactivity, beatSensitivity, bandTilt.
//
// Phase 3: runs in PARALLEL with the legacy pipeline so the
// bus can be verified via `audio` serial monitor before the
// animations are migrated onto it.
////////////////////////////////////////////////////////////

#define NUM_BANDS 7

// FFT1024 @ 44.1 kHz → ~43.07 Hz/bin. Inclusive bin ranges:
//   sub 43-86 | bass 86-172 | loMid 172-388 | mid 388-990
//   hiMid 990-2500 | presence 2500-5000 | brilliance 5000-10.8k
static const int   BAND_LO[NUM_BANDS] = {  1,  2,  4,  9, 23,  58, 116 };
static const int   BAND_HI[NUM_BANDS] = {  2,  4,  9, 23, 58, 116, 250 };
static const char* BAND_NAMES[NUM_BANDS] = { "sub","bass","loMid","mid","hiMid","pres","bril" };

struct AudioBus {
  float band[NUM_BANDS];   // room-adapted, normalized 0..1
  float flux[NUM_BANDS];   // positive change per band (onset energy)
  float level;             // overall loudness 0..1
  float centroid;          // spectral brightness 0..1
  float beatPhase;         // 0..1, resets each beat
  float barPhase;          // 0..1 over 4 beats
  bool  beatFired;         // true exactly one frame per beat
  float bpm;
  float tempoConfidence;   // 0..1

  // voice-oriented features (derived from the speech bands)
  float speech;            // speech-band energy 0..1
  bool  syllableOnset;     // true one frame per detected syllable
  float sylEnv;            // decaying envelope after a syllable

  float sub()        const { return band[0]; }
  float bass()       const { return band[1]; }
  float lowMid()     const { return band[2]; }
  float mid()        const { return band[3]; }
  float highMid()    const { return band[4]; }
  float presence()   const { return band[5]; }
  float brilliance() const { return band[6]; }
};

AudioBus audio;

// ── Global knobs (UI-wired in Phase 5) ────────────────────
float reactivity      = 1.0f;   // overall band gain
float beatSensitivity = 1.0f;   // >1 = more sensitive onset threshold
float bandTilt        = 0.0f;   // -1 bass-heavy .. +1 treble-heavy

bool  audioMonitorEnabled = false;

////////////////////////////////////////////////////////////
// ================= AGC STATE =================
////////////////////////////////////////////////////////////

// Floor: tracks ambient room level — falls fast, rises slowly.
// Ceiling: tracks loud peaks — rises fast, decays slowly.
#define FLOOR_RISE     0.0008f
#define FLOOR_FALL     0.03f
#define CEIL_RISE      0.30f
#define CEIL_DECAY     0.004f
#define BAND_GATE      0.06f     // normalized gate to kill hiss
#define AGC_WARMUP_MS  1500

static float smoothBand[NUM_BANDS] = {0};
static float bandFloor[NUM_BANDS]  = {0};
static float bandCeil[NUM_BANDS]   = {0};
static float prevBand[NUM_BANDS]   = {0};

////////////////////////////////////////////////////////////
// ================= BEAT / TEMPO STATE =================
////////////////////////////////////////////////////////////

#define ONSET_LEN      200        // ring buffer of onset envelope (~4 s)
#define BPM_MIN        60.0f
#define BPM_MAX        200.0f
#define AUTOCORR_EVERY 8          // frames between tempo re-estimates

static float onsetEnv[ONSET_LEN]   = {0};
static int   onsetHead             = 0;
static float avgFrameMs            = 22.0f;   // measured frame period (EMA)
static float beatPeriodMsA         = 500.0f;  // 120 BPM default
static float engBeatAccumMs           = 0.0f;
static float engBarAccumBeats         = 0.0f;
static int   autocorrCounter       = 0;
static unsigned long lastEngineMs  = 0;
static unsigned long lastOnsetMs   = 0;

////////////////////////////////////////////////////////////
// ================= TEMPO ESTIMATE =================
// Autocorrelation of the onset envelope over lags in the
// trackable BPM range. Peak lag → period → BPM. Confidence
// from how much the peak stands above the mean correlation.
////////////////////////////////////////////////////////////

static void estimateTempo() {
  int minLag = (int)((60000.0f / BPM_MAX) / avgFrameMs);   // fastest tempo
  int maxLag = (int)((60000.0f / BPM_MIN) / avgFrameMs);   // slowest tempo
  minLag = constrain(minLag, 4, ONSET_LEN / 2);
  maxLag = constrain(maxLag, minLag + 1, ONSET_LEN - 1);

  float best = 0, bestLag = 0, sum = 0; int lags = 0;
  for (int lag = minLag; lag <= maxLag; lag++) {
    float corr = 0;
    for (int k = 0; k < ONSET_LEN - lag; k++) {
      int i  = (onsetHead - 1 - k        + ONSET_LEN) % ONSET_LEN;
      int j  = (onsetHead - 1 - k - lag  + ONSET_LEN) % ONSET_LEN;
      corr += onsetEnv[i] * onsetEnv[j];
    }
    corr /= (ONSET_LEN - lag);
    sum += corr; lags++;
    if (corr > best) { best = corr; bestLag = lag; }
  }
  if (bestLag < 1 || best <= 0) return;

  float mean = sum / max(lags, 1);
  float prominence = (best - mean) / (best + 1e-6f);       // 0..1
  float targetPeriod = bestLag * avgFrameMs;
  float targetBpm    = constrain(60000.0f / targetPeriod, BPM_MIN, BPM_MAX);

  // Slow attack toward the new tempo; confidence from prominence.
  float newBpm    = audio.bpm * 0.8f + targetBpm * 0.2f;
  audio.bpm       = constrain(newBpm, BPM_MIN, BPM_MAX);
  beatPeriodMsA   = 60000.0f / audio.bpm;
  audio.tempoConfidence = audio.tempoConfidence * 0.7f +
                          constrain(prominence * 2.0f, 0.0f, 1.0f) * 0.3f;
}

////////////////////////////////////////////////////////////
// ================= MAIN UPDATE =================
////////////////////////////////////////////////////////////

void audioEngineUpdate() {
  unsigned long now = millis();
  float dt = (lastEngineMs > 0) ? (float)(now - lastEngineMs) : 22.0f;
  dt = constrain(dt, 1.0f, 50.0f);
  lastEngineMs = now;
  avgFrameMs = avgFrameMs * 0.95f + dt * 0.05f;

  bool warm = now > AGC_WARMUP_MS;

  // ── 1. Perceptual bands (mean magnitude per band) ────────
  for (int b = 0; b < NUM_BANDS; b++) {
    float sum = 0;
    for (int i = BAND_LO[b]; i <= BAND_HI[b] && i < FFT_BINS; i++) sum += fftBins[i];
    sum /= (float)(BAND_HI[b] - BAND_LO[b] + 1);
    smoothBand[b] = smoothBand[b] * 0.6f + sum * 0.4f;
  }

  // ── 2. Room-noise AGC + features ─────────────────────────
  float centNum = 0, centDen = 0, levelSum = 0;
  for (int b = 0; b < NUM_BANDS; b++) {
    float r = smoothBand[b];

    float df = r - bandFloor[b];
    bandFloor[b] += (df < 0) ? df * FLOOR_FALL : df * FLOOR_RISE;

    float dc = r - bandCeil[b];
    bandCeil[b] += (dc > 0) ? dc * CEIL_RISE : dc * CEIL_DECAY;
    if (bandCeil[b] < bandFloor[b] + 1e-5f) bandCeil[b] = bandFloor[b] + 1e-5f;

    float span = bandCeil[b] - bandFloor[b];
    float norm = (span > 1e-6f) ? (r - bandFloor[b]) / span : 0.0f;
    norm = constrain(norm, 0.0f, 1.0f);

    // gate the bottom to kill hiss, then re-expand
    norm = (norm < BAND_GATE) ? 0.0f : (norm - BAND_GATE) / (1.0f - BAND_GATE);

    // band tilt: b=0 → (1-tilt), b=last → (1+tilt)
    float pos      = (float)b / (NUM_BANDS - 1) - 0.5f;   // -0.5..+0.5
    float tiltGain = 1.0f + bandTilt * pos * 2.0f;
    norm = constrain(norm * reactivity * tiltGain, 0.0f, 1.0f);
    if (!warm) norm = 0.0f;

    audio.band[b] = norm;

    float fx = norm - prevBand[b];
    audio.flux[b] = (fx > 0) ? fx : 0.0f;
    prevBand[b] = norm;

    centNum += norm * b; centDen += norm; levelSum += norm;
  }
  audio.centroid = (centDen > 1e-6f) ? (centNum / centDen) / (NUM_BANDS - 1) : 0.0f;
  audio.level    = constrain(levelSum / NUM_BANDS, 0.0f, 1.0f);

  // ── Voice features (speech = mid + high-mid bands) ───────
  static float speechAvg = 0, speechLast = 0;
  audio.speech = constrain(audio.mid() * 0.7f + audio.highMid() * 0.7f, 0.0f, 1.0f);
  speechAvg = speechAvg * 0.98f + audio.speech * 0.02f;
  audio.syllableOnset = false;
  if (audio.speech > speechAvg * 1.6f && audio.speech > 0.08f &&
      audio.speech > speechLast * 1.2f) {
    audio.syllableOnset = true;
    audio.sylEnv = 1.0f;
  }
  speechLast = audio.speech;
  audio.sylEnv *= 0.91f;

  // ── 3. Onset envelope (kick-weighted + broadband) ────────
  float onset = audio.flux[0] + audio.flux[1];           // sub + bass = kick
  float broad = 0;
  for (int b = 2; b < NUM_BANDS; b++) broad += audio.flux[b];
  onset += broad * 0.3f;

  onsetEnv[onsetHead] = onset;
  onsetHead = (onsetHead + 1) % ONSET_LEN;

  // ── 4. Tempo estimate (periodic, not every frame) ────────
  if (++autocorrCounter >= AUTOCORR_EVERY) {
    autocorrCounter = 0;
    estimateTempo();
  }
  audio.tempoConfidence = constrain(audio.tempoConfidence - 0.0005f, 0.0f, 1.0f);

  // ── 5. Adaptive onset detection (mean + k·spread) ────────
  float mean = 0;
  int win = min(45, ONSET_LEN);                          // ~1 s
  for (int k = 0; k < win; k++)
    mean += onsetEnv[(onsetHead - 1 - k + ONSET_LEN) % ONSET_LEN];
  mean /= win;
  float var = 0;
  for (int k = 0; k < win; k++) {
    float d = onsetEnv[(onsetHead - 1 - k + ONSET_LEN) % ONSET_LEN] - mean;
    var += d * d;
  }
  float sd = sqrtf(var / win);
  float thresh = mean + sd * (1.6f / beatSensitivity);
  bool onsetHit = (onset > thresh) && (onset > 0.004f) && (now - lastOnsetMs > 90);
  if (onsetHit) lastOnsetMs = now;

  // ── 6. PLL beat phase ────────────────────────────────────
  audio.beatFired = false;
  engBeatAccumMs += dt;
  if (engBeatAccumMs >= beatPeriodMsA) {
    engBeatAccumMs -= beatPeriodMsA;
    audio.beatFired = true;
    engBarAccumBeats = fmodf(engBarAccumBeats + 1.0f, 4.0f);
  }
  // Nudge phase toward a confident onset landing near a beat boundary.
  if (onsetHit && audio.tempoConfidence > 0.4f) {
    float ph = engBeatAccumMs / beatPeriodMsA;              // 0..1
    if (ph < 0.18f)      engBeatAccumMs *= 0.5f;            // just after beat → pull back
    else if (ph > 0.82f) engBeatAccumMs += (beatPeriodMsA - engBeatAccumMs) * 0.5f; // just before → push to beat
  }
  audio.beatPhase = engBeatAccumMs / beatPeriodMsA;
  audio.barPhase  = (engBarAccumBeats + audio.beatPhase) / 4.0f;
}

////////////////////////////////////////////////////////////
// ================= SERIAL MONITOR =================
////////////////////////////////////////////////////////////

static unsigned long lastAudioMon = 0;
#define AUDIO_MON_INTERVAL_MS 150

void printAudioMonitor() {
  if (!audioMonitorEnabled) return;
  if (millis() - lastAudioMon < AUDIO_MON_INTERVAL_MS) return;
  lastAudioMon = millis();
  Serial.print(F("bands "));
  for (int b = 0; b < NUM_BANDS; b++) {
    Serial.print(BAND_NAMES[b]); Serial.print(':');
    int bars = constrain((int)(audio.band[b] * 10.0f), 0, 10);
    for (int k = 0; k < bars; k++) Serial.print('#');
    for (int k = bars; k < 10; k++) Serial.print('.');
    Serial.print(' ');
  }
  Serial.print(F(" lvl:"));  Serial.print(audio.level, 2);
  Serial.print(F(" cen:"));  Serial.print(audio.centroid, 2);
  Serial.print(F(" bpm:"));  Serial.print(audio.bpm, 1);
  Serial.print(F(" conf:")); Serial.print(audio.tempoConfidence, 2);
  Serial.print(F(" beat:")); Serial.println(audio.beatFired ? '*' : '.');
}
