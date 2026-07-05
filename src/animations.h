#pragma once

#include "globals.h"

////////////////////////////////////////////////////////////
// ================= PLACEHOLDER =================
////////////////////////////////////////////////////////////

void placeholderAnim(CRGB* buf, float t) {
  float v = (sinf(t * 1.2f) + 1.0f) * 0.5f * 0.3f;
  for (int i = 0; i < NUM_LEDS; i++) buf[i] = applyPalette(v);
}

////////////////////////////////////////////////////////////
// ================= STATIC ANIMATIONS =================
////////////////////////////////////////////////////////////

// Static0: Diagonal Flow
void staticDiagonalFlow(CRGB* buf, float t) {
  const float inv_sqrt3 = 0.57735f;
  float speed = 0.4f, width = 0.18f;
  float bandPos = fmodf(t * speed, 1.0f);
  for (int i = 0; i < NUM_LEDS; i++) {
    float h  = (voxels[i].x + voxels[i].y + voxels[i].z) * inv_sqrt3;
    float d0 = fabsf(h - bandPos);
    float d1 = fabsf(h - bandPos + 1.0f);
    float d2 = fabsf(h - bandPos - 1.0f);
    float d  = min(d0, min(d1, d2));
    buf[i]   = applyPalette(expf(-d * d / (width * width) * 4.0f));
  }
}

// ── Static1: Lissajous Tracer ─────────────────────────────
// Three oscillators at irrational frequency ratios drive a
// point through 3D space tracing a never-repeating Lissajous
// figure. A trail of history positions fades behind the point.
// The oscillator frequencies are close enough to musical
// intervals that the figure slowly morphs between recognisable
// geometric forms — Bowditch curves, knots, pretzel shapes.
// In audio mode the frequencies and amplitudes are driven by
// bass/mid/high so the figure breathes and shifts with music.

#define LISS_HISTORY  80        // trail length in frames
#define LISS_HALO     0.12f     // radius in voxel space for LED illumination

struct LissPoint { float x, y, z; };
static LissPoint lissHistory[LISS_HISTORY];
static int       lissHead    = 0;
static bool      lissInited  = false;

void staticLissajous(CRGB* buf, float t) {
    if (!lissInited) {
        for (int i = 0; i < LISS_HISTORY; i++)
            lissHistory[i] = {0.5f, 0.5f, 0.5f};
        lissInited = true;
    }

    fill_solid(buf, NUM_LEDS, CRGB::Black);

    // ── Oscillator frequencies ────────────────────────────
    // Three irrational ratios — figure never exactly repeats.
    // φ = 1.618..., δ = 1.324... (plastic constant),
    // ε = 1.732... (√3)
    // In audio mode these shift with the music.
    float fA = 1.000f;
    float fB = 1.618f;
    float fC = 1.324f;

    // Phase offsets give the figure its initial twist
    float phA = 0.0f;
    float phB = 0.7854f;   // π/4
    float phC = 1.5708f;   // π/2

    // Slow drift — makes the figure morph over minutes
    float drift = t * 0.007f;

    // Current position — push amplitude to 0.5 so the figure
    // reaches all six faces and regularly crosses the edges
    float px = 0.5f + 0.50f * sinf(fA * t * 1.3f + phA + drift);
    float py = 0.5f + 0.50f * sinf(fB * t * 1.3f + phB + drift * 1.3f);
    float pz = 0.5f + 0.50f * sinf(fC * t * 1.3f + phC + drift * 0.7f);

    // Clamp to cube — at amplitude 0.5 the point grazes the faces
    px = constrain(px, 0.0f, 1.0f);
    py = constrain(py, 0.0f, 1.0f);
    pz = constrain(pz, 0.0f, 1.0f);

    // Store in history ring buffer
    lissHistory[lissHead] = {px, py, pz};
    lissHead = (lissHead + 1) % LISS_HISTORY;

    // ── Render trail ──────────────────────────────────────
    // Walk history from newest to oldest, fading brightness
    for (int s = 0; s < LISS_HISTORY; s++) {
        int idx = ((lissHead - 1 - s) % LISS_HISTORY + LISS_HISTORY) % LISS_HISTORY;
        float age    = (float)s / (float)LISS_HISTORY;
        float bright = (1.0f - age);       // linear fade
        bright       = bright * bright;    // quadratic — head is much brighter
        if (bright < 0.01f) continue;

        float hx = lissHistory[idx].x;
        float hy = lissHistory[idx].y;
        float hz = lissHistory[idx].z;

        // Find all LEDs within halo radius and add contribution
        float halo2 = LISS_HALO * LISS_HALO;
        for (int j = 0; j < NUM_LEDS; j++) {
            float dx = voxels[j].x - hx;
            float dy = voxels[j].y - hy;
            float dz = voxels[j].z - hz;
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > halo2 * 4.0f) continue;
            float b = bright * expf(-d2 / (halo2 * 0.5f));
            b = constrain(b, 0.0f, 1.0f);
            CRGB c = applyPalette(b);
            buf[j].r = qadd8(buf[j].r, c.r);
            buf[j].g = qadd8(buf[j].g, c.g);
            buf[j].b = qadd8(buf[j].b, c.b);
        }
    }
}

// ── Static2: Gravity Particle System ─────────────────────
// Full 3D physics — particles spawn near the top vertex,
// fall under gravity, bounce off the six cube faces with
// energy loss. Each particle renders on the nearest edge
// LED in 3D space. No edge-graph constraints — pure physics.

#define GRAV_COUNT      24
#define GRAV_GRAVITY    0.35f     // units/s² downward (z axis)
#define GRAV_RESTITUTION 0.55f   // bounce energy retention
#define GRAV_DRAG       0.995f   // air resistance per frame
#define GRAV_SPAWN_RATE 900UL    // ms between spawns

struct GravParticle {
    bool  active;
    float x, y, z;           // position 0..1
    float vx, vy, vz;        // velocity units/s
    float life;               // 1.0 → 0.0 brightness envelope
    float decay;              // life decay rate per frame
};

static GravParticle gravP[GRAV_COUNT];
static bool         gravInited       = false;
static unsigned long gravLastSpawn   = 0;
static float        gravLastT        = 0;

