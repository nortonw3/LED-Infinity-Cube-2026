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

// Static1: Equator Trace
void staticEquatorTrace(CRGB* buf, float t) {
  fill_solid(buf, NUM_LEDS, CRGB::Black);
  const struct { int edge; bool rev; } ring[6] = {
    {1,false},{8,true},{11,true},{5,true},{4,true},{2,true}
  };
  const int   ringLen  = 6 * LEDS_PER_EDGE;
  const float trailLen = 18.0f, speed = 60.0f;
  int head = (int)fmodf(t * speed, (float)ringLen);
  for (int s = 0; s < (int)trailLen; s++) {
    int ringPos  = ((head - s) % ringLen + ringLen) % ringLen;
    int seg      = ringPos / LEDS_PER_EDGE;
    int posInSeg = ringPos % LEDS_PER_EDGE;
    int edgeBase = ring[seg].edge * LEDS_PER_EDGE;
    int ledIdx   = ring[seg].rev ? edgeBase + (LEDS_PER_EDGE-1-posInSeg) : edgeBase + posInSeg;
    float brightness = 1.0f - (float)s / trailLen;
    buf[ledIdx] = applyPalette(brightness * brightness);
  }
}

// Static2: Droplets
#define MAX_PARTICLES 12
struct Particle { bool active; int edge; bool forward; float pos, speed, trail; };
Particle particles[MAX_PARTICLES];

int pickDownwardEdge(int v, bool &forward) {
  const int edgeFrom[12]={0,1,1,2,2,3,3,0,4,5,6,7};
  const int edgeTo[12]  ={1,5,2,6,3,7,0,4,5,6,7,4};
  const int vHeight[8]  ={0,1,2,1,1,2,3,2};
  int cand[12]; bool cfwd[12]; int n=0;
  for (int e=0;e<12;e++) {
    if (edgeFrom[e]==v && vHeight[edgeTo[e]]<vHeight[v])       { cand[n]=e; cfwd[n]=true;  n++; }
    else if (edgeTo[e]==v && vHeight[edgeFrom[e]]<vHeight[v])  { cand[n]=e; cfwd[n]=false; n++; }
  }
  if (n==0) return -1;
  int p=random(n); forward=cfwd[p]; return cand[p];
}

int edgeEndVertex(int edge, bool forward) {
  const int ef[12]={0,1,1,2,2,3,3,0,4,5,6,7};
  const int et[12]={1,5,2,6,3,7,0,4,5,6,7,4};
  return forward ? et[edge] : ef[edge];
}

void spawnParticle() {
  for (int i=0;i<MAX_PARTICLES;i++) {
    if (!particles[i].active) {
      bool fwd; int e=pickDownwardEdge(6,fwd); if(e<0) return;
      particles[i]={true,e,fwd,0.0f,0.15f+(random(100)/100.0f)*0.25f,0.12f+(random(100)/100.0f)*0.10f};
      return;
    }
  }
}

void initParticles() { for (int i=0;i<MAX_PARTICLES;i++) particles[i].active=false; }

void staticDroplets(CRGB* buf, float t) {
  static float lastT=0; static unsigned long nextSpawn=0;
  float delta=t-lastT; if(delta<0||delta>0.5f) delta=0.011f; lastT=t;
  fill_solid(buf, NUM_LEDS, CRGB::Black);
  unsigned long now=millis();
  if (now>=nextSpawn) { spawnParticle(); nextSpawn=now+200+random(700); }
  for (int i=0;i<MAX_PARTICLES;i++) {
    if (!particles[i].active) continue;
    particles[i].pos += particles[i].speed * delta;
    while (particles[i].pos >= 1.0f) {
      particles[i].pos -= 1.0f;
      int av=edgeEndVertex(particles[i].edge,particles[i].forward);
      bool fwd; int ne=pickDownwardEdge(av,fwd);
      if (ne<0) { particles[i].active=false; break; }
      particles[i].edge=ne; particles[i].forward=fwd;
    }
    if (!particles[i].active) continue;
    float head=particles[i].pos, trail=particles[i].trail;
    int base=particles[i].edge*LEDS_PER_EDGE;
    for (int j=0;j<LEDS_PER_EDGE;j++) {
      float lp=((float)j/(LEDS_PER_EDGE-1));
      float db=particles[i].forward ? head-lp : lp-(1.0f-head);
      if (db>=0&&db<trail) {
        float br=(1.0f-db/trail); br*=br;
        int li=particles[i].forward ? base+j : base+(LEDS_PER_EDGE-1-j);
        CRGB c=applyPalette(br);
        buf[li].r=qadd8(buf[li].r,c.r); buf[li].g=qadd8(buf[li].g,c.g); buf[li].b=qadd8(buf[li].b,c.b);
      }
    }
  }
}

