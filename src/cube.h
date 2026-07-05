#pragma once

#include "globals.h"   // Voxel, NUM_LEDS/NUM_EDGES/LEDS_PER_EDGE, extern voxels + graph

////////////////////////////////////////////////////////////
// ================= CUBE TOOLKIT =================
// The shared foundation every animation builds on:
//   - geometry: voxels[] 3D positions
//   - the vertex-welded edge graph (rdNeighborL/R) that makes
//     a scalar field flow continuously along edges AND around
//     corners — the reason Reaction Diffusion looks native
//   - spatial-field helpers (diagonal, height, radius, angle,
//     distance) and graph diffusion
//
// DESIGN RULE for animations: read voxels[] for volumetric
// fields, OR propagate along the edge graph. Never key motion
// off raw index i, never treat edges as independent strips.
////////////////////////////////////////////////////////////

// ── Owned definitions (single translation unit) ───────────
Voxel voxels[NUM_LEDS];
int   rdNeighborL[NUM_LEDS];   // edge graph: left neighbor  (welded at vertices)
int   rdNeighborR[NUM_LEDS];   // edge graph: right neighbor (welded at vertices)

static float gGraphScratch[NUM_LEDS];   // private scratch for graphBlur

static const float INV_SQRT3 = 0.57735027f;
static const float TWO_PI_F   = 6.28318531f;