void gravSpawn() {
    for (int i = 0; i < GRAV_COUNT; i++) {
        if (!gravP[i].active) {
            // Spawn near top vertex G(1,1,1) with random scatter
            gravP[i].active = true;
            gravP[i].x = 0.85f + (random(30) - 15) * 0.005f;
            gravP[i].y = 0.85f + (random(30) - 15) * 0.005f;
            gravP[i].z = 0.85f + (random(20) - 10) * 0.005f;
            // Initial velocity — mostly downward with random spread
            gravP[i].vx = (random(100) - 50) * 0.003f;
            gravP[i].vy = (random(100) - 50) * 0.003f;
            gravP[i].vz = -(random(50))       * 0.002f;   // downward bias
            gravP[i].life  = 1.0f;
            gravP[i].decay = 0.004f + random(30) * 0.0001f;
            return;
        }
    }
}

void staticGravityParticles(CRGB* buf, float t) {
    if (!gravInited) {
        for (int i = 0; i < GRAV_COUNT; i++) gravP[i].active = false;
        gravInited = true;
    }

    // Delta time — capped to prevent physics explosions on lag frames
    float dt = t - gravLastT;
    if (dt <= 0 || dt > 0.1f) dt = 0.016f;
    gravLastT = t;

    fill_solid(buf, NUM_LEDS, CRGB::Black);

    // Spawn new particles
    unsigned long now = millis();
    if (now - gravLastSpawn > GRAV_SPAWN_RATE) {
        int burst = 1 + random(3);
        for (int b = 0; b < burst; b++) gravSpawn();
        gravLastSpawn = now;
    }

    // Update physics
    for (int i = 0; i < GRAV_COUNT; i++) {
        if (!gravP[i].active) continue;

        // Apply gravity and drag
        gravP[i].vz -= GRAV_GRAVITY * dt;
        gravP[i].vx *= GRAV_DRAG;
        gravP[i].vy *= GRAV_DRAG;
        gravP[i].vz *= GRAV_DRAG;

        // Integrate position
        gravP[i].x += gravP[i].vx * dt;
        gravP[i].y += gravP[i].vy * dt;
        gravP[i].z += gravP[i].vz * dt;

        // Bounce off cube faces [0..1]
        if (gravP[i].x < 0.0f) { gravP[i].x = 0.0f; gravP[i].vx = fabsf(gravP[i].vx) * GRAV_RESTITUTION; }
        if (gravP[i].x > 1.0f) { gravP[i].x = 1.0f; gravP[i].vx = -fabsf(gravP[i].vx) * GRAV_RESTITUTION; }
        if (gravP[i].y < 0.0f) { gravP[i].y = 0.0f; gravP[i].vy = fabsf(gravP[i].vy) * GRAV_RESTITUTION; }
        if (gravP[i].y > 1.0f) { gravP[i].y = 1.0f; gravP[i].vy = -fabsf(gravP[i].vy) * GRAV_RESTITUTION; }
        if (gravP[i].z < 0.0f) { gravP[i].z = 0.0f; gravP[i].vz = fabsf(gravP[i].vz) * GRAV_RESTITUTION; }
        if (gravP[i].z > 1.0f) { gravP[i].z = 1.0f; gravP[i].vz = -fabsf(gravP[i].vz) * GRAV_RESTITUTION; }

        // Decay life
        gravP[i].life -= gravP[i].decay;
        if (gravP[i].life <= 0.0f) { gravP[i].active = false; continue; }

        // Illuminate a soft halo on nearby LEDs
        float px = gravP[i].x, py = gravP[i].y, pz = gravP[i].z;
        for (int j = 0; j < NUM_LEDS; j++) {
            float dx = voxels[j].x - px;
            float dy = voxels[j].y - py;
            float dz = voxels[j].z - pz;
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 > 0.04f) continue;   // only LEDs within ~0.2 units
            float b = gravP[i].life * expf(-d2 * 40.0f);
            b = constrain(b, 0.0f, 1.0f);
            CRGB c = applyPalette(b);
            buf[j].r = qadd8(buf[j].r, c.r);
            buf[j].g = qadd8(buf[j].g, c.g);
            buf[j].b = qadd8(buf[j].b, c.b);
        }
    }
}

// (Static3 Sparkle cut — random per-index twinkle had no cube structure.)

// Static4: Reaction Diffusion
#define RD_DA    0.2f
#define RD_DB    0.08f
#define RD_FEED  0.023f
#define RD_KILL  0.049f
#define RD_DT    0.25f
#define RD_STEPS 6

bool rdReady = false;

void rdSeedEdges() {
  for (int i=0;i<NUM_LEDS;i++) { rdA[i]=1.0f; rdB[i]=0.0f; }
  randomSeed(42);
  for (int e=0;e<NUM_EDGES;e++) {
    int base=e*LEDS_PER_EDGE, last=base+LEDS_PER_EDGE-1;
    int seeds=2+random(3);
    for (int s=0;s<seeds;s++) {
      int cx=base+random(3,LEDS_PER_EDGE-3);
      for (int d=-3;d<=3;d++) { int idx=constrain(cx+d,base,last); rdA[idx]=0.3f; rdB[idx]=0.4f; }
    }
  }
}

void rdNudgeIfDead() {
  static unsigned long lastNudge=0, lastRandSeed=0;
  unsigned long now=millis();
  if (now-lastRandSeed>=10000) {
    lastRandSeed=now; int e=random(NUM_EDGES), base=e*LEDS_PER_EDGE;
    int cx=base+random(3,LEDS_PER_EDGE-3);
    for (int d=-2;d<=2;d++) { int idx=constrain(cx+d,base,base+LEDS_PER_EDGE-1); rdA[idx]=0.3f; rdB[idx]=0.4f; }
  }
  if (now-lastNudge<3000) return; lastNudge=now;
  float total=0; for (int i=0;i<NUM_LEDS;i++) total+=rdB[i];
  if (total<1.5f) {
    for (int e=0;e<NUM_EDGES;e++) {
      int base=e*LEDS_PER_EDGE, cx=base+random(3,LEDS_PER_EDGE-3);
      for (int d=-2;d<=2;d++) { int idx=constrain(cx+d,base,base+LEDS_PER_EDGE-1); rdA[idx]=0.3f; rdB[idx]=0.4f; }
    }
  }
}

// buildRDNeighbors() now lives in cube.h (the shared edge graph).

