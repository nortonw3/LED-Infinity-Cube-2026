// Port of the firmware's crossfade engine, brightness scaling, and demo
// reel (LEDCube3-30-26.ino:195-222, 172-178, 504-540).
import { CRGB, newBuffer, fillBlack, blend, clamp, randomInt, millis } from './fastled';
import { startPaletteFade, NUM_PALETTES_TOTAL } from './palettes';

export type AnimFunc = (buf: CRGB[], t: number) => void;
export type Mode = 'static' | 'audio' | 'voice';

const placeholder: AnimFunc = (buf, t) => {
  const v = Math.round((Math.sin(t * 1.2) + 1) * 0.5 * 0.3 * 255);
  for (const p of buf) { p.r = v; p.g = v; p.b = v; }
};

const registry: Record<Mode, { anims: AnimFunc[]; names: string[] }> = {
  static: { anims: [placeholder], names: ['--'] },
  audio: { anims: [placeholder], names: ['--'] },
  voice: { anims: [placeholder], names: ['--'] },
};

export function registerAnimations(mode: Mode, anims: AnimFunc[], names: string[]): void {
  registry[mode] = { anims, names };
}
export function getAnimNames(mode: Mode): string[] { return registry[mode].names; }

// ── State ───────────────────────────────────────────────────────────
let currentMode: Mode = 'static';
let currentIndex = 0;
let brightness = 128;                 // .ino:29 default
let demoEnabled = false;

let currentAnim: AnimFunc = placeholder;
let nextAnim: AnimFunc = placeholder;
let transitioning = false;
let transitionStart = 0;
const transitionDuration = 2.0;       // .ino:62

const bufferA = newBuffer();
const bufferB = newBuffer();

export function getState() {
  return { mode: currentMode, index: currentIndex, brightness, demo: demoEnabled };
}

// Port of startTransition (.ino:198-202)
function startTransition(target: AnimFunc): void {
  nextAnim = target;
  transitioning = true;
  transitionStart = millis() * 0.001;
  fillBlack(bufferA);
  fillBlack(bufferB);
}

export function selectAnimation(mode: Mode, index: number, immediate = false): void {
  const { anims } = registry[mode];
  currentMode = mode;
  currentIndex = clamp(index, 0, anims.length - 1);
  if (immediate) {
    currentAnim = anims[currentIndex];
    transitioning = false;
  } else {
    startTransition(anims[currentIndex]);
  }
}

export function setBrightness(b: number): void {
  brightness = clamp(Math.round(b), 5, 255);   // BRIGHTNESS_MIN..MAX (config.h)
}

// ── Demo reel — port of demoUpdate (.ino:507-533) ───────────────────
const DEMO_INTERVAL_MS = 30000;
const demoList: [Mode, number][] = [
  ...Array.from({ length: 8 }, (_, i) => ['static', i] as [Mode, number]),
  ...Array.from({ length: 10 }, (_, i) => ['audio', i] as [Mode, number]),
  ...Array.from({ length: 4 }, (_, i) => ['voice', i] as [Mode, number]),
];
let demoLastChange = 0;
let demoStep = -1;

export function setDemo(on: boolean): void {
  demoEnabled = on;
  if (on) { demoLastChange = millis() - DEMO_INTERVAL_MS; demoStep = -1; }
}

function tickDemo(): void {
  if (!demoEnabled) return;
  const now = millis();
  if (now - demoLastChange < DEMO_INTERVAL_MS) return;
  demoLastChange = now;
  let next: number;
  do { next = randomInt(demoList.length); } while (next === demoStep && demoList.length > 1);
  demoStep = next;
  startPaletteFade(randomInt(NUM_PALETTES_TOTAL));   // random(NUM_PALETTES) incl. Rotate, .ino:527
  const [mode, idx] = demoList[demoStep];
  selectAnimation(mode, idx);
}

// ── Frame render — port of renderFrame (.ino:204-222) ───────────────
export function renderFrame(out: CRGB[], t: number): void {
  tickDemo();
  fillBlack(out);
  if (!transitioning) {
    currentAnim(out, t);
  } else {
    currentAnim(bufferA, t);
    nextAnim(bufferB, t);
    const p = (millis() * 0.001 - transitionStart) / transitionDuration;
    if (p >= 1.0) {
      transitioning = false;
      currentAnim = nextAnim;
      fillBlack(bufferA); fillBlack(bufferB);
      currentAnim(out, t);
    } else {
      const ba = Math.floor(p * 255);
      for (let i = 0; i < out.length; i++) {
        const c = blend(bufferA[i], bufferB[i], ba);
        out[i].r = c.r; out[i].g = c.g; out[i].b = c.b;
      }
    }
  }
  // FastLED.setBrightness equivalent — scale final output
  const bf = brightness / 255;
  if (bf < 1) {
    for (const p of out) {
      p.r = Math.round(p.r * bf); p.g = Math.round(p.g * bf); p.b = Math.round(p.b * bf);
    }
  }
}
