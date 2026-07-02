#pragma once

#include "globals.h"

////////////////////////////////////////////////////////////
// ================= PALETTES =================
////////////////////////////////////////////////////////////

CRGBPalette16 palettes[] = {

  // 0 — Embers: long black gaps, single hot coal pops
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB(60,5,0),   CRGB(160,20,0), CRGB::OrangeRed,CRGB(255,80,0),
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB(40,0,0),
    CRGB(120,10,0), CRGB::Red,      CRGB(255,60,0), CRGB::Black
  ),

  // 1 — Ice Splinter: mostly black, razor-thin cyan/white shards
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB(0,60,80),  CRGB(0,160,200),CRGB::White,
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB(0,40,60),  CRGB(0,120,180),CRGB(180,240,255),CRGB::Black
  ),

  // 2 — Toxic: sickly yellow-green on black, brief hot-white tip
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB(10,30,0),
    CRGB(30,80,0),  CRGB(80,160,0), CRGB(160,220,0),CRGB(220,255,40),
    CRGB::Black,    CRGB::Black,    CRGB(0,20,0),   CRGB(20,60,0),
    CRGB(60,130,0), CRGB(140,200,0),CRGB(200,255,20),CRGB::White
  ),

  // 3 — Neon Sign: black void, isolated magenta + cyan pops
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB(120,0,80), CRGB::Magenta,  CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB::Black,    CRGB(0,80,120), CRGB(0,200,255),
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black
  ),

  // 4 — Deep Sea: dark teal abyss, rare bioluminescent blue flash
  CRGBPalette16(
    CRGB::Black,    CRGB(0,10,15),  CRGB(0,20,30),  CRGB(0,35,50),
    CRGB(0,55,70),  CRGB(0,80,90),  CRGB::Black,    CRGB::Black,
    CRGB(0,15,20),  CRGB(0,30,45),  CRGB(0,50,65),  CRGB::Black,
    CRGB::Black,    CRGB(0,100,140),CRGB(0,180,220),CRGB(100,230,255)
  ),

  // 5 — Forge: heavy black, brief white-hot bursts through orange
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB(80,20,0),  CRGB(180,60,0), CRGB(255,120,0),
    CRGB(255,200,80),CRGB::White,   CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black
  ),

  // 6 — Aurora: slow black swells with green + violet curtains
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB(0,20,10),  CRGB(0,60,20),
    CRGB(0,120,40), CRGB(0,180,60), CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB(20,0,40),  CRGB(60,0,100), CRGB(120,0,180),
    CRGB(160,40,220),CRGB::Black,   CRGB::Black,    CRGB::Black
  ),

  // 7 — Stardust: mostly black cosmos, scattered warm + cold stars
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB(40,30,10),
    CRGB::Black,    CRGB::Black,    CRGB(10,10,60), CRGB(80,80,200),
    CRGB::Black,    CRGB(60,40,10), CRGB(180,140,40),CRGB::Black,
    CRGB::Black,    CRGB::Black,    CRGB(20,20,80), CRGB(200,200,255)
  ),

  // 8 — Blood Moon: dark crimson void, rare bright red/orange flare
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB(20,0,0),   CRGB(50,0,0),
    CRGB(100,0,0),  CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB(30,0,0),   CRGB(80,0,0),   CRGB(160,10,0),
    CRGB(220,40,0), CRGB(255,80,0), CRGB::Black,    CRGB::Black
  ),

  // 9 — Glitch: pure black with jarring white/cyan/magenta spikes
  CRGBPalette16(
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Black,
    CRGB::Black,    CRGB::Black,    CRGB::White,    CRGB::Black,
    CRGB::Black,    CRGB::Black,    CRGB::Black,    CRGB::Cyan,
    CRGB::Black,    CRGB::Black,    CRGB::Magenta,  CRGB::Black
  ),
};

#define PALETTE_ROTATE_INTERVAL_MS 30000UL   // 10 seconds per palette
#define PALETTE_ROTATE_FADE_MS     6000UL    // 2 second crossfade between them

#define NUM_PALETTES 11

#define PALETTE_ROTATE_IDX  10
#define NUM_PALETTES_TOTAL  11   // replaces NUM_PALETTES in the name table

////////////////////////////////////////////////////////////
// ================= PALETTE STATE =================
////////////////////////////////////////////////////////////

uint8_t       currentPaletteIndex = 0;
CRGBPalette16 currentPalette      = palettes[0];
CRGBPalette16 previousPalette     = palettes[0];
float         paletteFadeStart    = 0;
float         paletteFadeDuration = 2.0f;
bool          paletteFading       = false;

////////////////////////////////////////////////////////////
// ================= PALETTE FUNCTIONS =================
////////////////////////////////////////////////////////////

void startPaletteFade(uint8_t newIndex) {
  previousPalette     = currentPalette;
  currentPaletteIndex = newIndex;
  currentPalette      = palettes[newIndex];
  paletteFadeStart    = millis() * 0.001f;
  paletteFading       = true;
}

CRGB applyPalette(float intensity) {
  intensity = constrain(intensity, 0.0f, 1.0f);

  // ── Rotate mode: calculate which real palette to show ──
  if (currentPaletteIndex == PALETTE_ROTATE_IDX) {
    // Total rotation: 10 palettes × (display + fade) ms each
    // Use 64-bit math to prevent overflow on long uptimes
    const uint32_t displayMs = PALETTE_ROTATE_INTERVAL_MS;
    const uint32_t fadeMs    = PALETTE_ROTATE_FADE_MS;
    const uint32_t slotMs    = displayMs + fadeMs;          // 12000ms per palette
    const uint32_t totalMs   = slotMs * 10UL;               // 120000ms full cycle

    uint32_t pos     = millis() % totalMs;                  // 0..119999
    uint8_t  palIdx  = (uint8_t)(pos / slotMs);             // 0..9 current palette
    uint32_t phaseMs = pos % slotMs;                        // 0..11999 within slot

    // phaseMs 0..displayMs    → show palIdx solidly
    // phaseMs displayMs..slotMs → crossfade palIdx → nextIdx
    uint8_t nextIdx = (palIdx + 1) % 10;

    CRGB a = ColorFromPalette(palettes[palIdx],  intensity * 255, 255, LINEARBLEND);
    CRGB b = ColorFromPalette(palettes[nextIdx], intensity * 255, 255, LINEARBLEND);

    if (phaseMs >= displayMs) {
      // In the fade window — blend from a toward b
      uint32_t fadePos = phaseMs - displayMs;               // 0..fadeMs-1
      uint8_t  blend8  = (uint8_t)((fadePos * 255UL) / fadeMs);
      return blend(a, b, blend8);
    }
    return a;
  }

  // ── Normal mode ──────────────────────────────────────
  if (!paletteFading)
    return ColorFromPalette(currentPalette, intensity * 255, 255, LINEARBLEND);

  float p = (millis() * 0.001f - paletteFadeStart) / paletteFadeDuration;
  if (p >= 1.0f) { paletteFading = false; p = 1.0f; }
  CRGB a = ColorFromPalette(previousPalette, intensity * 255, 255, LINEARBLEND);
  CRGB b = ColorFromPalette(currentPalette,  intensity * 255, 255, LINEARBLEND);
  return blend(a, b, (uint8_t)(p * 255));
}

////////////////////////////////////////////////////////////
// ================= PALETTE NAME TABLE =================
////////////////////////////////////////////////////////////

const char* paletteNames[11] = {
  "Embers","IceSplnt","Toxic","NeonSign",
  "DeepSea","Forge","Aurora","Stardust",
  "BloodMoon","Glitch","Rotate"
};