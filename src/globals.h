#pragma once

#include <FastLED.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FreeStack.h>

#include "config.h"   // hardware pins + build constants

////////////////////////////////////////////////////////////
// TYPES
////////////////////////////////////////////////////////////

struct Voxel { float x, y, z; };

typedef void (*AnimFunc)(CRGB*, float);


////////////////////////////////////////////////////////////
// EXTERN — LED + GEOMETRY
////////////////////////////////////////////////////////////

extern CRGB  leds[NUM_LEDS];
extern CRGB  bufferA[NUM_LEDS];
extern CRGB  bufferB[NUM_LEDS];
extern Voxel voxels[NUM_LEDS];
extern uint8_t currentBrightness;

////////////////////////////////////////////////////////////
// EXTERN — AUDIO
// (Per-band features, beat, and knobs live in the AudioBus
//  `audio` and globals in audio_engine.h. Only the raw FFT
//  spectrum is shared here.)
////////////////////////////////////////////////////////////

extern float fftBins[FFT_BINS];

////////////////////////////////////////////////////////////
// EXTERN — ANIMATION SUPPORT ARRAYS
////////////////////////////////////////////////////////////

extern float sparkEnvs[NUM_LEDS];
extern float stormSparkEnv[NUM_LEDS];
extern float rdA[NUM_LEDS];
extern float rdB[NUM_LEDS];
extern float rdA2[NUM_LEDS];
extern float rdB2[NUM_LEDS];
extern int   rdNeighborL[NUM_LEDS];
extern int   rdNeighborR[NUM_LEDS];

////////////////////////////////////////////////////////////
// EXTERN — PALETTE STATE
////////////////////////////////////////////////////////////

extern CRGBPalette16 currentPalette;
extern CRGBPalette16 previousPalette;
extern float         paletteFadeStart;
extern float         paletteFadeDuration;
extern bool          paletteFading;
extern uint8_t       currentPaletteIndex;

////////////////////////////////////////////////////////////
// EXTERN — MODE + ANIMATION INDICES
////////////////////////////////////////////////////////////

enum Mode { STATIC_MODE, AUDIO_MODE, VOICE_MODE, ARTNET_MODE };
extern Mode currentMode;
extern int  staticIndex, audioIndex, voiceIndex;

extern AnimFunc staticAnims[10];
extern AnimFunc audioAnims[10];
extern AnimFunc voiceAnims[10];

////////////////////////////////////////////////////////////
// EXTERN — CROSSFADE
////////////////////////////////////////////////////////////

extern AnimFunc currentAnim;
extern AnimFunc nextAnim;
extern bool     transitioning;
extern float    transitionStart;
extern float    transitionDuration;

////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
////////////////////////////////////////////////////////////

// Palette
CRGB   applyPalette(float intensity);
void   startPaletteFade(uint8_t newIndex);

// Crossfade
void   startTransition(AnimFunc target);