void rdStepAll() {
  for (int i = 0; i < NUM_LEDS; i++) {
    float a = rdA[i], b = rdB[i];
    int   L = rdNeighborL[i], R = rdNeighborR[i];
    float lapA = rdA[L] + rdA[R] - 2.0f * a;
    float lapB = rdB[L] + rdB[R] - 2.0f * b;
    float rxn  = a * b * b;
    rdA2[i] = constrain(a + RD_DT * (RD_DA * lapA - rxn + RD_FEED * (1.0f - a)), 0.0f, 1.0f);
    rdB2[i] = constrain(b + RD_DT * (RD_DB * lapB + rxn - (RD_KILL + RD_FEED) * b), 0.0f, 1.0f);
  }
  memcpy(rdA, rdA2, sizeof(float) * NUM_LEDS);
  memcpy(rdB, rdB2, sizeof(float) * NUM_LEDS);
}

void staticReactionDiffusion(CRGB* buf, float t) {
  if (!rdReady) { rdSeedEdges(); rdReady=true; }
  rdNudgeIfDead();
  for (int s=0;s<RD_STEPS;s++) rdStepAll();
  for (int i=0;i<NUM_LEDS;i++) buf[i]=applyPalette(constrain(rdB[i]*3.5f,0.0f,1.0f));
}

// Static: Coral — a second reaction-diffusion in a coral/maze
// regime (different feed/kill from RD). Same welded-graph
// diffusion, but grows branching labyrinth textures instead of
// travelling spots. Has its own chemical fields so it can run
// alongside RD (e.g. during a crossfade) without interfering.
#define CORAL_DA    0.2f
#define CORAL_DB    0.08f
#define CORAL_FEED  0.0545f
#define CORAL_KILL  0.0620f
#define CORAL_DT    0.25f
#define CORAL_STEPS 6
static float coralA[NUM_LEDS], coralB[NUM_LEDS];
static float coralA2[NUM_LEDS], coralB2[NUM_LEDS];
static bool  coralReady = false;

void coralSeed() {
  for (int i=0;i<NUM_LEDS;i++) { coralA[i]=1.0f; coralB[i]=0.0f; }
  for (int e=0;e<NUM_EDGES;e++) {
    int base=e*LEDS_PER_EDGE, last=base+LEDS_PER_EDGE-1;
    int seeds=2+random(3);
    for (int s=0;s<seeds;s++) {
      int cx=base+random(3,LEDS_PER_EDGE-3);
      for (int d=-3;d<=3;d++) { int idx=constrain(cx+d,base,last); coralA[idx]=0.3f; coralB[idx]=0.4f; }
    }
  }
}

void staticCoral(CRGB* buf, float t) {
  if (!coralReady) { coralSeed(); coralReady=true; }
  // reseed watchdog — keep the reaction perpetually alive
  static unsigned long lastSeed=0; unsigned long now=millis();
  if (now-lastSeed>3000) {
    lastSeed=now; float tot=0; for (int i=0;i<NUM_LEDS;i++) tot+=coralB[i];
    if (tot<1.5f) coralSeed();
  }
  for (int s=0;s<CORAL_STEPS;s++) {
    for (int i=0;i<NUM_LEDS;i++) {
      float a=coralA[i], b=coralB[i];
      int L=rdNeighborL[i], R=rdNeighborR[i];
      float lapA=coralA[L]+coralA[R]-2.0f*a;
      float lapB=coralB[L]+coralB[R]-2.0f*b;
      float rxn=a*b*b;
      coralA2[i]=constrain(a+CORAL_DT*(CORAL_DA*lapA-rxn+CORAL_FEED*(1.0f-a)),0.0f,1.0f);
      coralB2[i]=constrain(b+CORAL_DT*(CORAL_DB*lapB+rxn-(CORAL_KILL+CORAL_FEED)*b),0.0f,1.0f);
    }
    memcpy(coralA,coralA2,sizeof(float)*NUM_LEDS);
    memcpy(coralB,coralB2,sizeof(float)*NUM_LEDS);
  }
  for (int i=0;i<NUM_LEDS;i++) buf[i]=applyPalette(constrain(coralB[i]*3.5f,0.0f,1.0f));
}

// Static5: Mobius Bend
void staticMobiusBraid(CRGB* buf, float t) {
    fill_solid(buf, NUM_LEDS, CRGB::Black);

    // Three wave carriers, each wound on a different axis pair.
    // Frequencies are irrational ratios so the pattern never
    // perfectly repeats.
    const float freqs[3]  = { 1.000f, 1.618f, 2.414f };  // 1, phi, silver ratio
    const float speeds[3] = { 0.29f,  0.19f,  0.13f  };
    const float phases[3] = { 0.0f,   2.094f, 4.189f };   // 0, 2π/3, 4π/3

    for (int i = 0; i < NUM_LEDS; i++) {
        float x = voxels[i].x, y = voxels[i].y, z = voxels[i].z;

        float v = 0;
        for (int w = 0; w < 3; w++) {
            // Each wave is wound around a different axis pair
            // so the braid crosses all three spatial dimensions.
            float arg;
            if      (w == 0) arg = (x - y) * freqs[w] * 6.2832f + t * speeds[w] * 6.2832f + phases[w];
            else if (w == 1) arg = (y - z) * freqs[w] * 6.2832f + t * speeds[w] * 6.2832f + phases[w];
            else             arg = (z - x) * freqs[w] * 6.2832f + t * speeds[w] * 6.2832f + phases[w];

            // Sharpen the sine into a narrow bright ridge
            float s = (sinf(arg) + 1.0f) * 0.5f;
            s = s * s * s * s;   // quartic sharpening — tight knot, dark between
            v += s;
        }

        // Normalise to 0..1 — three waves can sum to 3.0 at peak
        v = constrain(v / 3.0f, 0.0f, 1.0f);

        // Lift the brightness curve so dim regions stay visible
        // but peaks still punch to full brightness
        v = v * v * (3.0f - 2.0f * v);   // smoothstep

        buf[i] = applyPalette(v);
    }
}

// (Static6 EdgeBreathe cut — pulsed edges in wiring order, not spatial order.)

// Static7: Plasma Cube
void staticPlasmaCube(CRGB* buf, float t) {
  for (int i = 0; i < NUM_LEDS; i++) {
    float x=voxels[i].x, y=voxels[i].y, z=voxels[i].z;
    float v  = sinf(x*4.0f+t*1.1f);
          v += sinf(y*4.0f+t*0.9f+1.0f);
          v += sinf(z*4.0f+t*1.3f+2.0f);
          v += sinf((x+y+z)*3.0f+t*0.7f);
          v += sinf(sqrtf(x*x+y*y+z*z+0.01f)*6.0f-t*1.5f);
    buf[i] = applyPalette((v/5.0f+1.0f)*0.5f);
  }
}