// Static3: Sparkle
void staticSparkle(CRGB* buf, float t) {
  for (int i=0;i<NUM_LEDS;i++) {
    float phase=(float)((i*2731+1013)%997)/997.0f;
    float rate=2.5f+(float)((i*1637)%100)/100.0f*4.0f;
    float cycle=fmodf(t*rate+phase*6.2832f,6.2832f);
    float v=0; if(cycle<0.35f){v=1.0f-(cycle/0.35f);v=v*v*v;}
    buf[i]=applyPalette(v);
  }
}

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

void buildRDNeighbors() {
  const int EF[12] = {0,1,1,2,2,3,3,0,4,5,6,7};
  const int ET[12] = {1,5,2,6,3,7,0,4,5,6,7,4};
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

// Static5: Comet Chase
void staticCometChase(CRGB* buf, float t) {
  fill_solid(buf, NUM_LEDS, CRGB::Black);
  const float speeds[3]  = { 0.31f, 0.47f, 0.23f };
  const float axes[3][3] = { {1,0,0}, {0,1,0}, {0,0,1} };
  const float tailLen    = 0.32f;
  for (int c = 0; c < 3; c++) {
    float head = fmodf(t * speeds[c], 1.0f);
    for (int i = 0; i < NUM_LEDS; i++) {
      float proj = voxels[i].x*axes[c][0] + voxels[i].y*axes[c][1] + voxels[i].z*axes[c][2];
      float d0=head-proj, d1=d0+1.0f, d2=d0-1.0f;
      float d=(fabsf(d0)<fabsf(d1))?d0:d1; if(fabsf(d2)<fabsf(d)) d=d2;
      float v=0;
      if (d>=0&&d<tailLen) { v=(1.0f-d/tailLen); v=v*v*v; if(d<0.04f) v=1.0f; }
      CRGB col=applyPalette(v*(0.5f+c*0.25f));
      buf[i].r=qadd8(buf[i].r,col.r); buf[i].g=qadd8(buf[i].g,col.g); buf[i].b=qadd8(buf[i].b,col.b);
    }
  }
}

// Static6: Edge Breathe
void staticEdgeBreathe(CRGB* buf, float t) {
  for (int e = 0; e < NUM_EDGES; e++) {
    float phase  = (float)e / NUM_EDGES * 6.2832f;
    float bright = (sinf(t * 0.7f + phase) + 1.0f) * 0.5f;
    bright = bright * bright;
    int base = e * LEDS_PER_EDGE;
    for (int j = 0; j < LEDS_PER_EDGE; j++) {
      float edgePos = (float)j / (LEDS_PER_EDGE - 1);
      float tip = expf(-fabsf(edgePos - 0.5f) * 6.0f);
      buf[base + j] = applyPalette(bright * (0.3f + tip * 0.7f));
    }
  }
}

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

// Static8: Zipper
void staticZipper(CRGB* buf, float t) {
  fill_solid(buf, NUM_LEDS, CRGB::Black);
  const int pairs[6][2] = { {0,9},{1,11},{2,7},{3,6},{4,10},{5,8} };
  const float period=4.0f, seam=0.04f;
  for (int p=0;p<6;p++) {
    float phase  = (float)p/6.0f;
    float cursor = fmodf(t/period+phase, 1.0f);
    for (int side=0;side<2;side++) {
      int base=pairs[p][side]*LEDS_PER_EDGE;
      for (int j=0;j<LEDS_PER_EDGE;j++) {
        float lp=(float)j/(LEDS_PER_EDGE-1);
        float dist=fabsf(lp-cursor);
        float v=expf(-dist*dist/(seam*seam));
        float fill=(lp<cursor)?0.15f:0.0f;
        CRGB c=applyPalette(constrain(v+fill,0.0f,1.0f));
        buf[base+j].r=qadd8(buf[base+j].r,c.r);
        buf[base+j].g=qadd8(buf[base+j].g,c.g);
        buf[base+j].b=qadd8(buf[base+j].b,c.b);
      }
    }
  }
}

// Static9: Noise Worms
void staticNoiseWorms(CRGB* buf, float t) {
  uint32_t ti=(uint32_t)(t*60.0f);
  for (int e=0;e<NUM_EDGES;e++) {
    int base=e*LEDS_PER_EDGE;
    for (int j=0;j<LEDS_PER_EDGE;j++) {
      uint8_t n=inoise8(e*47+j*11, ti+e*200);
      float v=(n/255.0f); v=v*v;
      buf[base+j]=applyPalette(v);
    }
  }
}

////////////////////////////////////////////////////////////
// ================= AUDIO ANIMATIONS =================
////////////////////////////////////////////////////////////

// Audio0: Tri Axis
static const float equatorialVerts[6][3]={{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{0,1,1}};
static float burstX=0.5f, burstY=0.5f, burstZ=0.5f, burstEnv=0.0f, lastHigh=0.0f;

void audioTriAxis(CRGB* buf, float t) {
  const float inv_sqrt3=0.57735f;
  if (high>lastHigh*1.35f&&high>0.04f&&burstEnv<0.2f) {
    int v=random(6); burstX=equatorialVerts[v][0]; burstY=equatorialVerts[v][1]; burstZ=equatorialVerts[v][2]; burstEnv=1.0f;
  }
  lastHigh=high; burstEnv*=0.88f;
  for (int i=0;i<NUM_LEDS;i++) {
    float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
    float h=(x+y+z)*inv_sqrt3;
    float bW=0.15f+bass*0.6f, bassV=bass*2.5f*expf(-(h*h)/(bW*bW));
    float mW=0.15f+mid*0.6f,  mH=1.0f-h, midV=mid*2.5f*expf(-(mH*mH)/(mW*mW));
    float dx=x-burstX,dy=y-burstY,dz=z-burstZ;
    float highV=burstEnv*expf(-(dx*dx+dy*dy+dz*dz)*8.0f);
    buf[i]=applyPalette(constrain(bassV+midV+highV,0.0f,1.0f));
  }
}

// Audio1: Impact
#define SHOCK_COUNT 4
struct Shockwave { float radius,speed,cx,cy,cz,env; bool active; };
static Shockwave shocks[SHOCK_COUNT];
static float shockLastBass=0, highLast=0;
static bool  shocksInited=false;

void audioImpact(CRGB* buf, float t) {
  if (!shocksInited) { for(int s=0;s<SHOCK_COUNT;s++) shocks[s].active=false; for(int i=0;i<NUM_LEDS;i++) sparkEnvs[i]=0; shocksInited=true; }
  if (bass>shockLastBass*1.35f&&bass>0.05f) {
    for (int s=0;s<SHOCK_COUNT;s++) if(!shocks[s].active){shocks[s]={0,0.55f+bass*0.8f,(float)random(2),(float)random(2),(float)random(2),0.6f+bass*1.2f,true};break;}
  }
  shockLastBass=bass;
  if (high>highLast*1.4f&&high>0.04f) { int cnt=3+(int)(high*20.0f); for(int k=0;k<cnt;k++) sparkEnvs[random(NUM_LEDS)]=0.8f+high*0.5f; }
  highLast=high;
  for (int s=0;s<SHOCK_COUNT;s++) { if(!shocks[s].active) continue; shocks[s].radius+=shocks[s].speed*0.011f; shocks[s].env*=0.94f; if(shocks[s].radius>2.0f||shocks[s].env<0.02f) shocks[s].active=false; }
  for (int i=0;i<NUM_LEDS;i++) sparkEnvs[i]*=0.78f;
  for (int i=0;i<NUM_LEDS;i++) {
    float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
    float nx=inoise8(x*110+mid*180,y*110,z*110+t*18)/255.0f;
    float ny=inoise8(y*110+50,z*110,x*110+t*15)/255.0f;
    float seethe=((nx+ny)*0.5f)*mid*2.2f, shockV=0;
    for (int s=0;s<SHOCK_COUNT;s++) { if(!shocks[s].active) continue; float dx=x-shocks[s].cx,dy=y-shocks[s].cy,dz=z-shocks[s].cz,d=sqrtf(dx*dx+dy*dy+dz*dz),dr=d-shocks[s].radius; shockV+=shocks[s].env*expf(-dr*dr*35.0f); }
    buf[i]=applyPalette(constrain(seethe+shockV+sparkEnvs[i],0.0f,1.0f));
  }
}

// Audio2: Storm
#define BOLT_COUNT 6
struct LightningBolt { bool active; int edge; bool forward; float pos,speed,explodeEnv; };
static LightningBolt bolts[BOLT_COUNT];
static float boltLastBass=0, boltEdgeEnv[12], stormHighLast=0;
static bool  stormInited=false;

static int pickUpwardEdge(int v, bool &fwd) {
  const int ef[12]={0,1,1,2,2,3,3,0,4,5,6,7},et[12]={1,5,2,6,3,7,0,4,5,6,7,4};
  const int vh[8]={0,1,2,1,1,2,3,2};
  int cand[12]; bool cfwd[12]; int n=0;
  for(int e=0;e<12;e++){if(ef[e]==v&&vh[et[e]]>vh[v]){cand[n]=e;cfwd[n]=true;n++;}else if(et[e]==v&&vh[ef[e]]>vh[v]){cand[n]=e;cfwd[n]=false;n++;}}
  if(n==0) return -1; int p=random(n); fwd=cfwd[p]; return cand[p];
}
static int boltArrivalVertex(int edge, bool fwd) {
  const int ef[12]={0,1,1,2,2,3,3,0,4,5,6,7},et[12]={1,5,2,6,3,7,0,4,5,6,7,4};
  return fwd?et[edge]:ef[edge];
}

void audioStorm(CRGB* buf, float t) {
  const float inv_sqrt3=0.57735f;
  if (!stormInited) { for(int i=0;i<BOLT_COUNT;i++) bolts[i].active=false; for(int i=0;i<12;i++) boltEdgeEnv[i]=0; for(int i=0;i<NUM_LEDS;i++) stormSparkEnv[i]=0; stormInited=true; }
  if (bass>boltLastBass*1.3f&&bass>0.04f) { for(int b=0;b<BOLT_COUNT;b++) if(!bolts[b].active){bool fwd;int e=pickUpwardEdge(0,fwd);if(e>=0){bolts[b]={true,e,fwd,0.5f,0.5f+bass*3.0f,0.2f};}break;} }
  boltLastBass=bass;
  if (high>stormHighLast*1.35f&&high>0.04f) { static const int gE[3]={3,9,10}; int cnt=5+(int)(high*25.0f); for(int k=0;k<cnt;k++){int e=gE[random(3)]*LEDS_PER_EDGE;stormSparkEnv[e+random(LEDS_PER_EDGE)]=0.9f+high*0.4f;} }
  stormHighLast=high;
  for (int b=0;b<BOLT_COUNT;b++) {
    if (!bolts[b].active){bolts[b].explodeEnv*=0.85f;continue;}
    bolts[b].pos+=bolts[b].speed*0.011f;
    if (bolts[b].pos>=1.0f) {
      int arr=boltArrivalVertex(bolts[b].edge,bolts[b].forward); boltEdgeEnv[bolts[b].edge]=1.0f;
      bool fwd; int next=pickUpwardEdge(arr,fwd);
      if(next<0){bolts[b].active=false;bolts[b].explodeEnv=1.0f;}else{bolts[b].edge=next;bolts[b].forward=fwd;bolts[b].pos=0.0f;}
    }
  }
  for(int i=0;i<12;i++) boltEdgeEnv[i]*=0.90f;
  for(int i=0;i<NUM_LEDS;i++) stormSparkEnv[i]*=0.80f;
  for (int i=0;i<NUM_LEDS;i++) {
    float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z; int e=i/LEDS_PER_EDGE;
    float pu=(x-y)*0.70711f,pv=(x+y-2.0f*z)*0.40825f;
    float plasma=((sinf(atan2f(pv,pu)*3.0f-t*(1.5f+mid*6.0f)+(x+y+z)*inv_sqrt3*4.0f)+1.0f)*0.5f)*mid*1.8f;
    float boltV=boltEdgeEnv[e]*0.5f;
    for(int b=0;b<BOLT_COUNT;b++){if(!bolts[b].active||bolts[b].edge!=e) continue;float lp=(float)(i%LEDS_PER_EDGE)/(LEDS_PER_EDGE-1),bp=bolts[b].forward?bolts[b].pos:1.0f-bolts[b].pos;boltV=max(boltV,expf(-fabsf(lp-bp)*fabsf(lp-bp)*80.0f));}
    float explodeV=0; for(int b=0;b<BOLT_COUNT;b++){float dx=x-1,dy=y-1,dz=z-1;explodeV=max(explodeV,bolts[b].explodeEnv*expf(-(dx*dx+dy*dy+dz*dz)*10.0f));}
    buf[i]=applyPalette(constrain(plasma+boltV+explodeV+stormSparkEnv[i],0.0f,1.0f));
  }
}

// Audio3: Frequency Bands
void audioFreqBands(CRGB* buf, float t) {
  for (int e = 0; e < NUM_EDGES; e++) {
    int midLed = e * LEDS_PER_EDGE + (LEDS_PER_EDGE / 2);
    if (midLed >= NUM_LEDS) continue;
    float z = voxels[midLed].z;
    float level;
    if      (z < 0.25f) level = constrain(bass * 3.0f, 0.0f, 1.0f);
    else if (z < 0.75f) level = constrain(mid  * 3.0f, 0.0f, 1.0f);
    else                level = constrain(high * 3.0f, 0.0f, 1.0f);
    int base = e * LEDS_PER_EDGE;
    for (int j = 0; j < LEDS_PER_EDGE; j++) {
      uint8_t n = inoise8((uint16_t)(e*31+j*7), (uint16_t)(t*40.0f));
      buf[base+j] = applyPalette(level * (0.6f + (n/255.0f) * 0.4f));
    }
  }
}

// Audio4: Bass Bloom
static float bloomRadius=0, bloomEnv=0, bloomLastBass=0;

void audioBassBloom(CRGB* buf, float t) {
  if (bass > bloomLastBass * 1.3f && bass > 0.04f) { bloomRadius=0.0f; bloomEnv=0.5f+bass*1.5f; }
  bloomLastBass=bass; bloomRadius+=0.018f; bloomEnv*=0.91f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx=voxels[i].x-0.5f, dy=voxels[i].y-0.5f, dz=voxels[i].z-0.5f;
    float d=sqrtf(dx*dx+dy*dy+dz*dz), dr=d-bloomRadius;
    float bloom=bloomEnv*expf(-dr*dr*50.0f);
    float hum=mid*0.4f*(sinf(t*3.0f+d*8.0f)*0.5f+0.5f);
    float spark=(inoise8(i*13,(uint16_t)(t*90.0f))/255.0f)*high*2.0f;
    buf[i]=applyPalette(constrain(bloom+hum+spark,0.0f,1.0f));
  }
}

// Audio5: Vortex
void audioVortex(CRGB* buf, float t) {
  float spin=t*(1.5f+mid*8.0f), armWidth=max(0.25f+bass*0.4f,0.01f);
  for (int i = 0; i < NUM_LEDS; i++) {
    float x=voxels[i].x-0.5f, y=voxels[i].y-0.5f, z=voxels[i].z;
    float r2=x*x+y*y;
    float normAng=(r2<1e-6f)?0.0f:(atan2f(y,x)/6.2832f+0.5f);
    float armPhase=fmodf(normAng-z*0.5f-spin*0.05f+2.0f,1.0f);
    float dArm=armPhase-0.5f;
    float v=expf(-dArm*dArm/(armWidth*armWidth));
    float spark=(inoise8(i*17,(uint16_t)(t*80.0f))/255.0f)*high*1.5f;
    buf[i]=applyPalette(constrain(v*(0.1f+bass*0.5f)+spark,0.0f,1.0f));
  }
}

// Audio6: Resonance Nodes
void audioResonanceNodes(CRGB* buf, float t) {
  float freq=1.5f+bass*6.0f, amp=0.2f+mid*1.5f;
  for (int e = 0; e < NUM_EDGES; e++) {
    float ePhase=(float)e/NUM_EDGES*3.14159f;
    int base=e*LEDS_PER_EDGE;
    for (int j = 0; j < LEDS_PER_EDGE; j++) {
      float lp=(float)j/(LEDS_PER_EDGE-1);
      float wave=sinf(lp*freq*6.2832f+t*3.0f+ePhase);
      float v=amp*(wave*0.5f+0.5f);
      float spark=(inoise8(e*23+j*9,(uint16_t)(t*100.0f))/255.0f)*high;
      buf[base+j]=applyPalette(constrain(v+spark,0.0f,1.0f));
    }
  }
}

// Audio7: Pulse Web
#define PWEB_COUNT 8
struct PulseWeb { bool active; float radius,env,speed,cx,cy,cz; };
static PulseWeb    pwebs[PWEB_COUNT];
static float       pwebLastBass=0;
static bool        pwebInited=false;
static const float cubeCorners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};

