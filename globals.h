#pragma once

#include <FastLED.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FreeStack.h>

////////////////////////////////////////////////////////////
// USER CONFIG
////////////////////////////////////////////////////////////

#define DATA_PIN            2
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define MAX_BRIGHTNESS      255
#define TARGET_FPS          90

#define POWER_VOLTAGE       5
#define POWER_LIMIT_MA      15000

#define BRIGHTNESS_POT_PIN  A2
#define BRIGHTNESS_MIN      20
#define BRIGHTNESS_MAX      180

#define NUM_EDGES           12
#define LEDS_PER_EDGE       32
#define NUM_LEDS            (NUM_EDGES * LEDS_PER_EDGE)

#define ENC_CLK             3
#define ENC_DT              4
#define ENC_SW              5

#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_ADDR           0x3C

#define FFT_BINS            512

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

////////////////////////////////////////////////////////////
// EXTERN — AUDIO
////////////////////////////////////////////////////////////

extern float fftBins[FFT_BINS];
extern float fftGain;
extern float bassGain, midGain, highGain, voiceGain;
extern float smoothingFactor;
extern float bass, mid, high, voiceLevel, speechEnergy, sparkle;
extern bool  syllableOnset;
extern float sylEnv;

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

enum Mode { STATIC_MODE, AUDIO_MODE, VOICE_MODE };
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

// FFT param accessor (used by menu)
float& fftParamRef(int row);