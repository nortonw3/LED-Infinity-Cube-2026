import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { voxels, NUM_LEDS, NUM_EDGES, LEDS_PER_EDGE } from './geometry';
import type { CRGB } from './fastled';

const LED_RADIUS = 0.012;

export class CubeRenderer {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera: THREE.PerspectiveCamera;
  private controls: OrbitControls;
  private composer: EffectComposer;
  private leds: THREE.InstancedMesh;
  private color = new THREE.Color();
  private idleTimer = 0;

  constructor(container: HTMLElement) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.setSize(container.clientWidth, container.clientHeight);
    container.appendChild(this.renderer.domElement);

    this.camera = new THREE.PerspectiveCamera(
      45, container.clientWidth / container.clientHeight, 0.1, 100);
    this.camera.position.set(1.6, 1.2, 1.6);

    this.scene.background = new THREE.Color(0x000000);

    // ── 384 instanced LED spheres ────────────────────────────
    const geo = new THREE.SphereGeometry(LED_RADIUS, 8, 8);
    const mat = new THREE.MeshBasicMaterial({ toneMapped: false });
    this.leds = new THREE.InstancedMesh(geo, mat, NUM_LEDS);
    const m = new THREE.Matrix4();
    for (let i = 0; i < NUM_LEDS; i++) {
      m.setPosition(voxels[i].x - 0.5, voxels[i].z - 0.5, voxels[i].y - 0.5);
      this.leds.setMatrixAt(i, m);           // note: firmware z = up → three.js y
      this.leds.setColorAt(i, this.color.setRGB(0, 0, 0));
    }
    this.scene.add(this.leds);

    // ── Faint edge lines for structure ───────────────────────
    const linePts: THREE.Vector3[] = [];
    for (let e = 0; e < NUM_EDGES; e++) {
      const a = voxels[e * LEDS_PER_EDGE], b = voxels[e * LEDS_PER_EDGE + LEDS_PER_EDGE - 1];
      linePts.push(new THREE.Vector3(a.x - 0.5, a.z - 0.5, a.y - 0.5));
      linePts.push(new THREE.Vector3(b.x - 0.5, b.z - 0.5, b.y - 0.5));
    }
    const lineGeo = new THREE.BufferGeometry().setFromPoints(linePts);
    const lineMat = new THREE.LineBasicMaterial({ color: 0x14141c, toneMapped: false });
    this.scene.add(new THREE.LineSegments(lineGeo, lineMat));

    // ── Controls ─────────────────────────────────────────────
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.autoRotate = true;
    this.controls.autoRotateSpeed = 0.8;
    this.controls.addEventListener('start', () => {
      this.controls.autoRotate = false;
      window.clearTimeout(this.idleTimer);
    });
    this.controls.addEventListener('end', () => {
      window.clearTimeout(this.idleTimer);
      this.idleTimer = window.setTimeout(() => { this.controls.autoRotate = true; }, 3000);
    });

    // ── Bloom pipeline ───────────────────────────────────────
    this.composer = new EffectComposer(this.renderer);
    this.composer.addPass(new RenderPass(this.scene, this.camera));
    const bloom = new UnrealBloomPass(
      new THREE.Vector2(container.clientWidth, container.clientHeight),
      1.4,   // strength
      0.5,   // radius
      0.0);  // threshold — every lit LED blooms
    this.composer.addPass(bloom);
    this.composer.addPass(new OutputPass());

    window.addEventListener('resize', () => this.resize());
  }

  setColors(buf: CRGB[]): void {
    for (let i = 0; i < NUM_LEDS; i++) {
      this.leds.setColorAt(i, this.color.setRGB(
        buf[i].r / 255, buf[i].g / 255, buf[i].b / 255));
    }
    this.leds.instanceColor!.needsUpdate = true;
  }

  render(): void {
    this.controls.update();
    this.composer.render();
  }

  resize(): void {
    const el = this.renderer.domElement.parentElement!;
    this.camera.aspect = el.clientWidth / el.clientHeight;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(el.clientWidth, el.clientHeight);
    this.composer.setSize(el.clientWidth, el.clientHeight);
  }

  dispose(): void { this.renderer.dispose(); }
}