void audioPulseWeb(CRGB* buf, float t) {
  if (!pwebInited) { for(int i=0;i<PWEB_COUNT;i++) pwebs[i].active=false; pwebInited=true; }
  if (bass>pwebLastBass*1.25f&&bass>0.04f) {
    for (int i=0;i<PWEB_COUNT;i++) if(!pwebs[i].active){int c=random(8);pwebs[i]={true,0.0f,0.5f+bass*1.2f,0.45f+bass*0.6f,cubeCorners[c][0],cubeCorners[c][1],cubeCorners[c][2]};break;}
  }
  pwebLastBass=bass;
  for (int i=0;i<PWEB_COUNT;i++) { if(!pwebs[i].active) continue; pwebs[i].radius+=pwebs[i].speed*0.011f; pwebs[i].env*=0.93f; if(pwebs[i].radius>2.0f||pwebs[i].env<0.02f) pwebs[i].active=false; }
  float hum=mid*0.2f;
  for (int i=0;i<NUM_LEDS;i++) {
    float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z,v=hum;
    for (int w=0;w<PWEB_COUNT;w++) { if(!pwebs[w].active) continue; float dx=x-pwebs[w].cx,dy=y-pwebs[w].cy,dz=z-pwebs[w].cz,d=sqrtf(dx*dx+dy*dy+dz*dz),dr=d-pwebs[w].radius; v+=pwebs[w].env*expf(-dr*dr*40.0f); }
    float spark=(inoise8(i*11,(uint16_t)(t*90.0f))/255.0f)*high*1.5f;
    buf[i]=applyPalette(constrain(v+spark,0.0f,1.0f));
  }
}