// (Static8 Zipper cut — edge pairing was a hand-picked index table, not geometry.)

// Static: Noise Worms — 3D Perlin sampled at each LED's actual
// position, so worms flow continuously across edges and around
// corners instead of being 12 independent per-edge strips.
void staticNoiseWorms(CRGB* buf, float t) {
  uint32_t ti=(uint32_t)(t*40.0f);
  for (int i=0;i<NUM_LEDS;i++) {
    uint8_t n=inoise8((uint16_t)(voxels[i].x*160.0f + ti),
                      (uint16_t)(voxels[i].y*160.0f + ti/2),
                      (uint16_t)(voxels[i].z*160.0f));
    float v=n/255.0f; v=v*v;   // gamma → dark troughs, bright crests
    buf[i]=applyPalette(v);
  }
}

////////////////////////////////////////////////////////////
// ================= AUDIO ANIMATIONS =================
////////////////////////////////////////////////////////////

// ── Audio0: Tri Axis ──────────────────────────────────────
// Beat: bass blob snaps to full on beatFired, decays with
// beatPhase. High burst locks to barPhase at high confidence.

static const float equatorialVerts[6][3]={{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{0,1,1}};
static float burstX=0.5f,burstY=0.5f,burstZ=0.5f,burstEnv=0.0f,lastHigh=0.0f;

void audioTriAxis(CRGB* buf, float t) {
    const float inv_sqrt3=0.57735f;

    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired; float beatPhase = audio.beatPhase;
    float tempoConfidence = audio.tempoConfidence;

    // Beat-locked burst
    if (beatFired && tempoConfidence > 0.25f) {
        int v=random(6);
        burstX=equatorialVerts[v][0]; burstY=equatorialVerts[v][1];
        burstZ=equatorialVerts[v][2]; burstEnv=0.6f+bass*0.6f;
    } else if (high>lastHigh*1.35f&&high>0.04f&&burstEnv<0.2f) {
        int v=random(6);
        burstX=equatorialVerts[v][0]; burstY=equatorialVerts[v][1];
        burstZ=equatorialVerts[v][2]; burstEnv=1.0f;
    }
    lastHigh=high; burstEnv*=0.88f;

    // Beat phase modulates bass blob size — pulses in time
    float beatMod = (tempoConfidence > 0.25f) ?
                    (1.0f + 0.4f * (1.0f - beatPhase)) : 1.0f;

    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
        float h=(x+y+z)*inv_sqrt3;
        float bW=(0.15f+bass*0.6f)*beatMod;
        float bassV=bass*2.5f*expf(-(h*h)/(bW*bW));
        float mW=0.15f+mid*0.6f, mH=1.0f-h;
        float midV=mid*2.5f*expf(-(mH*mH)/(mW*mW));
        float dx=x-burstX,dy=y-burstY,dz=z-burstZ;
        float highV=burstEnv*expf(-(dx*dx+dy*dy+dz*dz)*8.0f);
        buf[i]=applyPalette(constrain(bassV+midV+highV,0.0f,1.0f));
    }
}

// ── Audio1: Impact ────────────────────────────────────────
// Beat: shockwaves spawn on beatFired at high confidence.
// Ring radius at spawn biased by beatPhase position.

#define SHOCK_COUNT 4
struct Shockwave { float radius,speed,cx,cy,cz,env; bool active; };
static Shockwave shocks[SHOCK_COUNT];
static float shockLastBass=0,highLast=0;
static bool  shocksInited=false;

void audioImpact(CRGB* buf, float t) {
    if (!shocksInited) {
        for(int s=0;s<SHOCK_COUNT;s++) shocks[s].active=false;
        for(int i=0;i<NUM_LEDS;i++) sparkEnvs[i]=0;
        shocksInited=true;
    }

    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float tempoConfidence = audio.tempoConfidence;

    // Spawn on beat when confident, otherwise on bass transient
    bool spawnShock = (beatFired && tempoConfidence > 0.25f && bass > 0.02f) ||
                      (bass > shockLastBass*1.35f && bass > 0.05f);
    if (spawnShock) {
        for (int s=0;s<SHOCK_COUNT;s++) {
            if (!shocks[s].active) {
                shocks[s]={0, 0.55f+bass*0.8f,
                           (float)random(2),(float)random(2),(float)random(2),
                           0.6f+bass*1.2f, true};
                break;
            }
        }
    }
    shockLastBass=bass;

    if (high>highLast*1.4f&&high>0.04f) {
        int cnt=3+(int)(high*20.0f);
        for(int k=0;k<cnt;k++) sparkEnvs[random(NUM_LEDS)]=0.8f+high*0.5f;
    }
    highLast=high;

    for (int s=0;s<SHOCK_COUNT;s++) {
        if(!shocks[s].active) continue;
        shocks[s].radius+=shocks[s].speed*0.011f;
        shocks[s].env*=0.94f;
        if(shocks[s].radius>2.0f||shocks[s].env<0.02f) shocks[s].active=false;
    }
    for (int i=0;i<NUM_LEDS;i++) sparkEnvs[i]*=0.78f;

    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
        float nx=inoise8(x*110+mid*180,y*110,z*110+t*18)/255.0f;
        float ny=inoise8(y*110+50,z*110,x*110+t*15)/255.0f;
        float seethe=((nx+ny)*0.5f)*mid*2.2f, shockV=0;
        for (int s=0;s<SHOCK_COUNT;s++) {
            if(!shocks[s].active) continue;
            float dx=x-shocks[s].cx,dy=y-shocks[s].cy,dz=z-shocks[s].cz;
            float d=sqrtf(dx*dx+dy*dy+dz*dz),dr=d-shocks[s].radius;
            shockV+=shocks[s].env*expf(-dr*dr*35.0f);
        }
        buf[i]=applyPalette(constrain(seethe+shockV+sparkEnvs[i],0.0f,1.0f));
    }
}

// ── Audio2: Cellular Automaton ────────────────────────────
// Beat: seeding rate jumps on beatFired. Infection strength
// pulses with barPhase for a 4-beat breathing cycle.

#define CELL_COUNT NUM_LEDS
static float cellState[CELL_COUNT]={0};
static float cellNext[CELL_COUNT]={0};
static float cellLastBass=0,cellLastHigh=0;
static bool  cellInited=false;