// The 8 cube vertices, indexed A..H = 0..7 (matches EF/ET tables below).
const Voxel CUBE_CORNERS[8] = {
  {0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}
};
// The 6 non-origin/non-far vertices, handy for "burst" targets.
const Voxel EQUATOR_VERTS[6] = {
  {1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{0,1,1}
};

////////////////////////////////////////////////////////////
// ================= GEOMETRY =================
////////////////////////////////////////////////////////////

void mapEdge(int offset, float x1, float y1, float z1,
                         float x2, float y2, float z2) {
  for (int i = 0; i < LEDS_PER_EDGE; i++) {
    float t = (float)i / (LEDS_PER_EDGE - 1);
    voxels[offset+i].x = x1 + (x2-x1)*t;
    voxels[offset+i].y = y1 + (y2-y1)*t;
    voxels[offset+i].z = z1 + (z2-z1)*t;
  }
}

void buildCubeGeometry() {
  float A[3]={0,0,0}, B[3]={1,0,0}, C[3]={1,1,0}, D[3]={0,1,0};
  float E[3]={0,0,1}, F[3]={1,0,1}, G[3]={1,1,1}, H[3]={0,1,1};
  int e = 0;
  mapEdge(e++*LEDS_PER_EDGE, A[0],A[1],A[2], B[0],B[1],B[2]);
  mapEdge(e++*LEDS_PER_EDGE, B[0],B[1],B[2], F[0],F[1],F[2]);
  mapEdge(e++*LEDS_PER_EDGE, B[0],B[1],B[2], C[0],C[1],C[2]);
  mapEdge(e++*LEDS_PER_EDGE, C[0],C[1],C[2], G[0],G[1],G[2]);
  mapEdge(e++*LEDS_PER_EDGE, C[0],C[1],C[2], D[0],D[1],D[2]);
  mapEdge(e++*LEDS_PER_EDGE, D[0],D[1],D[2], H[0],H[1],H[2]);
  mapEdge(e++*LEDS_PER_EDGE, D[0],D[1],D[2], A[0],A[1],A[2]);
  mapEdge(e++*LEDS_PER_EDGE, A[0],A[1],A[2], E[0],E[1],E[2]);
  mapEdge(e++*LEDS_PER_EDGE, E[0],E[1],E[2], F[0],F[1],F[2]);
  mapEdge(e++*LEDS_PER_EDGE, F[0],F[1],F[2], G[0],G[1],G[2]);
  mapEdge(e++*LEDS_PER_EDGE, G[0],G[1],G[2], H[0],H[1],H[2]);
  mapEdge(e++*LEDS_PER_EDGE, H[0],H[1],H[2], E[0],E[1],E[2]);
}

////////////////////////////////////////////////////////////
// ================= EDGE GRAPH =================
// Two passes: (1) chain LEDs along each edge, (2) weld the
// endpoints that meet at each shared vertex into a ring so a
// field diffuses THROUGH corners between meeting edges.
////////////////////////////////////////////////////////////

void buildRDNeighbors() {
  const int EF[12] = {0,1,1,2,2,3,3,0,4,5,6,7};   // "from" vertex of each edge
  const int ET[12] = {1,5,2,6,3,7,0,4,5,6,7,4};   // "to"   vertex of each edge
  for (int e = 0; e < NUM_EDGES; e++) {
    int base = e * LEDS_PER_EDGE, last = base + LEDS_PER_EDGE - 1;
    for (int i = base; i <= last; i++) {
      rdNeighborL[i] = (i > base) ? i - 1 : last;
      rdNeighborR[i] = (i < last) ? i + 1 : base;
    }
  }
  struct EndpointRef { int led; int edge; };
  for (int v = 0; v < 8; v++) {
    EndpointRef eps[6]; int n = 0;
    for (int e = 0; e < NUM_EDGES; e++) {
      int base = e * LEDS_PER_EDGE, tip = base + LEDS_PER_EDGE - 1;
      if (EF[e] == v) { eps[n++] = { base, e }; }
      if (ET[e] == v) { eps[n++] = { tip,  e }; }
    }
    if (n < 2) continue;
    for (int a = 0; a < n; a++) {
      int b = (a + 1) % n;
      int ledA = eps[a].led, ledB = eps[b].led;
      int baseA = eps[a].edge * LEDS_PER_EDGE; bool aIsBase = (ledA == baseA);
      int baseB = eps[b].edge * LEDS_PER_EDGE; bool bIsBase = (ledB == baseB);
      if (aIsBase) rdNeighborL[ledA] = ledB; else rdNeighborR[ledA] = ledB;
      if (bIsBase) rdNeighborL[ledB] = ledA; else rdNeighborR[ledB] = ledA;
    }
  }
}

// New-code alias — reads clearer than the historical rd* name.
inline void buildEdgeGraph() { buildRDNeighbors(); }
inline int  graphL(int i) { return rdNeighborL[i]; }
inline int  graphR(int i) { return rdNeighborR[i]; }

////////////////////////////////////////////////////////////
// ================= SPATIAL FIELD HELPERS =================
// Scalar fields over the cube volume. Prefer these over
// re-deriving the same math inline in every animation.
////////////////////////////////////////////////////////////

// Position along the main space diagonal (0,0,0)->(1,1,1), 0..1.
inline float diagonalOf(int i) {
  return (voxels[i].x + voxels[i].y + voxels[i].z) * INV_SQRT3;
}
// Height up the vertical axis, 0..1.
inline float heightOf(int i) { return voxels[i].z; }
// Cylindrical radius from the vertical center axis, 0..~0.707.
inline float radiusXY(int i) {
  float x = voxels[i].x - 0.5f, y = voxels[i].y - 0.5f;
  return sqrtf(x*x + y*y);
}
// Angle about the vertical axis, normalized 0..1.
inline float angleZ(int i) {
  float x = voxels[i].x - 0.5f, y = voxels[i].y - 0.5f;
  return atan2f(y, x) / TWO_PI_F + 0.5f;
}
// Distance from the cube center (0.5,0.5,0.5).
inline float distToCenter(int i) {
  float x = voxels[i].x - 0.5f, y = voxels[i].y - 0.5f, z = voxels[i].z - 0.5f;
  return sqrtf(x*x + y*y + z*z);
}
// Distance from LED i to cube corner c (0..7).
inline float distToCorner(int i, int c) {
  float dx = voxels[i].x - CUBE_CORNERS[c].x;
  float dy = voxels[i].y - CUBE_CORNERS[c].y;
  float dz = voxels[i].z - CUBE_CORNERS[c].z;
  return sqrtf(dx*dx + dy*dy + dz*dz);
}
// Distance from LED i to an arbitrary point.
inline float distToPoint(int i, float px, float py, float pz) {
  float dx = voxels[i].x - px, dy = voxels[i].y - py, dz = voxels[i].z - pz;
  return sqrtf(dx*dx + dy*dy + dz*dz);
}
// Gaussian falloff: 1 at d=0, narrowing with width w.
inline float gaussian(float d, float w) {
  return expf(-(d*d) / (w*w));
}

////////////////////////////////////////////////////////////
// ================= GRAPH DIFFUSION =================
// Generalizes Reaction Diffusion's 1D Laplacian: smear any
// per-LED scalar field along the welded edge graph so it
// flows across edges and around corners. `amount` in (0..0.5]
// is stable; 0.5 ≈ neighbor-average smoothing.
////////////////////////////////////////////////////////////

inline void graphBlur(float* field, int passes = 1, float amount = 0.5f) {
  for (int p = 0; p < passes; p++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      float lap = field[rdNeighborL[i]] + field[rdNeighborR[i]] - 2.0f * field[i];
      gGraphScratch[i] = field[i] + amount * lap;
    }
    memcpy(field, gGraphScratch, sizeof(float) * NUM_LEDS);
  }
}
inline void graphDiffuse(float* field, float amount = 0.5f) {
  graphBlur(field, 1, amount);
}
