#include "globals.h"
#include "cube.h"
#include "palettes.h"
#include "audio_engine.h"
#include "animations.h"
//#include "artnet.h"

////////////////////////////////////////////////////////////
// ================= HARDFAULT HANDLER =================
////////////////////////////////////////////////////////////

extern "C" void fault_isr(void) {
  uint32_t* sp = (uint32_t*)__builtin_frame_address(0);
  Serial.print(F("\n!!! HARDFAULT  PC=0x"));
  Serial.println(sp[6], HEX);
  Serial.flush();
  pinMode(13, OUTPUT);
  while (1) { digitalToggle(13); delay(80); }
}
extern "C" void hard_fault_isr()     __attribute__((weak, alias("fault_isr")));
extern "C" void memmanage_fault_isr()__attribute__((weak, alias("fault_isr")));
extern "C" void bus_fault_isr()      __attribute__((weak, alias("fault_isr")));
extern "C" void usage_fault_isr()    __attribute__((weak, alias("fault_isr")));

////////////////////////////////////////////////////////////
// ================= GLOBAL DEFINITIONS =================
////////////////////////////////////////////////////////////

uint8_t currentBrightness = 128;   // default 50%

// LED buffers  (voxels[] + edge graph now live in cube.h)
CRGB  leds[NUM_LEDS];
CRGB  bufferA[NUM_LEDS];
CRGB  bufferB[NUM_LEDS];

// Animation support arrays
float sparkEnvs[NUM_LEDS]     = {0};
float stormSparkEnv[NUM_LEDS] = {0};
float rdA[NUM_LEDS];
float rdB[NUM_LEDS];
float rdA2[NUM_LEDS];
float rdB2[NUM_LEDS];

// Audio — raw FFT spectrum, fed to the audio engine (audio_engine.h)
float fftBins[FFT_BINS];

// Mode + animation indices
Mode currentMode = STATIC_MODE;
int  staticIndex = 0, audioIndex = 0, voiceIndex = 0;

AnimFunc staticAnims[10];
AnimFunc audioAnims[10];
AnimFunc voiceAnims[10];

unsigned long frameDeadline = 0;

// Crossfade
AnimFunc currentAnim;
AnimFunc nextAnim;
bool     transitioning      = false;
float    transitionStart    = 0;
float    transitionDuration = 2.0f;

// Per-mode animation counts (see config.h)
int animCount(Mode m) {
  switch (m) {
    case STATIC_MODE: return NUM_STATIC_ANIMS;
    case AUDIO_MODE:  return NUM_AUDIO_ANIMS;
    case VOICE_MODE:  return NUM_VOICE_ANIMS;
    default:          return 1;
  }
}


////////////////////////////////////////////////////////////
// ================= AUDIO SETUP =================
////////////////////////////////////////////////////////////

AudioInputI2S            i2s1;
AudioAnalyzeFFT1024      fft1024;
AudioConnection          patchCord1(i2s1, 0, fft1024, 0);
AudioConnection          patchCord2(i2s1, 1, fft1024, 0);

////////////////////////////////////////////////////////////
// ================= EEPROM =================
////////////////////////////////////////////////////////////

#define EEPROM_VERSION           2
#define EEPROM_VERSION_ADDR      0
#define EEPROM_MODE_ADDR         1
#define EEPROM_STATIC_INDEX_ADDR 2
#define EEPROM_AUDIO_INDEX_ADDR  3
#define EEPROM_VOICE_INDEX_ADDR  4
#define EEPROM_PALETTE_ADDR      5
#define EEPROM_BRIGHT_ADDR       6
#define EEPROM_REACTIVITY_ADDR   8    // float
#define EEPROM_BEATSENS_ADDR     12   // float
#define EEPROM_BANDTILT_ADDR     16   // float

void EEPROMWriteFloat(int addr, float value) {
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < 4; i++) EEPROM.write(addr + i, p[i]);
}

