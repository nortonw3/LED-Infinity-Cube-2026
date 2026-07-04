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
