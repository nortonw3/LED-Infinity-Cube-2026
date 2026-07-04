import { CubeRenderer } from './renderer';
import { newBuffer } from './fastled';
import { voxels, NUM_LEDS } from './geometry';
import { applyPalette } from './palettes';

const app = document.getElementById('app')!;
const cube = new CubeRenderer(app);
const buf = newBuffer();

// TEMP test pattern: diagonal rainbow sweep (replaced in Task 6)
function frame(nowMs: number) {
  const t = nowMs * 0.001;
  for (let i = 0; i < NUM_LEDS; i++) {
    const h = (voxels[i].x + voxels[i].y + voxels[i].z) / 3;
    const v = (Math.sin(h * 6.28 + t * 2) + 1) * 0.5;
    const c = applyPalette(v);
    buf[i].r = c.r; buf[i].g = c.g; buf[i].b = c.b;
  }
  cube.setColors(buf);
  cube.render();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