float EEPROMReadFloat(int addr) {
  float value = 0;
  byte* p = (byte*)(void*)&value;
  for (int i = 0; i < 4; i++) p[i] = EEPROM.read(addr + i);
  return value;
}

void saveSettings() {
  EEPROM.write(EEPROM_VERSION_ADDR,      EEPROM_VERSION);
  EEPROM.write(EEPROM_MODE_ADDR,         currentMode);
  EEPROM.write(EEPROM_STATIC_INDEX_ADDR, staticIndex);
  EEPROM.write(EEPROM_AUDIO_INDEX_ADDR,  audioIndex);
  EEPROM.write(EEPROM_VOICE_INDEX_ADDR,  voiceIndex);
  EEPROM.write(EEPROM_PALETTE_ADDR,      currentPaletteIndex);
  EEPROM.write(EEPROM_BRIGHT_ADDR,       currentBrightness);
  EEPROMWriteFloat(EEPROM_REACTIVITY_ADDR, reactivity);
  EEPROMWriteFloat(EEPROM_BEATSENS_ADDR,   beatSensitivity);
  EEPROMWriteFloat(EEPROM_BANDTILT_ADDR,   bandTilt);
}

void loadSettings() {
  // Version mismatch (fresh chip or old layout) → load defaults + persist.
  if (EEPROM.read(EEPROM_VERSION_ADDR) != EEPROM_VERSION) {
    currentMode = STATIC_MODE;
    staticIndex = audioIndex = voiceIndex = 0;
    currentPaletteIndex = 0;
    currentBrightness = 128;
    reactivity = 1.0f; beatSensitivity = 1.0f; bandTilt = 0.0f;
    currentPalette = previousPalette = palettes[0];
    paletteFading = false;
    applyBrightness();
    saveSettings();
    return;
  }

  currentMode = (Mode)EEPROM.read(EEPROM_MODE_ADDR);
  staticIndex = EEPROM.read(EEPROM_STATIC_INDEX_ADDR);
  audioIndex  = EEPROM.read(EEPROM_AUDIO_INDEX_ADDR);
  voiceIndex  = EEPROM.read(EEPROM_VOICE_INDEX_ADDR);

  if (currentMode > ARTNET_MODE)       currentMode = STATIC_MODE;
  if (staticIndex >= NUM_STATIC_ANIMS) staticIndex = 0;
  if (audioIndex  >= NUM_AUDIO_ANIMS)  audioIndex  = 0;
  if (voiceIndex  >= NUM_VOICE_ANIMS)  voiceIndex  = 0;

  currentPaletteIndex = EEPROM.read(EEPROM_PALETTE_ADDR);
  if (currentPaletteIndex >= NUM_PALETTES) currentPaletteIndex = 0;
  currentPalette  = palettes[currentPaletteIndex < PALETTE_ROTATE_IDX ? currentPaletteIndex : 0];
  previousPalette = currentPalette;
  paletteFading   = false;

  currentBrightness = EEPROM.read(EEPROM_BRIGHT_ADDR);
  if (currentBrightness < BRIGHTNESS_MIN) currentBrightness = 128;
  applyBrightness();

  reactivity      = EEPROMReadFloat(EEPROM_REACTIVITY_ADDR);
  beatSensitivity = EEPROMReadFloat(EEPROM_BEATSENS_ADDR);
  bandTilt        = EEPROMReadFloat(EEPROM_BANDTILT_ADDR);
  if (isnan(reactivity)      || reactivity      < 0.1f || reactivity      > 3.0f)  reactivity      = 1.0f;
  if (isnan(beatSensitivity) || beatSensitivity < 0.1f || beatSensitivity > 3.0f)  beatSensitivity = 1.0f;
  if (isnan(bandTilt)        || bandTilt        < -1.0f || bandTilt       > 1.0f)  bandTilt        = 0.0f;
}

////////////////////////////////////////////////////////////
// ================= BRIGHTNESS =================
////////////////////////////////////////////////////////////