void audioCell(CRGB* buf, float t) {
    if (!cellInited) {
        for(int i=0;i<CELL_COUNT;i++) cellState[i]=0;
        cellInited=true;
    }

    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float barPhase = audio.barPhase;
    float tempoConfidence = audio.tempoConfidence;

    // Beat-locked seeding — bigger burst on beat
    bool doSeed = (beatFired && tempoConfidence > 0.25f) ||
                  (bass > cellLastBass*1.3f && bass > 0.04f);
    if (doSeed) {
        float seedStr = beatFired ? (0.6f + bass*0.6f) : (0.5f + bass*0.5f);
        int seeds = beatFired ? (5 + (int)(bass*25.0f)) : (3 + (int)(bass*20.0f));
        for (int k=0;k<seeds;k++) {
            int idx=random(NUM_LEDS);
            cellState[idx]=seedStr;
            int L=rdNeighborL[idx],R=rdNeighborR[idx];
            cellState[L]=max(cellState[L],seedStr*0.7f);
            cellState[R]=max(cellState[R],seedStr*0.7f);
        }
    }
    cellLastBass=bass;

    if (high>cellLastHigh*1.3f&&high>0.03f) {
        int mutations=2+(int)(high*15.0f);
        for(int k=0;k<mutations;k++) cellState[random(NUM_LEDS)]=0.4f+high*0.5f;
    }
    cellLastHigh=high;

    // barPhase breathes the infection strength over 4 beats
    float barMod = (tempoConfidence > 0.25f) ?
                   (0.85f + 0.3f * sinf(barPhase * 6.2832f)) : 1.0f;

    float infectThresh  = 0.25f + mid*0.25f;
    float infectStrength= (0.55f + mid*0.35f) * barMod;
    float decay         = 0.964f - mid*0.018f;

    for (int i=0;i<NUM_LEDS;i++) {
        float s=cellState[i];
        int   L=rdNeighborL[i],R=rdNeighborR[i];
        float spread=(s>infectThresh)?s*infectStrength:0;
        cellNext[i]=max(s,max(cellState[L],cellState[R])*infectStrength);
        cellNext[i]=max(cellNext[i],spread);
        cellNext[i]*=decay;
        cellNext[i]=constrain(cellNext[i],0.0f,1.0f);
    }
    memcpy(cellState,cellNext,sizeof(float)*NUM_LEDS);
    for (int i=0;i<NUM_LEDS;i++) buf[i]=applyPalette(cellState[i]);
}

// (Audio3 FreqBands cut — spectrum-by-height folded into SpectrumHelix.)

// ── Audio4: Bass Bloom ────────────────────────────────────
// Beat: bloom fires on beatFired. Expansion speed derived
// from tempo so bloom peaks at next beat boundary.

static float bloomRadius=0,bloomEnv=0,bloomLastBass=0;

void audioBassBloom(CRGB* buf, float t) {
    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float tempoConfidence = audio.tempoConfidence;
    float beatPeriodMs = 60000.0f / audio.bpm;

    // Fire on beat when confident, otherwise on bass transient
    bool doBloom = (beatFired && tempoConfidence > 0.25f && bass > 0.02f) ||
                   (bass > bloomLastBass*1.3f && bass > 0.04f);
    if (doBloom) {
        bloomRadius=0.0f;
        bloomEnv=0.5f+bass*1.5f;
    }
    bloomLastBass=bass;

    // Expansion speed: tempo-matched so bloom fills cube in one beat
    float expandSpeed = (tempoConfidence > 0.25f) ?
                        (1.8f / (beatPeriodMs * 0.001f)) * 0.011f :
                        0.018f;
    bloomRadius+=expandSpeed;
    bloomEnv*=0.91f;

    for (int i=0;i<NUM_LEDS;i++) {
        float dx=voxels[i].x-0.5f,dy=voxels[i].y-0.5f,dz=voxels[i].z-0.5f;
        float d=sqrtf(dx*dx+dy*dy+dz*dz),dr=d-bloomRadius;
        float bloom=bloomEnv*expf(-dr*dr*50.0f);
        float hum=mid*0.4f*(sinf(t*3.0f+d*8.0f)*0.5f+0.5f);
        float spark=(inoise8(i*13,(uint16_t)(t*90.0f))/255.0f)*high*2.0f;
        buf[i]=applyPalette(constrain(bloom+hum+spark,0.0f,1.0f));
    }
}

// ── Audio5: Vortex ────────────────────────────────────────
// Beat: spin speed snaps to tempo multiples at high confidence.
// Arm width pulses on beat for a breathing vortex.

void audioVortex(CRGB* buf, float t) {
    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    float beatPhase = audio.beatPhase;
    float bpm = audio.bpm;
    float tempoConfidence = audio.tempoConfidence;

    // Lock spin speed to tempo when confident
    float spinBase;
    if (tempoConfidence > 0.5f) {
        // spin exactly one revolution per beat
        spinBase = bpm / 60.0f;
    } else {
        spinBase = 1.5f + mid*8.0f;
    }
    float spin = t * spinBase;

    // Arm width pulses with beatPhase
    float beatPulse  = (tempoConfidence > 0.25f) ?
                       (1.0f - 0.3f * beatPhase) : 1.0f;
    float armWidth   = max((0.25f + bass*0.4f) * beatPulse, 0.01f);

    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x-0.5f,y=voxels[i].y-0.5f,z=voxels[i].z;
        float r2=x*x+y*y;
        float normAng=(r2<1e-6f)?0.0f:(atan2f(y,x)/6.2832f+0.5f);
        float armPhase=fmodf(normAng-z*0.5f-spin*0.05f+2.0f,1.0f);
        float dArm=armPhase-0.5f;
        float v=expf(-dArm*dArm/(armWidth*armWidth));
        float spark=(inoise8(i*17,(uint16_t)(t*80.0f))/255.0f)*high*1.5f;
        buf[i]=applyPalette(constrain(v*(0.1f+bass*0.5f)+spark,0.0f,1.0f));
    }
}

// (Audio6 Resonance cut — per-edge standing waves lacked cross-corner coherence.)

// ── Audio7: Pulse Web ─────────────────────────────────────
// Beat: pulses spawn on beatFired. Decay rate matched to
// beat interval so pulses fade just before the next beat.

