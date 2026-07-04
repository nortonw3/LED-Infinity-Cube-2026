import './animations/statics';
import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { renderFrame, selectAnimation } from './engine';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

selectAnimation('static', 0);

// 45 FPS cap to match TARGET_FPS (config.h) — animation speeds are tuned to it
const FRAME_MS = 1000 / 45;
let lastFrame = 0;
function frame(nowMs: number) {
  requestAnimationFrame(frame);
  if (nowMs - lastFrame < FRAME_MS) return;
  lastFrame = nowMs;
  renderFrame(buf, nowMs * 0.001);
  cube.setColors(buf);
  cube.render();
}
requestAnimationFrame(frame);