void applyBrightness() {
  float bFrac = (float)currentBrightness / 255.0f;
  uint32_t powerLimit = (uint32_t)(POWER_LIMIT_MA * bFrac * bFrac);
  powerLimit = max(powerLimit, (uint32_t)200);
  FastLED.setBrightness(currentBrightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTAGE, powerLimit);
}

////////////////////////////////////////////////////////////
// ================= FFT PROCESSING =================
////////////////////////////////////////////////////////////

void updateFFTRaw() {
  if (fft1024.available())
    for (int i = 0; i < FFT_BINS; i++) fftBins[i] = fft1024.read(i);
}

void updateAudio() {
    updateFFTRaw();          // fill fftBins[] from the FFT
    audioEngineUpdate();     // bands + room AGC + beat/tempo → AudioBus `audio`
}

////////////////////////////////////////////////////////////
// ================= CROSSFADE ENGINE =================
////////////////////////////////////////////////////////////

void startTransition(AnimFunc target) {
  nextAnim = target; transitioning = true; transitionStart = millis() * 0.001f;
  fill_solid(bufferA, NUM_LEDS, CRGB::Black);
  fill_solid(bufferB, NUM_LEDS, CRGB::Black);
}

void renderFrame(float t) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (!transitioning) {
    currentAnim(leds, t);
  } else {
    currentAnim(bufferA, t); nextAnim(bufferB, t);
    float p = (t - transitionStart) / transitionDuration;
    if (p >= 1.0f) {
      transitioning = false; currentAnim = nextAnim;
      fill_solid(bufferA, NUM_LEDS, CRGB::Black);
      fill_solid(bufferB, NUM_LEDS, CRGB::Black);
      currentAnim(leds, t);
    } else {
      uint8_t ba = p * 255;
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = blend(bufferA[i], bufferB[i], ba);
    }
  }
}

////////////////////////////////////////////////////////////
// ================= OLED + ENCODER =================
////////////////////////////////////////////////////////////

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire2, -1);

volatile int  encRaw       = 0;
uint8_t       encStateLast = 0;
bool          swLastState  = HIGH;
unsigned long swLastChange = 0;
bool          swFired      = false;
#define ENC_DEBOUNCE_MS 40

