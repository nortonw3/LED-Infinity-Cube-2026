// Direct port of src/cube.h:43-111 (buildCubeGeometry + buildRDNeighbors).
// The firmware is the source of truth — do not "improve" the math.

export const NUM_EDGES = 12;
export const LEDS_PER_EDGE = 32;
export const NUM_LEDS = NUM_EDGES * LEDS_PER_EDGE;

export interface Voxel { x: number; y: number; z: number; }

export const CUBE_CORNERS: Voxel[] = [
  { x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 },
  { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 1 }, { x: 1, y: 1, z: 1 }, { x: 0, y: 1, z: 1 },
];
export const EQUATOR_VERTS: Voxel[] = [
  { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 },
  { x: 0, y: 0, z: 1 }, { x: 1, y: 0, z: 1 }, { x: 0, y: 1, z: 1 },
];

export const voxels: Voxel[] = Array.from({ length: NUM_LEDS }, () => ({ x: 0, y: 0, z: 0 }));
export const graphL: number[] = new Array(NUM_LEDS).fill(0);
export const graphR: number[] = new Array(NUM_LEDS).fill(0);

function mapEdge(offset: number, x1: number, y1: number, z1: number,
                 x2: number, y2: number, z2: number): void {
  for (let i = 0; i < LEDS_PER_EDGE; i++) {
    const t = i / (LEDS_PER_EDGE - 1);
    voxels[offset + i].x = x1 + (x2 - x1) * t;
    voxels[offset + i].y = y1 + (y2 - y1) * t;
    voxels[offset + i].z = z1 + (z2 - z1) * t;
  }
}

function buildCubeGeometry(): void {
  const A = [0, 0, 0], B = [1, 0, 0], C = [1, 1, 0], D = [0, 1, 0];
  const E = [0, 0, 1], F = [1, 0, 1], G = [1, 1, 1], H = [0, 1, 1];
  // Same edge order as cube.h:57-68
  const edges = [
    [A, B], [B, F], [B, C], [C, G], [C, D], [D, H],
    [D, A], [A, E], [E, F], [F, G], [G, H], [H, E],
  ];
  edges.forEach(([p, q], e) =>
    mapEdge(e * LEDS_PER_EDGE, p[0], p[1], p[2], q[0], q[1], q[2]));
}

// Port of buildRDNeighbors (cube.h:78-106): chain LEDs along each edge, then
// weld the endpoints meeting at each of the 8 vertices into a ring so fields
// flow through corners.
function buildEdgeGraph(): void {
  const EF = [0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 6, 7]; // "from" vertex of each edge
  const ET = [1, 5, 2, 6, 3, 7, 0, 4, 5, 6, 7, 4]; // "to" vertex of each edge
  for (let e = 0; e < NUM_EDGES; e++) {
    const base = e * LEDS_PER_EDGE, last = base + LEDS_PER_EDGE - 1;
    for (let i = base; i <= last; i++) {
      graphL[i] = i > base ? i - 1 : last;
      graphR[i] = i < last ? i + 1 : base;
    }
  }
  for (let v = 0; v < 8; v++) {
    const eps: { led: number; edge: number }[] = [];
    for (let e = 0; e < NUM_EDGES; e++) {
      const base = e * LEDS_PER_EDGE, tip = base + LEDS_PER_EDGE - 1;
      if (EF[e] === v) eps.push({ led: base, edge: e });
      if (ET[e] === v) eps.push({ led: tip, edge: e });
    }
    if (eps.length < 2) continue;
    for (let a = 0; a < eps.length; a++) {
      const b = (a + 1) % eps.length;
      const ledA = eps[a].led, ledB = eps[b].led;
      const aIsBase = ledA === eps[a].edge * LEDS_PER_EDGE;
      const bIsBase = ledB === eps[b].edge * LEDS_PER_EDGE;
      if (aIsBase) graphL[ledA] = ledB; else graphR[ledA] = ledB;
      if (bIsBase) graphL[ledB] = ledA; else graphR[ledB] = ledA;
    }
  }
}

buildCubeGeometry();
buildEdgeGraph();