#define PWEB_COUNT 8
struct PulseWeb { bool active; float radius,env,speed,cx,cy,cz; };
static PulseWeb    pwebs[PWEB_COUNT];
static float       pwebLastBass=0;
static bool        pwebInited=false;
static const float cubeCorners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};

void audioPulseWeb(CRGB* buf, float t) {
    if (!pwebInited) {
        for(int i=0;i<PWEB_COUNT;i++) pwebs[i].active=false;
        pwebInited=true;
    }

    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float tempoConfidence = audio.tempoConfidence;
    float beatPeriodMs = 60000.0f / audio.bpm;

    // Spawn on beat when confident, otherwise on bass transient
    bool doSpawn = (beatFired && tempoConfidence > 0.25f && bass > 0.02f) ||
                   (bass > pwebLastBass*1.25f && bass > 0.04f);
    if (doSpawn) {
        for (int i=0;i<PWEB_COUNT;i++) {
            if (!pwebs[i].active) {
                int c=random(8);
                // Speed matched to tempo: travel cube diagonal in one beat
                float spd = (tempoConfidence > 0.25f) ?
                            (1.8f / (beatPeriodMs * 0.001f)) * 0.011f :
                            0.45f + bass*0.6f;
                pwebs[i]={true,0.0f,0.5f+bass*1.2f,spd,
                          cubeCorners[c][0],cubeCorners[c][1],cubeCorners[c][2]};
                break;
            }
        }
    }
    pwebLastBass=bass;

    for (int i=0;i<PWEB_COUNT;i++) {
        if(!pwebs[i].active) continue;
        pwebs[i].radius+=pwebs[i].speed;
        pwebs[i].env*=0.93f;
        if(pwebs[i].radius>2.0f||pwebs[i].env<0.02f) pwebs[i].active=false;
    }

    float hum=mid*0.2f;
    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z,v=hum;
        for (int w=0;w<PWEB_COUNT;w++) {
            if(!pwebs[w].active) continue;
            float dx=x-pwebs[w].cx,dy=y-pwebs[w].cy,dz=z-pwebs[w].cz;
            float d=sqrtf(dx*dx+dy*dy+dz*dz),dr=d-pwebs[w].radius;
            v+=pwebs[w].env*expf(-dr*dr*40.0f);
        }
        float spark=(inoise8(i*11,(uint16_t)(t*90.0f))/255.0f)*high*1.5f;
        buf[i]=applyPalette(constrain(v+spark,0.0f,1.0f));
    }
}

// ── Audio8: Spectrum Helix ────────────────────────────────
// Beat: rotation speed locks to BPM. On beatFired, brightness
// flares briefly before settling back to ambient level.

static float helixFlare = 0.0f;

void audioSpectrumHelix(CRGB* buf, float t) {
    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    bool  beatFired = audio.beatFired;
    float bpm = audio.bpm;
    float tempoConfidence = audio.tempoConfidence;

    // Lock rotation to tempo when confident
    float spinRate = (tempoConfidence > 0.5f) ?
                     (bpm / 60.0f * 0.08f) :
                     (0.8f + mid*5.0f) * 0.08f;
    float spin = t * (tempoConfidence > 0.5f ? bpm/60.0f : (0.8f+mid*5.0f));

    // Beat flare
    if (beatFired && tempoConfidence > 0.25f) helixFlare = 0.5f + bass*0.5f;
    helixFlare *= 0.82f;

    uint32_t ti=(uint32_t)(t*50.0f);

    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x-0.5f,y=voxels[i].y-0.5f,z=voxels[i].z;
        float r=sqrtf(x*x+y*y);
        float theta=(r<0.001f)?0.0f:(atan2f(y,x)/6.2832f+0.5f);
        float helixTheta=fmodf(z*1.5f+spin*spinRate+1.0f,1.0f);
        float dTheta=fabsf(theta-helixTheta);
        if(dTheta>0.5f) dTheta=1.0f-dTheta;
        float onHelix=expf(-dTheta*dTheta*80.0f)*expf(-r*r*12.0f);

        // Height selects the perceptual band: bottom=sub … top=brilliance
        int band = constrain((int)(z * NUM_BANDS), 0, NUM_BANDS - 1);
        float fv = audio.band[band];
        float amb=(inoise8(i*13,ti)/255.0f)*constrain(audio.level*2.0f+0.15f,0.05f,0.4f);
        float stripe=onHelix*(0.3f+fv*0.7f)+helixFlare*onHelix;
        buf[i]=applyPalette(constrain(amb+stripe,0.0f,1.0f));
    }
}

// ── Audio9: Earthquake ────────────────────────────────────
// Beat: rumble envelope resets on beatFired. Shudder
// intensity rides barPhase for a 4-beat tension/release cycle.

static float quakeLastBass=0,quakeRumbleEnv=0;

void audioEarthquake(CRGB* buf, float t) {
    // audio bus → drivers
    float bass = audio.sub()*0.6f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid() + audio.highMid()*0.5f;
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float barPhase = audio.barPhase;
    float tempoConfidence = audio.tempoConfidence;

    // Reset rumble on beat
    if (beatFired && tempoConfidence > 0.25f)
        quakeRumbleEnv=constrain(quakeRumbleEnv+0.4f+bass*1.2f,0.0f,1.5f);
    else if (bass>quakeLastBass*1.2f&&bass>0.03f)
        quakeRumbleEnv=constrain(quakeRumbleEnv+bass*1.5f,0.0f,1.5f);

    quakeLastBass=bass;
    quakeRumbleEnv*=0.97f;

    // barPhase drives tension: shudder builds over 4 beats then releases
    float barTension = (tempoConfidence > 0.25f) ?
                       (0.5f + 0.5f * sinf(barPhase * 3.14159f)) : 1.0f;

    uint32_t ti=(uint32_t)(t*55.0f);
    for (int i=0;i<NUM_LEDS;i++) {
        float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
        float waveFront=quakeRumbleEnv*0.6f;
        float groundV=expf(-(z-waveFront)*(z-waveFront)*20.0f)*quakeRumbleEnv;
        float nx=inoise8((uint16_t)(x*80+ti),(uint16_t)(y*80))/255.0f;
        float ny=inoise8((uint16_t)(y*80+ti+100),(uint16_t)(z*80))/255.0f;
        float shudder=((nx+ny)*0.5f)*mid*2.0f*barTension;
        float upSpark=0;
        if(high>0.04f) upSpark=(inoise8(i*19,(uint16_t)(ti*2))/255.0f)*high*2.5f*z;
        buf[i]=applyPalette(constrain(groundV+shudder+upSpark,0.0f,1.0f));
    }
}

