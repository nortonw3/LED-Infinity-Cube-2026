#pragma once

#include "globals.h"

////////////////////////////////////////////////////////////
// ================= BEAT DETECTOR =================
// Spectral flux onset detection with adaptive median
// threshold, tempo tracking, and beat phase output.
// Range: 60-200 BPM. Call updateBeatDetector() once
// per frame from updateAudio().
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// ================= EXPORTED GLOBALS =================
////////////////////////////////////////////////////////////

float beatPhase      = 0.0f;  // 0→1 per beat, resets on each beat
float barPhase       = 0.0f;  // 0→1 over 4 beats
float tempoConfidence= 0.0f;  // 0→1 reliability of tempo estimate
float onsetStrength  = 0.0f;  // raw spectral flux this frame
bool  beatFired      = false; // true for exactly one frame per beat
float bpm            = 120.0f;// current tempo estimate

////////////////////////////////////////////////////////////
// ================= INTERNAL STATE =================
////////////////////////////////////////////////////////////

// Spectral flux
#define FLUX_BINS       80              // bins to watch (matches your high range)
#define FLUX_HISTORY    43               // ~200ms at 90fps — median window
static float prevMag[FLUX_BINS]         = {0};
static float fluxHistory[FLUX_HISTORY]  = {0};
static int   fluxHead                   = 0;

// Inter-onset interval tracker
#define IOI_HISTORY     16               // store last 16 inter-onset intervals
static float ioiHistory[IOI_HISTORY]    = {0};
static int   ioiHead                    = 0;
static int   ioiCount                   = 0;
static unsigned long lastOnsetTime      = 0;

// Beat phase tracker
float beatPeriodMs               = 500.0f;  // default 120 BPM
static float beatAccumMs                = 0.0f;
static float barAccumBeats              = 0.0f;
static unsigned long lastFrameTime      = 0;

// Confidence
#define CONF_ATTACK     0.35f
#define CONF_DECAY      0.0001f

// BPM limits
#define BPM_MIN         60.0f
#define BPM_MAX         200.0f
#define MS_MIN          (60000.0f / BPM_MAX)   // ~300ms
#define MS_MAX          (60000.0f / BPM_MIN)   // 1000ms

////////////////////////////////////////////////////////////
// ================= MEDIAN HELPER =================
// Simple insertion sort on a small copy — fine for 43 elements
////////////////////////////////////////////////////////////

static float medianOf(float* arr, int n) {
    float tmp[FLUX_HISTORY];
    memcpy(tmp, arr, sizeof(float) * n);
    // insertion sort
    for (int i = 1; i < n; i++) {
        float key = tmp[i]; int j = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j+1] = tmp[j]; j--; }
        tmp[j+1] = key;
    }
    return tmp[n / 2];
}

////////////////////////////////////////////////////////////
// ================= IOI → BPM =================
// Given a new inter-onset interval, update the tempo
// estimate. Handles halftime/doubletime by folding IOIs
// into a common tempo hypothesis.
////////////////////////////////////////////////////////////

static void updateTempo(float newIoiMs) {
    // Reject IOIs outside trackable range
    if (newIoiMs < MS_MIN || newIoiMs > MS_MAX) {
        // Try halftime — if double the IOI is in range, use it
        float doubled = newIoiMs * 2.0f;
        if (doubled >= MS_MIN && doubled <= MS_MAX) newIoiMs = doubled;
        else return;
    }

    // Store in circular buffer
    ioiHistory[ioiHead] = newIoiMs;
    ioiHead = (ioiHead + 1) % IOI_HISTORY;
    if (ioiCount < IOI_HISTORY) ioiCount++;

    if (ioiCount < 3) return;   // need at least 3 onsets to estimate

    // Compute weighted median of stored IOIs
    // Recent IOIs weighted more heavily — copy with duplication
    float weighted[IOI_HISTORY];
    int   wCount = 0;
    for (int i = 0; i < ioiCount; i++) {
        int   age    = (ioiHead - 1 - i + IOI_HISTORY) % IOI_HISTORY;
        int   weight = (i < 4) ? 3 : (i < 8) ? 2 : 1;
        float val    = ioiHistory[age];
        for (int w = 0; w < weight && wCount < IOI_HISTORY; w++)
            weighted[wCount++] = val;
    }

    float medIoi = medianOf(weighted, wCount);

    // Slow-attack filter toward new estimate — prevents jumping on outliers
    float targetBpm = 60000.0f / medIoi;
    float alpha     = 0.25f;
    bpm             = bpm * (1.0f - alpha) + targetBpm * alpha;
    bpm             = constrain(bpm, BPM_MIN, BPM_MAX);
    beatPeriodMs    = 60000.0f / bpm;

    // Boost confidence
    tempoConfidence = constrain(tempoConfidence + CONF_ATTACK * (float)ioiCount * 0.1f, 0.0f, 1.0f);
}