// Audio8: Spectrum Helix
void audioSpectrumHelix(CRGB* buf, float t) {
  float spin=t*(0.8f+mid*5.0f);
  uint32_t ti=(uint32_t)(t*50.0f);
  for (int i = 0; i < NUM_LEDS; i++) {
    float x=voxels[i].x-0.5f, y=voxels[i].y-0.5f, z=voxels[i].z;
    float r=sqrtf(x*x+y*y);
    float theta=(r<0.001f)?0.0f:(atan2f(y,x)/6.2832f+0.5f);
    float helixTheta=fmodf(z*1.5f+spin*0.08f+1.0f,1.0f);
    float dTheta=fabsf(theta-helixTheta); if(dTheta>0.5f) dTheta=1.0f-dTheta;
    float onHelix=expf(-dTheta*dTheta*80.0f)*expf(-r*r*12.0f);
    int bin=constrain(2+(int)(z*50.0f),2,FFT_BINS-1);
    float fv=constrain(fftBins[bin]*fftGain*10.0f,0.0f,1.0f);
    float amb=(inoise8(i*13,ti)/255.0f)*constrain(voiceLevel*2.0f+0.15f,0.05f,0.4f);
    float stripe=onHelix*(0.3f+fv*0.7f);
    buf[i]=applyPalette(constrain(amb+stripe,0.0f,1.0f));
  }
}