void updateEncoder() {
  static const int8_t tbl[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  uint8_t s=(digitalRead(ENC_CLK)<<1)|digitalRead(ENC_DT);
  encRaw+=tbl[(encStateLast<<2)|s];
  encStateLast=s;
}

bool demoEnabled = false;

const char* modeNames[4] = { "STATIC", "AUDIO", "VOICE", "ARTNET" };

////////////////////////////////////////////////////////////
// ================= MENU SYSTEM =================
////////////////////////////////////////////////////////////

enum MenuState { TOP_NAV, TOP_EDIT, FFT_NAV, FFT_EDIT };
enum TopRow { TOP_MODE, TOP_ANIM, TOP_PALETTE, TOP_BRIGHT, TOP_FFT, TOP_ROW_COUNT };

// Audio submenu: 3 global knobs + Back
#define AUDIO_ROW_COUNT 4
#define AUDIO_ROW_BACK  3

MenuState menuState = TOP_NAV;
TopRow    topRow    = TOP_MODE;
int       fftRow    = 0;

const char* audioParamNames[3] = { "React", "BeatSns", "Tilt" };

float& audioKnobRef(int row) {
  switch (row) {
    case 0: return reactivity;
    case 1: return beatSensitivity;
    case 2: return bandTilt;
    default: return reactivity;
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  const char* animName = "";
  if      (currentMode == STATIC_MODE) animName = staticAnimNames[staticIndex];
  else if (currentMode == AUDIO_MODE)  animName = audioAnimNames[audioIndex];
  else                                 animName = voiceAnimNames[voiceIndex];

  // ── Top level ─────────────────────────────────────────
  if (menuState == TOP_NAV || menuState == TOP_EDIT) {

    if (demoEnabled) { display.setCursor(86, 0); display.print(F("[DEMO]")); }

    // Row 0 — Mode  (y=2, hline y=13)
    if (topRow == TOP_MODE)
      display.fillTriangle(1,2,1,9,7,5, SSD1306_WHITE);
    display.setCursor(13, 2);
    display.print(F("Mode   :"));
    if (menuState == TOP_EDIT && topRow == TOP_MODE) display.print(F("<"));
    else display.print(F(" "));
    display.print(modeNames[currentMode]);
    display.drawFastHLine(0, 13, 128, SSD1306_WHITE);

    // Row 1 — Anim  (y=15, hline y=26)
    if (topRow == TOP_ANIM)
      display.fillTriangle(1,15,1,22,7,18, SSD1306_WHITE);
    display.setCursor(13, 15);
    display.print(F("Anim   :"));
    if (menuState == TOP_EDIT && topRow == TOP_ANIM) display.print(F("<"));
    else display.print(F(" "));
    display.print(animName);
    display.drawFastHLine(0, 26, 128, SSD1306_WHITE);

    // Row 2 — Palette  (y=28, hline y=39)
    if (topRow == TOP_PALETTE)
      display.fillTriangle(1,28,1,35,7,31, SSD1306_WHITE);
    display.setCursor(13, 28);
    display.print(F("Palette:"));
    if (menuState == TOP_EDIT && topRow == TOP_PALETTE) display.print(F("<"));
    else display.print(F(" "));
    display.print(paletteNames[currentPaletteIndex]);
    display.drawFastHLine(0, 39, 128, SSD1306_WHITE);

    // Row 3 — Brightness  (y=41, hline y=52)
    if (topRow == TOP_BRIGHT)
      display.fillTriangle(1,41,1,48,7,44, SSD1306_WHITE);
    display.setCursor(13, 41);
    display.print(F("Bright :"));
    if (menuState == TOP_EDIT && topRow == TOP_BRIGHT) display.print(F("<"));
    else display.print(F(" "));
    display.print(currentBrightness);
    display.drawFastHLine(0, 52, 128, SSD1306_WHITE);

    // Row 4 — Audio Menu  (y=55, no hline)
    if (topRow == TOP_FFT)
      display.fillTriangle(1,55,1,62,7,58, SSD1306_WHITE);
    display.setCursor(13, 55);
    display.print(F("Audio >"));

  // ── Audio knobs page ──────────────────────────────────
  } else {   // FFT_NAV || FFT_EDIT

    display.setCursor(0, 0);
    display.print(F("-- Audio Reactivity --"));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    const int rowH = 10;
    for (int r = 0; r < AUDIO_ROW_COUNT; r++) {
      int y = 15 + r * rowH;
      if (fftRow == r)
        display.fillTriangle(1, y, 1, y+7, 7, y+3, SSD1306_WHITE);
      display.setCursor(13, y);
      if (r < 3) {
        display.print(audioParamNames[r]);
        display.print(F(": "));
        display.print(audioKnobRef(r), 2);
        if (menuState == FFT_EDIT && fftRow == r) {
          display.setCursor(116, y); display.print(F("<"));
        }
      } else {
        display.print(F("[ Back ]"));
      }
    }
  }

  display.display();
}

void handleEncoder() {
  int steps = encRaw / 4; if (steps == 0) return; encRaw -= steps * 4;

  switch (menuState) {

    case TOP_NAV:
      topRow = (TopRow)(((int)topRow + steps + TOP_ROW_COUNT) % TOP_ROW_COUNT);
      break;

    case TOP_EDIT:
      if (topRow == TOP_MODE) {
        int m = ((int)currentMode + steps + 4) % 4;
        if (m != (int)currentMode) {
          currentMode = (Mode)m;
          if      (currentMode == STATIC_MODE) startTransition(staticAnims[staticIndex]);
          else if (currentMode == AUDIO_MODE)  startTransition(audioAnims[audioIndex]);
          else if (currentMode == VOICE_MODE)  startTransition(voiceAnims[voiceIndex]);
          else                                 fill_solid(leds, NUM_LEDS, CRGB::Black);
          saveSettings(); printModeStatus();
        }
      } else if (topRow == TOP_ANIM) {
        int cnt = animCount(currentMode);
        if      (currentMode == STATIC_MODE) { staticIndex = ((staticIndex + steps) % cnt + cnt) % cnt; startTransition(staticAnims[staticIndex]); saveSettings(); printModeStatus(); }
        else if (currentMode == AUDIO_MODE)  { audioIndex  = ((audioIndex  + steps) % cnt + cnt) % cnt; startTransition(audioAnims[audioIndex]);  saveSettings(); printModeStatus(); }
        else if (currentMode == VOICE_MODE)  { voiceIndex  = ((voiceIndex  + steps) % cnt + cnt) % cnt; startTransition(voiceAnims[voiceIndex]);  saveSettings(); printModeStatus(); }
      } else if (topRow == TOP_PALETTE) {
        uint8_t newIdx = ((int)(currentPaletteIndex + steps) % (int)NUM_PALETTES + NUM_PALETTES) % NUM_PALETTES;
        startPaletteFade(newIdx);
        saveSettings(); printModeStatus();
      } else if (topRow == TOP_BRIGHT) {
        currentBrightness = (uint8_t)constrain(
          (int)currentBrightness + steps * 3, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
        applyBrightness();
      }
      break;

    case FFT_NAV:
      fftRow = ((fftRow + steps) % AUDIO_ROW_COUNT + AUDIO_ROW_COUNT) % AUDIO_ROW_COUNT;
      break;

    case FFT_EDIT: {
      float d = steps * 0.05f;
      if (fftRow == 2) audioKnobRef(2)      = constrain(audioKnobRef(2) + d, -1.0f, 1.0f);   // Band Tilt
      else             audioKnobRef(fftRow) = constrain(audioKnobRef(fftRow) + d, 0.1f, 3.0f);
      break;
    }
  }

  updateOLED();
}

void handleEncoderButton() {
  bool sw = digitalRead(ENC_SW); unsigned long now = millis();
  if (sw != swLastState) { swLastChange = now; swLastState = sw; }
  if (!((now - swLastChange) >= ENC_DEBOUNCE_MS && sw == LOW && !swFired)) {
    if (sw == HIGH) swFired = false; return;
  }
  swFired = true;

  switch (menuState) {

    case TOP_NAV:
      if (topRow == TOP_FFT) { menuState = FFT_NAV; fftRow = 0; }
      else                   { menuState = TOP_EDIT; }
      break;

    case TOP_EDIT:
      menuState = TOP_NAV;
      break;

    case FFT_NAV:
      if (fftRow == AUDIO_ROW_BACK) { menuState = TOP_NAV; topRow = TOP_FFT; }
      else                          { menuState = FFT_EDIT; }
      break;

    case FFT_EDIT:
      saveSettings(); menuState = FFT_NAV;
      break;
  }

  updateOLED();
}

void initOLEDEncoder() {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT),  updateEncoder, CHANGE);
  Wire2.begin();
  Wire2.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { Serial.println(F("OLED init failed")); return; }
  display.setRotation(0);
  display.clearDisplay();
  display.display();
  Serial.println(F("OLED ready"));
}

////////////////////////////////////////////////////////////
// ================= MODE STATUS PRINT =================
////////////////////////////////////////////////////////////


void printModeStatus() {
  Serial.println(F("\n┌─────────────────────────┐"));
  Serial.print(F("│ Mode      : ")); Serial.print(modeNames[currentMode]);
  int ml=strlen(modeNames[currentMode]); for(int i=ml;i<12;i++) Serial.print(' '); Serial.println(F("│"));
  Serial.print(F("│ Animation : "));
  const char* an="";
  if      (currentMode==STATIC_MODE) an=staticAnimNames[staticIndex];
  else if (currentMode==AUDIO_MODE)  an=audioAnimNames[audioIndex];
  else                               an=voiceAnimNames[voiceIndex];
  Serial.print(an); int al=strlen(an); for(int i=al;i<12;i++) Serial.print(' '); Serial.println(F("│"));
  Serial.print(F("│ Palette   : ")); Serial.print(currentPaletteIndex); Serial.print(F("/")); Serial.print(NUM_PALETTES-1);
  int pl=3+(currentPaletteIndex>9?2:1); for(int i=pl;i<12;i++) Serial.print(' '); Serial.println(F("│"));
  Serial.println(F("└─────────────────────────┘"));
}

////////////////////////////////////////////////////////////
// ================= SERIAL INTERFACE =================
////////////////////////////////////////////////////////////

String serialBuffer;

void printHelp() {
  Serial.println(F("\nEncoder: turn=scroll rows  press=enter/exit"));
  Serial.println(F("\nSerial commands:"));
  Serial.println(F("react <v>  beatsens <v>  tilt <v>   (audio knobs)"));
  Serial.println(F("status  save  help  mode  demo"));
  Serial.println(F("audio  — toggle live band/beat monitor"));
}

void printStatus() {
  Serial.println(F("\nAudio knobs:"));
  Serial.print(F("Reactivity : ")); Serial.println(reactivity, 2);
  Serial.print(F("Beat Sens  : ")); Serial.println(beatSensitivity, 2);
  Serial.print(F("Band Tilt  : ")); Serial.println(bandTilt, 2);
}

////////////////////////////////////////////////////////////
// ================= DEMO MODE =================
////////////////////////////////////////////////////////////

unsigned long demoLastChange = 0;
#define DEMO_INTERVAL_MS 30000

const int demoList[][2] = {
  {STATIC_MODE,0},{STATIC_MODE,1},{STATIC_MODE,2},{STATIC_MODE,3},
  {STATIC_MODE,4},{STATIC_MODE,5},{STATIC_MODE,6},{STATIC_MODE,7},
  {AUDIO_MODE,0},{AUDIO_MODE,1},{AUDIO_MODE,2},{AUDIO_MODE,3},{AUDIO_MODE,4},
  {AUDIO_MODE,5},{AUDIO_MODE,6},{AUDIO_MODE,7},{AUDIO_MODE,8},{AUDIO_MODE,9},
  {VOICE_MODE,0},{VOICE_MODE,1},{VOICE_MODE,2},{VOICE_MODE,3},
};
#define DEMO_TOTAL (sizeof(demoList)/sizeof(demoList[0]))
static int demoStep = 0;

void demoUpdate() {
  if (!demoEnabled) return;
  unsigned long now = millis();
  if (now-demoLastChange<DEMO_INTERVAL_MS) return;
  demoLastChange = now;
  int next; do { next=random(DEMO_TOTAL); } while (next==demoStep && DEMO_TOTAL>1);
  demoStep = next;
  startPaletteFade(random(NUM_PALETTES));
  currentMode = (Mode)demoList[demoStep][0];
  int idx = demoList[demoStep][1];
  if      (currentMode==STATIC_MODE) { staticIndex=idx; startTransition(staticAnims[staticIndex]); }
  else if (currentMode==AUDIO_MODE)  { audioIndex=idx;  startTransition(audioAnims[audioIndex]);  }
  else                               { voiceIndex=idx;  startTransition(voiceAnims[voiceIndex]);  }
  printModeStatus(); updateOLED();
}

void toggleDemo() {
  demoEnabled = !demoEnabled;
  if (demoEnabled) { demoLastChange=millis()-DEMO_INTERVAL_MS; demoStep=-1; Serial.println(F("Demo ON")); }
  else Serial.println(F("Demo OFF"));
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c=='\n'||c=='\r') {
      serialBuffer.trim();
      if      (serialBuffer.startsWith("react "))    reactivity      = constrain(serialBuffer.substring(6).toFloat(), 0.1f, 3.0f);
      else if (serialBuffer.startsWith("beatsens ")) beatSensitivity = constrain(serialBuffer.substring(9).toFloat(), 0.1f, 3.0f);
      else if (serialBuffer.startsWith("tilt "))     bandTilt        = constrain(serialBuffer.substring(5).toFloat(), -1.0f, 1.0f);
      else if (serialBuffer=="status")             printStatus();
      else if (serialBuffer=="mode")               printModeStatus();
      else if (serialBuffer=="save")             { saveSettings(); Serial.println(F("Saved.")); }
      else if (serialBuffer=="help")               printHelp();
      else if (serialBuffer=="demo")               toggleDemo();
      else if (serialBuffer=="audio")            { audioMonitorEnabled = !audioMonitorEnabled; Serial.println(audioMonitorEnabled ? F("Audio monitor ON") : F("Audio monitor OFF")); }
      else Serial.println(F("Unknown command. Type 'help'."));
      serialBuffer = "";
    } else serialBuffer += c;
  }
}