////////////////////////////////////////////////////////////
// ================= MAIN UPDATE =================
////////////////////////////////////////////////////////////

void updateBeatDetector() {
    unsigned long now     = millis();
    float         deltaMs = (lastFrameTime > 0) ? (float)(now - lastFrameTime) : 11.0f;
    deltaMs               = constrain(deltaMs, 1.0f, 50.0f);
    lastFrameTime         = now;

    beatFired = false;

    // ── Spectral flux ─────────────────────────────────────
    float flux = 0;
        for (int i = 1; i <= 6; i++) {   // bins 1-6 = 43-258 Hz — kick only
            float mag  = fftBins[i];
            float diff = mag - prevMag[i];
            if (diff > 0) flux += diff;
            prevMag[i] = mag;
        }

        for (int i = 7; i < FLUX_BINS; i++) prevMag[i] = fftBins[i];
    onsetStrength = flux;

    // Store in circular history
    fluxHistory[fluxHead] = flux;
    fluxHead = (fluxHead + 1) % FLUX_HISTORY;

    // Adaptive threshold = median * sensitivity factor
    float threshold = medianOf(fluxHistory, FLUX_HISTORY) * .8f;

    // ── Onset detection ───────────────────────────────────
    bool onsetDetected = (flux > threshold) && (flux > 0.0005f);

    if (onsetDetected) {
        unsigned long ioi = now - lastOnsetTime;
        if (lastOnsetTime > 0 && ioi > 80) {   // debounce — ignore onsets < 80ms apart
            updateTempo((float)ioi);
        }
        lastOnsetTime = now;
    }

    // ── Confidence decay ──────────────────────────────────
    tempoConfidence = constrain(tempoConfidence - CONF_DECAY, 0.0f, 1.0f);

    // ── Beat phase advance ────────────────────────────────
    beatAccumMs += deltaMs;

    if (beatAccumMs >= beatPeriodMs) {
        beatAccumMs  -= beatPeriodMs;
        beatFired     = true;
        barAccumBeats = fmodf(barAccumBeats + 1.0f, 4.0f);
    }

    // Resync phase to onset when confidence is high
    // and onset lands near an expected beat boundary
    if (onsetDetected && tempoConfidence > 0.5f) {
        float phaseErr = beatAccumMs / beatPeriodMs;   // 0→1
        // If onset is within 15% of expected beat, nudge phase toward it
        if (phaseErr < 0.15f || phaseErr > 0.85f) {
            beatAccumMs *= 0.5f;   // pull phase toward zero (beat boundary)
        }
    }

    beatPhase = beatAccumMs / beatPeriodMs;             // 0→1
    barPhase  = (barAccumBeats + beatPhase) / 4.0f;    // 0→1 over 4 beats
}

////////////////////////////////////////////////////////////
// ================= DEBUG PRINT =================
////////////////////////////////////////////////////////////

void printBeatStatus() {
    Serial.println(F("\n--- Beat detector ---"));
    Serial.print(F("BPM        : ")); Serial.println(bpm, 1);
    Serial.print(F("Confidence : ")); Serial.println(tempoConfidence, 3);
    Serial.print(F("Beat phase : ")); Serial.println(beatPhase, 3);
    Serial.print(F("Bar phase  : ")); Serial.println(barPhase, 3);
    Serial.print(F("Onset str  : ")); Serial.println(onsetStrength, 4);
    Serial.print(F("IOI count  : ")); Serial.println(ioiCount);
}

bool  beatDebugEnabled  = false;
unsigned long lastBeatDebug = 0;
#define BEAT_DEBUG_INTERVAL_MS 200

void printBeatDebug() {
    if (!beatDebugEnabled) return;
    if (millis() - lastBeatDebug < BEAT_DEBUG_INTERVAL_MS) return;
    lastBeatDebug = millis();
    Serial.print(F("BPM:")); Serial.print(bpm, 1);
    Serial.print(F(" Conf:")); Serial.print(tempoConfidence, 2);
    Serial.print(F(" Phase:")); Serial.print(beatPhase, 2);
    Serial.print(F(" Bar:")); Serial.print(barPhase, 2);
    Serial.print(F(" Onset:")); Serial.print(onsetStrength, 4);
    Serial.print(F(" BEAT:")); Serial.println(beatFired ? F("*") : F("."));
}