// Audio9: Earthquake
static float quakeLastBass=0, quakeRumbleEnv=0;

void audioEarthquake(CRGB* buf, float t) {
  if (bass>quakeLastBass*1.2f&&bass>0.03f) quakeRumbleEnv=constrain(quakeRumbleEnv+bass*1.5f,0.0f,1.5f);
  quakeLastBass=bass; quakeRumbleEnv*=0.97f;
  uint32_t ti=(uint32_t)(t*55.0f);
  for (int i=0;i<NUM_LEDS;i++) {
    float x=voxels[i].x,y=voxels[i].y,z=voxels[i].z;
    float waveFront=quakeRumbleEnv*0.6f;
    float groundV=expf(-(z-waveFront)*(z-waveFront)*20.0f)*quakeRumbleEnv;
    float nx=inoise8((uint16_t)(x*80+ti),(uint16_t)(y*80))/255.0f;
    float ny=inoise8((uint16_t)(y*80+ti+100),(uint16_t)(z*80))/255.0f;
    float shudder=((nx+ny)*0.5f)*mid*2.0f;
    float upSpark=0;
    if (high>0.04f) upSpark=(inoise8(i*19,(uint16_t)(ti*2))/255.0f)*high*2.5f*z;
    buf[i]=applyPalette(constrain(groundV+shudder+upSpark,0.0f,1.0f));
  }
}