// ── Audio: Flame ──────────────────────────────────────────
// Beat/bass injects heat at graph nodes; heat diffuses along
// the welded edge graph and cools, so fire crawls around the
// wireframe and through corners (a bass-driven diffusion field).
static float flameHeat[NUM_LEDS];
static bool  flameInit    = false;
static float flameLastBass = 0;

void audioFlame(CRGB* buf, float t) {
    float bass = audio.sub()*0.7f + audio.bass();
    float mid  = audio.lowMid()*0.5f + audio.mid();
    bool  beatFired = audio.beatFired;
    float tempoConfidence = audio.tempoConfidence;

    if (!flameInit) { for (int i=0;i<NUM_LEDS;i++) flameHeat[i]=0; flameInit=true; }

    // Inject heat on beat or bass transient; splash to neighbours so it catches.
    bool inject = (beatFired && tempoConfidence>0.25f) || (bass>flameLastBass*1.3f && bass>0.05f);
    flameLastBass = bass;
    if (inject) {
        int seeds = 2 + (int)(bass*8.0f);
        for (int k=0;k<seeds;k++) {
            int idx=random(NUM_LEDS);
            flameHeat[idx]=constrain(flameHeat[idx]+0.7f+bass*0.6f, 0.0f, 1.5f);
            flameHeat[rdNeighborL[idx]]=max(flameHeat[rdNeighborL[idx]],0.5f);
            flameHeat[rdNeighborR[idx]]=max(flameHeat[rdNeighborR[idx]],0.5f);
        }
    }
    // Sparse embers from the mids keep it flickering between beats.
    if (mid>0.1f && (int)random(100) < (int)(mid*40.0f)) flameHeat[random(NUM_LEDS)] += mid*0.5f;

    // Spread along the graph, then cool (louder mids = more sustain).
    graphBlur(flameHeat, 1, 0.28f);
    float cool = 0.90f - constrain(mid*0.03f, 0.0f, 0.05f);
    for (int i=0;i<NUM_LEDS;i++) {
        flameHeat[i]*=cool;
        buf[i]=applyPalette(constrain(flameHeat[i],0.0f,1.0f));
    }
}

// ── Audio: Dendrite ───────────────────────────────────────
// Beats spawn charges that walk the welded graph and fork,
// branching around corners like lightning; a decaying charge
// field leaves fading trails behind each tip.
#define DEND_TIPS 24
struct DendTip { bool active; int node; bool fwd; int life; float env; };
static DendTip dendTips[DEND_TIPS];
static float   dendCharge[NUM_LEDS];
static bool    dendInit = false;

static void dendSpawn(int node, bool fwd, float env, int life) {
    for (int i=0;i<DEND_TIPS;i++)
        if (!dendTips[i].active) { dendTips[i] = { true, node, fwd, life, env }; return; }
}

void audioDendrite(CRGB* buf, float t) {
    float bass = audio.sub()*0.7f + audio.bass();
    float high = audio.presence() + audio.brilliance();
    bool  beatFired = audio.beatFired;
    float tempoConfidence = audio.tempoConfidence;

    if (!dendInit) {
        for (int i=0;i<DEND_TIPS;i++) dendTips[i].active=false;
        for (int i=0;i<NUM_LEDS;i++) dendCharge[i]=0;
        dendInit=true;
    }

    // Strike on a confident beat (or a big bass hit): several branches
    // from one node, each heading a random way around the graph.
    bool strike = (beatFired && tempoConfidence>0.25f && bass>0.03f) || (bass>0.6f);
    if (strike) {
        int startNode = random(NUM_LEDS);
        int branches  = 2 + (int)(bass*3.0f);
        int life      = 12 + (int)(bass*10.0f);
        for (int b=0;b<branches;b++) dendSpawn(startNode, (random(2)==0), 0.8f+bass*0.5f, life);
    }

    // Advance tips along the graph; occasionally fork the other way.
    for (int i=0;i<DEND_TIPS;i++) {
        if (!dendTips[i].active) continue;
        DendTip &tp = dendTips[i];
        dendCharge[tp.node] = max(dendCharge[tp.node], tp.env + high*0.4f);
        tp.node = tp.fwd ? rdNeighborR[tp.node] : rdNeighborL[tp.node];
        tp.life--;
        if (tp.life>3 && random(100)<18) dendSpawn(tp.node, !tp.fwd, tp.env*0.7f, tp.life/2);
        if (tp.life<=0) tp.active=false;
    }

    // Decay the charge field → fading dendrite trails.
    for (int i=0;i<NUM_LEDS;i++) {
        dendCharge[i]*=0.80f;
        buf[i]=applyPalette(constrain(dendCharge[i],0.0f,1.0f));
    }
}

////////////////////////////////////////////////////////////
// ================= VOICE ANIMATIONS =================
////////////////////////////////////////////////////////////

// Voice0: Breathe
void voiceBreathe(CRGB* buf, float t) {
  const float inv_sqrt3=0.57735f;
  float speechEnergy = audio.speech;
  float high = audio.presence() + audio.brilliance();
  float breath=0.06f+0.06f*sinf(t*1.1f), swell=speechEnergy*2.5f;
  float tipFlash=high*3.0f*expf(-high*2.0f);
  for (int i=0;i<NUM_LEDS;i++) {
    float h=(voxels[i].x+voxels[i].y+voxels[i].z)*inv_sqrt3;
    buf[i]=applyPalette(constrain(breath+swell*(0.4f+h*0.6f)+tipFlash*h*h,0.0f,1.0f));
  }
}

