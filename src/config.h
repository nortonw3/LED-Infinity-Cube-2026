#pragma once

////////////////////////////////////////////////////////////
// HARDWARE + BUILD CONFIG
// Pins and compile-time constants. No dependencies — included
// first by globals.h. Runtime-tunable settings (gains, knobs)
// live in the audio/UI modules, not here.
////////////////////////////////////////////////////////////

// ── LED strip ─────────────────────────────────────────────
#define DATA_PIN            2
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB
#define MAX_BRIGHTNESS      255
#define TARGET_FPS          45

#define POWER_VOLTAGE       5
#define POWER_LIMIT_MA      15000

#define BRIGHTNESS_MIN      5
#define BRIGHTNESS_MAX      255

// ── Cube geometry ─────────────────────────────────────────
#define NUM_EDGES           12
#define LEDS_PER_EDGE       32
#define NUM_LEDS            (NUM_EDGES * LEDS_PER_EDGE)

// ── Rotary encoder ────────────────────────────────────────
#define ENC_CLK             3
#define ENC_DT              4
#define ENC_SW              5

// ── OLED (SSD1306 on Wire2) ───────────────────────────────
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_ADDR           0x3C

// ── Audio ─────────────────────────────────────────────────
#define FFT_BINS            512

// ── Animation slot counts (arrays are sized 10; these are the
//    number of real animations reachable in each mode) ───────
#define ANIM_SLOTS          10
#define NUM_STATIC_ANIMS    8
#define NUM_AUDIO_ANIMS     10
#define NUM_VOICE_ANIMS     4
