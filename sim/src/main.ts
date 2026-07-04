import './animations/statics';
import './animations/audios';
import './animations/voices';
import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { renderFrame, selectAnimation, setDemo } from './engine';
import { updateAudioFrame, startSynthetic } from './audio/sources';
import { initUI } from './ui';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

// Showcase defaults: auto-cycle the demo reel, driven by synthetic audio so
// the page is alive on load with zero setup. The UI can switch either.
selectAnimation('static', 0, true);
startSynthetic();
setDemo(true);
initUI();

// 45 FPS cap to match TARGET_FPS (config.h) — animation speeds are tuned to it
const FRAME_MS = 1000 / 45;
let lastFrame = 0;
function frame(nowMs: number) {
  requestAnimationFrame(frame);
  if (nowMs - lastFrame < FRAME_MS) return;
  lastFrame = nowMs;
  updateAudioFrame(nowMs);
  renderFrame(buf, nowMs * 0.001);
  cube.setColors(buf);
  cube.render();
}
requestAnimationFrame(frame);