// Voice1: Formant
void voiceFormant(CRGB* buf, float t) {
  const float inv_sqrt3=0.57735f, bandW=0.12f;
  static float posA=0.15f,posB=0.85f,velA=0,velB=0;
  float speechEnergy = audio.speech;
  bool  syllableOnset = audio.syllableOnset;
  float sylEnv = audio.sylEnv;
  float comp=speechEnergy*1.8f, tA=0.15f+comp*0.25f, tB=0.85f-comp*0.25f;
  if(syllableOnset){velA+=0.04f;velB-=0.04f;}
  velA+=(tA-posA)*0.08f; velB+=(tB-posB)*0.08f; velA*=0.75f; velB*=0.75f;
  posA+=velA; posB+=velB; posA=constrain(posA,0.05f,0.95f); posB=constrain(posB,0.05f,0.95f);
  float br=0.3f+speechEnergy*2.0f+sylEnv*0.5f;
  for (int i=0;i<NUM_LEDS;i++) {
    float h=(voxels[i].x+voxels[i].y+voxels[i].z)*inv_sqrt3;
    float vA=expf(-fabsf(h-posA)*fabsf(h-posA)/(bandW*bandW)*4.0f);
    float vB=expf(-fabsf(h-posB)*fabsf(h-posB)/(bandW*bandW)*4.0f);
    float between=(h>posA&&h<posB)?speechEnergy*0.4f*(1.0f-fabsf(h-0.5f)*2.0f):0;
    buf[i]=applyPalette(constrain((vA+vB)*br+between,0.0f,1.0f));
  }
}

// Voice2: Harmonic Rings
#define HRING_COUNT 6
struct HarmonicRing { float radius,env,speed; bool active; };
static HarmonicRing hrings[HRING_COUNT];
static bool hringInited=false;

void voiceHarmonicRings(CRGB* buf, float t) {
  bool  syllableOnset = audio.syllableOnset;
  float speechEnergy = audio.speech;
  float voiceLevel = audio.level;
  if(!hringInited){for(int i=0;i<HRING_COUNT;i++) hrings[i].active=false;hringInited=true;}
  if(syllableOnset){for(int i=0;i<HRING_COUNT;i++) if(!hrings[i].active){hrings[i]={0.0f,0.6f+speechEnergy*1.5f,0.4f+speechEnergy*0.8f,true};break;}}
  for(int i=0;i<HRING_COUNT;i++){if(!hrings[i].active) continue;hrings[i].radius+=hrings[i].speed*0.011f;hrings[i].env*=0.93f;if(hrings[i].radius>1.5f||hrings[i].env<0.02f) hrings[i].active=false;}
  float hum=voiceLevel*0.3f*(sinf(t*4.0f)*0.5f+0.5f);
  for(int i=0;i<NUM_LEDS;i++){
    float dx=voxels[i].x-0.5f,dy=voxels[i].y-0.5f,dz=voxels[i].z-0.5f,d=sqrtf(dx*dx+dy*dy+dz*dz),v=hum;
    for(int r=0;r<HRING_COUNT;r++){if(!hrings[r].active) continue;float dr=d-hrings[r].radius;v+=hrings[r].env*(dr<0?expf(-dr*dr*60.0f):expf(-dr*dr*180.0f)*1.4f);}
    buf[i]=applyPalette(constrain(v,0.0f,1.0f));
  }
}

// Voice3: Syllable Sparks — sparks walk the welded edge graph,
// flowing across corners instead of stopping at an edge end.
#define SSPARK_COUNT 14
#define SSPARK_TAIL  7
struct GraphSpark { bool active; int node; bool fwd; float accum, speed, env; };
static GraphSpark ssparks[SSPARK_COUNT];
static bool ssparksInited=false;

void fireSyllableSparks(float speechEnergy, float sylEnv) {
  int want = 2 + (int)(speechEnergy * 4.0f), spawned = 0;
  for (int s=0; s<SSPARK_COUNT && spawned<want; s++) {
    if (!ssparks[s].active) {
      ssparks[s] = { true, (int)random(NUM_LEDS), (random(2)==0),
                     0.0f, 0.4f + speechEnergy*0.9f, 0.7f + sylEnv*0.5f };
      spawned++;
    }
  }
}

void voiceSyllableSparks(CRGB* buf, float t) {
  bool  syllableOnset = audio.syllableOnset;
  float speechEnergy  = audio.speech;
  float sylEnv        = audio.sylEnv;
  float high          = audio.presence() + audio.brilliance();
  float voiceLevel    = audio.level;

  if(!ssparksInited){for(int i=0;i<SSPARK_COUNT;i++) ssparks[i].active=false; ssparksInited=true;}
  if(syllableOnset) fireSyllableSparks(speechEnergy, sylEnv);

  // Ambient shimmer sampled at each LED's 3D position → continuous
  // across edges/corners (no per-edge seams).
  uint32_t ti=(uint32_t)(t*80.0f);
  for(int i=0;i<NUM_LEDS;i++){
    uint8_t n=inoise8((uint16_t)(voxels[i].x*140.0f+ti),
                      (uint16_t)(voxels[i].y*140.0f),
                      (uint16_t)(voxels[i].z*140.0f));
    buf[i]=applyPalette(n/255.0f*voiceLevel*0.35f);
  }

  // Advance + render each spark walking the graph, with a fading
  // tail trailing behind it around corners.
  for(int s=0;s<SSPARK_COUNT;s++){
    if(!ssparks[s].active) continue;
    ssparks[s].accum += ssparks[s].speed;
    while(ssparks[s].accum >= 1.0f){
      ssparks[s].node = ssparks[s].fwd ? graphR(ssparks[s].node) : graphL(ssparks[s].node);
      ssparks[s].accum -= 1.0f;
    }
    ssparks[s].env *= 0.94f;
    if(ssparks[s].env < 0.03f){ ssparks[s].active=false; continue; }

    int n=ssparks[s].node; float b=ssparks[s].env;
    for(int k=0;k<SSPARK_TAIL;k++){
      float br = b + (k==0 ? high*1.5f : 0.0f);
      CRGB c=applyPalette(constrain(br,0.0f,1.0f));
      buf[n].r=qadd8(buf[n].r,c.r); buf[n].g=qadd8(buf[n].g,c.g); buf[n].b=qadd8(buf[n].b,c.b);
      n = ssparks[s].fwd ? graphL(n) : graphR(n);   // tail trails behind the head
      b *= 0.6f;
    }
  }
}

////////////////////////////////////////////////////////////
// ================= ANIMATION NAME TABLES =================
////////////////////////////////////////////////////////////

const char* staticAnimNames[10] = {
  "DiagFlow","Lissajous","GravPart","RD","MobiusBraid",
  "Plasma","NoiseWorms","Coral","--","--"
};
const char* audioAnimNames[10] = {
  "TriAxis","Impact","CellAuto","BassBloom","Vortex",
  "PulseWeb","SpectrHlix","Earthquake","Flame","Dendrite"
};
const char* voiceAnimNames[10] = {
  "Breathe","Formant","Rings","Sparks","Voice4",
  "Voice5","Voice6","Voice7","Voice8","Voice9"
};