////////////////////////////////////////////////////////////
// ================= VOICE ANIMATIONS =================
////////////////////////////////////////////////////////////

// Voice0: Breathe
void voiceBreathe(CRGB* buf, float t) {
  const float inv_sqrt3=0.57735f;
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

// Voice3: Syllable Sparks
#define SSPARK_COUNT 12
struct SyllableSpark { bool active; int edge; bool forward; float pos,speed,env; };
static SyllableSpark ssparks[SSPARK_COUNT];
static bool ssparksInited=false;

void fireSyllableSparks() {
  int slot=0;
  for(int e=0;e<NUM_EDGES&&slot<SSPARK_COUNT;e++) if(!ssparks[slot].active){ssparks[slot]={true,e,(random(2)==0),0.5f,0.6f+speechEnergy*1.2f,0.7f+sylEnv*0.5f};slot++;}
}

void voiceSyllableSparks(CRGB* buf, float t) {
  if(!ssparksInited){for(int i=0;i<SSPARK_COUNT;i++) ssparks[i].active=false;ssparksInited=true;}
  if(syllableOnset) fireSyllableSparks();
  for(int i=0;i<SSPARK_COUNT;i++){if(!ssparks[i].active) continue;ssparks[i].pos+=ssparks[i].speed*0.011f*(ssparks[i].forward?1:-1);ssparks[i].env*=0.92f;if(ssparks[i].pos>1.05f||ssparks[i].pos<-0.05f||ssparks[i].env<0.02f) ssparks[i].active=false;}
  for(int i=0;i<NUM_LEDS;i++) buf[i]=applyPalette(inoise8(i*17,(uint32_t)(t*80))/255.0f*voiceLevel*0.35f);
  for(int i=0;i<SSPARK_COUNT;i++){
    if(!ssparks[i].active) continue;
    int base=ssparks[i].edge*LEDS_PER_EDGE; float head=ssparks[i].pos, tl=0.25f;
    for(int j=0;j<LEDS_PER_EDGE;j++){
      float lp=(float)j/(LEDS_PER_EDGE-1), db=ssparks[i].forward?head-lp:lp-head;
      if(db>=0&&db<tl){float br=(1.0f-db/tl);br=br*br*ssparks[i].env;if(db<0.03f) br+=high*2.0f;int li=base+j;CRGB c=applyPalette(constrain(br,0.0f,1.0f));buf[li].r=qadd8(buf[li].r,c.r);buf[li].g=qadd8(buf[li].g,c.g);buf[li].b=qadd8(buf[li].b,c.b);}
    }
  }
}

////////////////////////////////////////////////////////////
// ================= ANIMATION NAME TABLES =================
////////////////////////////////////////////////////////////

const char* staticAnimNames[10] = {
  "DiagFlow","EquatorTrace","Droplets","Sparkle","RD",
  "CometChase","EdgeBreathe","Plasma","Zipper","NoiseWorms"
};
const char* audioAnimNames[10] = {
  "TriAxis","Impact","Storm","FreqBands","BassBloom",
  "Vortex","Resonance","PulseWeb","SpectrHlix","Earthquake"
};
const char* voiceAnimNames[10] = {
  "Breathe","Formant","Rings","Sparks","Voice4",
  "Voice5","Voice6","Voice7","Voice8","Voice9"
};