////////////////////////////////////////////////////////////
// ================= SETUP & LOOP =================
////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(500);
  printHelp();
  AudioMemory(40);

  staticAnims[0] = staticDiagonalFlow;
  staticAnims[1] = staticLissajous;
  staticAnims[2] = staticGravityParticles;
  staticAnims[3] = staticReactionDiffusion;
  staticAnims[4] = staticMobiusBraid;
  staticAnims[5] = staticPlasmaCube;
  staticAnims[6] = staticNoiseWorms;      // rebuilt: 3D-noise, flows across corners
  staticAnims[7] = staticCoral;           // 2nd reaction-diffusion (coral/maze regime)
  staticAnims[8] = placeholderAnim;       // free slot
  staticAnims[9] = placeholderAnim;       // free slot

  audioAnims[0] = audioTriAxis;
  audioAnims[1] = audioImpact;
  audioAnims[2] = audioCell;
  audioAnims[3] = audioBassBloom;
  audioAnims[4] = audioVortex;
  audioAnims[5] = audioPulseWeb;
  audioAnims[6] = audioSpectrumHelix;
  audioAnims[7] = audioEarthquake;
  audioAnims[8] = audioFlame;        // graph heat-diffusion (fire crawls the wireframe)
  audioAnims[9] = audioDendrite;     // graph lightning (charges walk + fork at corners)

  voiceAnims[0] = voiceBreathe;
  voiceAnims[1] = voiceFormant;
  voiceAnims[2] = voiceHarmonicRings;
  voiceAnims[3] = voiceSyllableSparks;
  for (int i=4; i<10; i++) voiceAnims[i] = placeholderAnim;

  EEPROM.begin();
  loadSettings();
  applyBrightness();
  printModeStatus();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_MIN);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTAGE, POWER_LIMIT_MA);

  buildCubeGeometry();
  buildRDNeighbors();
  initOLEDEncoder();
  //artnetInit();
  updateOLED();

  Serial.print(F("Free RAM: "));
  Serial.println(FreeStack());

  if      (currentMode==STATIC_MODE) currentAnim = staticAnims[staticIndex];
  else if (currentMode==AUDIO_MODE)  currentAnim = audioAnims[audioIndex];
  else                               currentAnim = voiceAnims[voiceIndex];
}

void loop() {
  float t = millis() * 0.001f;
  demoUpdate();
  updateAudio();
  printAudioMonitor();
  handleSerial();
  handleEncoder();
  handleEncoderButton();
  renderFrame(t);
  FastLED.show();

  unsigned long now = micros();
  if (frameDeadline == 0) frameDeadline = now;
  frameDeadline += (1000000UL / TARGET_FPS);
  if (now < frameDeadline) {
    delayMicroseconds(frameDeadline - now);
  }
}