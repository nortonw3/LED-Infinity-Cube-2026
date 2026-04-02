#include "globals.h"
#include "palettes.h"
#include "animations.h"

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

bool          fftDebugEnabled = false;
unsigned long lastDebugPrint  = 0;
#define DEBUG_INTERVAL_MS 200

// LED + geometry
CRGB  leds[NUM_LEDS];
CRGB  bufferA[NUM_LEDS];
CRGB  bufferB[NUM_LEDS];
Voxel voxels[NUM_LEDS];

// Animation support arrays
float sparkEnvs[NUM_LEDS]     = {0};
float stormSparkEnv[NUM_LEDS] = {0};
float rdA[NUM_LEDS];
float rdB[NUM_LEDS];
float rdA2[NUM_LEDS];
float rdB2[NUM_LEDS];
int   rdNeighborL[NUM_LEDS];
int   rdNeighborR[NUM_LEDS];

// Audio
float fftBins[FFT_BINS];
float bass = 0, mid = 0, high = 0;
float voiceLevel = 0, speechEnergy = 0, sparkle = 0;
float fftGain         = 1.0f;
float bassGain        = 1.0f;
float midGain         = 1.0f;
float highGain        = 1.0f;
float voiceGain       = 1.0f;
float smoothingFactor = 0.8f;

// Syllable detector state (used by voice animations)
float sylAvg        = 0;
float sylEnv        = 0;
bool  syllableOnset = false;
float sylLastEnergy = 0;

// Mode + animation indices
Mode currentMode = STATIC_MODE;
int  staticIndex = 0, audioIndex = 0, voiceIndex = 0;

AnimFunc staticAnims[10];
AnimFunc audioAnims[10];
AnimFunc voiceAnims[10];

// Crossfade
AnimFunc currentAnim;
AnimFunc nextAnim;
bool     transitioning      = false;
float    transitionStart    = 0;
float    transitionDuration = 2.0f;

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

#define EEPROM_MODE_ADDR         0
#define EEPROM_STATIC_INDEX_ADDR 1
#define EEPROM_AUDIO_INDEX_ADDR  2
#define EEPROM_VOICE_INDEX_ADDR  3
#define EEPROM_PALETTE_ADDR      4

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
  EEPROM.write(EEPROM_MODE_ADDR,         currentMode);
  EEPROM.write(EEPROM_STATIC_INDEX_ADDR, staticIndex);
  EEPROM.write(EEPROM_AUDIO_INDEX_ADDR,  audioIndex);
  EEPROM.write(EEPROM_VOICE_INDEX_ADDR,  voiceIndex);
  EEPROM.write(EEPROM_PALETTE_ADDR,      currentPaletteIndex);
  EEPROMWriteFloat(10, fftGain);
  EEPROMWriteFloat(14, bassGain);
  EEPROMWriteFloat(18, midGain);
  EEPROMWriteFloat(22, highGain);
  EEPROMWriteFloat(26, voiceGain);
  EEPROMWriteFloat(30, smoothingFactor);
}

void loadSettings() {
  currentMode = (Mode)EEPROM.read(EEPROM_MODE_ADDR);
  staticIndex = EEPROM.read(EEPROM_STATIC_INDEX_ADDR);
  audioIndex  = EEPROM.read(EEPROM_AUDIO_INDEX_ADDR);
  voiceIndex  = EEPROM.read(EEPROM_VOICE_INDEX_ADDR);

  if (currentMode > VOICE_MODE) currentMode = STATIC_MODE;
  if (staticIndex >= 10)        staticIndex = 0;
  if (audioIndex  >= 10)        audioIndex  = 0;
  if (voiceIndex  >= 10)        voiceIndex  = 0;

  currentPaletteIndex = EEPROM.read(EEPROM_PALETTE_ADDR);
  if (currentPaletteIndex >= NUM_PALETTES) currentPaletteIndex = 0;
  currentPalette  = palettes[currentPaletteIndex];
  previousPalette = palettes[currentPaletteIndex];
  paletteFading   = false;

  fftGain         = EEPROMReadFloat(10);
  bassGain        = EEPROMReadFloat(14);
  midGain         = EEPROMReadFloat(18);
  highGain        = EEPROMReadFloat(22);
  voiceGain       = EEPROMReadFloat(26);
  smoothingFactor = EEPROMReadFloat(30);

  if (fftGain        <= 0 || fftGain        > 10)        fftGain        = 1.0f;
  if (bassGain       <= 0 || bassGain       > 10)        bassGain       = 1.0f;
  if (midGain        <= 0 || midGain        > 10)        midGain        = 1.0f;
  if (highGain       <= 0 || highGain       > 10)        highGain       = 1.0f;
  if (voiceGain      <= 0 || voiceGain      > 10)        voiceGain      = 1.0f;
  if (smoothingFactor < 0.5f || smoothingFactor > 0.99f) smoothingFactor = 0.8f;
}

////////////////////////////////////////////////////////////
// ================= BRIGHTNESS =================
////////////////////////////////////////////////////////////

void updateBrightness() {
  static uint8_t lastBrightness = BRIGHTNESS_MIN;
  int raw = analogRead(BRIGHTNESS_POT_PIN);
  uint8_t brightness = map(raw, 0, 1023, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
  if (abs((int)brightness - (int)lastBrightness) > 3) {
    lastBrightness = brightness;
    float bFrac = (float)brightness / 255.0f;
    uint32_t powerLimit = (uint32_t)(POWER_LIMIT_MA * bFrac * bFrac);
    powerLimit = max(powerLimit, (uint32_t)500);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTAGE, powerLimit);
  }
}

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
// ================= FFT PROCESSING =================
////////////////////////////////////////////////////////////

void updateFFTRaw() {
  if (fft1024.available())
    for (int i = 0; i < FFT_BINS; i++) fftBins[i] = fft1024.read(i);
}

void updateFFTFeatures() {
  float low=0, mids=0, highs=0, speechBand=0, total=0;
  for (int i=2;  i<=5;  i++) low        += fftBins[i];
  for (int i=6;  i<=20; i++) mids       += fftBins[i];
  for (int i=21; i<=80; i++) highs      += fftBins[i];
  for (int i=6;  i<=40; i++) speechBand += fftBins[i];
  for (int i=2;  i<=80; i++) total      += fftBins[i];

  low   *= fftGain * bassGain;
  mids  *= fftGain * midGain;
  highs *= fftGain * highGain;
  total *= fftGain * voiceGain;

  bass         = bass         * smoothingFactor + low        * (1.0f - smoothingFactor);
  mid          = mid          * smoothingFactor + mids       * (1.0f - smoothingFactor);
  high         = high         * smoothingFactor + highs      * (1.0f - smoothingFactor);
  voiceLevel   = voiceLevel   * smoothingFactor + total      * (1.0f - smoothingFactor);
  speechEnergy = speechEnergy * smoothingFactor + speechBand * (1.0f - smoothingFactor);
  sparkle      = sparkle      * smoothingFactor + highs      * (1.0f - smoothingFactor);

  if (bass         < 0.005f) bass         = 0;
  if (mid          < 0.005f) mid          = 0;
  if (high         < 0.005f) high         = 0;
  if (voiceLevel   < 0.008f) voiceLevel   = 0;
  if (speechEnergy < 0.008f) speechEnergy = 0;
  if (sparkle      < 0.005f) sparkle      = 0;
}

////////////////////////////////////////////////////////////
// ================= AUTO CALIBRATION =================
////////////////////////////////////////////////////////////

#define CAL_ATTACK    0.001f
#define CAL_DECAY     0.0003f
#define CAL_TARGET    0.4f
#define CAL_MIN_GAIN  0.2f
#define CAL_MAX_GAIN  12.0f
#define CAL_WARMUP_MS 3000

static float calBassGain  = 1.0f;
static float calMidGain   = 1.0f;
static float calHighGain  = 1.0f;
static float calVoiceGain = 1.0f;
static float peakBass     = 0;
static float peakMid      = 0;
static float peakHigh     = 0;
static float peakVoice    = 0;
bool autoCalEnabled = false;

void updateAutoCalibration() {
  if (!autoCalEnabled) return;
  if (millis() < CAL_WARMUP_MS) return;
  float pkAttack = 0.15f, pkDecay = 0.003f;
  peakBass  = bass       > peakBass  ? bass       * pkAttack + peakBass  * (1-pkAttack) : peakBass  * (1-pkDecay);
  peakMid   = mid        > peakMid   ? mid        * pkAttack + peakMid   * (1-pkAttack) : peakMid   * (1-pkDecay);
  peakHigh  = high       > peakHigh  ? high       * pkAttack + peakHigh  * (1-pkAttack) : peakHigh  * (1-pkDecay);
  peakVoice = voiceLevel > peakVoice ? voiceLevel * pkAttack + peakVoice * (1-pkAttack) : peakVoice * (1-pkDecay);
  if (peakBass  > 0.001f) { float e=CAL_TARGET/peakBass;  if(e<1)calBassGain -=CAL_ATTACK; else calBassGain +=CAL_DECAY; calBassGain =constrain(calBassGain, CAL_MIN_GAIN,CAL_MAX_GAIN); }
  if (peakMid   > 0.001f) { float e=CAL_TARGET/peakMid;   if(e<1)calMidGain  -=CAL_ATTACK; else calMidGain  +=CAL_DECAY; calMidGain  =constrain(calMidGain,  CAL_MIN_GAIN,CAL_MAX_GAIN); }
  if (peakHigh  > 0.001f) { float e=CAL_TARGET/peakHigh;  if(e<1)calHighGain -=CAL_ATTACK; else calHighGain +=CAL_DECAY; calHighGain =constrain(calHighGain, CAL_MIN_GAIN,CAL_MAX_GAIN); }
  if (peakVoice > 0.001f) { float e=CAL_TARGET/peakVoice; if(e<1)calVoiceGain-=CAL_ATTACK; else calVoiceGain+=CAL_DECAY; calVoiceGain=constrain(calVoiceGain,CAL_MIN_GAIN,CAL_MAX_GAIN); }
  bass *= calBassGain; mid *= calMidGain; high *= calHighGain;
  voiceLevel *= calVoiceGain; speechEnergy *= calVoiceGain; sparkle *= calHighGain;
}

void printCalStatus() {
  Serial.println(F("\n--- Auto calibration status ---"));
  Serial.print(F("Enabled : ")); Serial.println(autoCalEnabled ? F("YES") : F("NO"));
  Serial.print(F("Bass  gain: ")); Serial.print(calBassGain,3);  Serial.print(F("  peak: ")); Serial.println(peakBass,3);
  Serial.print(F("Mid   gain: ")); Serial.print(calMidGain,3);   Serial.print(F("  peak: ")); Serial.println(peakMid,3);
  Serial.print(F("High  gain: ")); Serial.print(calHighGain,3);  Serial.print(F("  peak: ")); Serial.println(peakHigh,3);
  Serial.print(F("Voice gain: ")); Serial.print(calVoiceGain,3); Serial.print(F("  peak: ")); Serial.println(peakVoice,3);
}

void resetCalibration() {
  calBassGain = calMidGain = calHighGain = calVoiceGain = 1.0f;
  peakBass = peakMid = peakHigh = peakVoice = 0;
  Serial.println(F("Calibration reset."));
}

////////////////////////////////////////////////////////////
// ================= SYLLABLE DETECTOR =================
////////////////////////////////////////////////////////////

void updateSyllableDetector() {
  sylAvg = sylAvg * 0.982f + speechEnergy * 0.018f;
  syllableOnset = false;
  if (speechEnergy > sylAvg * 1.6f && speechEnergy > 0.015f && speechEnergy > sylLastEnergy * 1.2f) {
    syllableOnset = true;
    sylEnv        = 1.0f;
  }
  sylLastEnergy = speechEnergy;
  sylEnv       *= 0.91f;
}

void updateAudio() { updateFFTRaw(); updateFFTFeatures(); updateAutoCalibration(); updateSyllableDetector(); }

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
  if (!transitioning) { currentAnim(leds, t); return; }
  currentAnim(bufferA, t); nextAnim(bufferB, t);
  float p = (t - transitionStart) / transitionDuration;
  if (p >= 1.0f) {
    transitioning = false; currentAnim = nextAnim;
    fill_solid(bufferA, NUM_LEDS, CRGB::Black);
    fill_solid(bufferB, NUM_LEDS, CRGB::Black);
    currentAnim(leds, t); return;
  }
  uint8_t ba = p * 255;
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = blend(bufferA[i], bufferB[i], ba);
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

const char* modeNames[3] = { "STATIC", "AUDIO", "VOICE" };

////////////////////////////////////////////////////////////
// ================= MENU SYSTEM =================
////////////////////////////////////////////////////////////

enum MenuState { TOP_NAV, TOP_EDIT, FFT_NAV, FFT_EDIT };
enum TopRow    { TOP_MODE, TOP_ANIM, TOP_PALETTE, TOP_FFT, TOP_ROW_COUNT };

#define FFT_ROW_BACK  5
#define FFT_ROW_COUNT 6

MenuState menuState = TOP_NAV;
TopRow    topRow    = TOP_MODE;
int       fftRow    = 0;

const char* fftParamNames[5] = { "Gain","Bass","Mid","High","Voice" };

float& fftParamRef(int row) {
  switch (row) {
    case 0: return fftGain;
    case 1: return bassGain;
    case 2: return midGain;
    case 3: return highGain;
    case 4: return voiceGain;
    default: return fftGain;
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

  if (menuState == TOP_NAV || menuState == TOP_EDIT) {

    if (demoEnabled) { display.setCursor(86, 0); display.print(F("[DEMO]")); }

    if (topRow == TOP_MODE)
      display.fillTriangle(1,4,1,11,7,7, SSD1306_WHITE);
    display.setCursor(13, 4);
    display.print(F("Mode   :"));
    if (menuState == TOP_EDIT && topRow == TOP_MODE) display.print(F("<"));
    else display.print(F(" "));
    display.print(modeNames[currentMode]);
    display.drawFastHLine(0, 17, 128, SSD1306_WHITE);

    if (topRow == TOP_ANIM)
      display.fillTriangle(1,22,1,29,7,25, SSD1306_WHITE);
    display.setCursor(13, 22);
    display.print(F("Anim   :"));
    if (menuState == TOP_EDIT && topRow == TOP_ANIM) display.print(F("<"));
    else display.print(F(" "));
    display.print(animName);
    display.drawFastHLine(0, 35, 128, SSD1306_WHITE);

    if (topRow == TOP_PALETTE)
      display.fillTriangle(1,40,1,47,7,43, SSD1306_WHITE);
    display.setCursor(13, 40);
    display.print(F("Palette:"));
    if (menuState == TOP_EDIT && topRow == TOP_PALETTE) display.print(F("<"));
    else display.print(F(" "));
    display.print(paletteNames[currentPaletteIndex]);
    display.drawFastHLine(0, 53, 128, SSD1306_WHITE);

    if (topRow == TOP_FFT)
      display.fillTriangle(1,56,1,63,7,59, SSD1306_WHITE);
    display.setCursor(13, 56);
    display.print(F("FFT Menu >"));

  } else {

    display.setCursor(0, 0);
    display.print(F("-- FFT Settings --"));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    const int rowH = 8;
    for (int r = 0; r < FFT_ROW_COUNT; r++) {
      int y = 13 + r * rowH;
      if (fftRow == r)
        display.fillTriangle(1, y, 1, y+7, 7, y+3, SSD1306_WHITE);
      display.setCursor(13, y);
      if (r < 5) {
        display.print(fftParamNames[r]);
        display.print(F(": "));
        display.print(fftParamRef(r), 2);
        if (menuState == FFT_EDIT && fftRow == r) {
          display.setCursor(110, y); display.print(F("<"));
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
        int m = ((int)currentMode + steps + 3) % 3;
        if (m != (int)currentMode) {
          currentMode = (Mode)m;
          if      (currentMode == STATIC_MODE) startTransition(staticAnims[staticIndex]);
          else if (currentMode == AUDIO_MODE)  startTransition(audioAnims[audioIndex]);
          else                                 startTransition(voiceAnims[voiceIndex]);
          saveSettings(); printModeStatus();
        }
      } else if (topRow == TOP_ANIM) {
        if      (currentMode == STATIC_MODE) { staticIndex = ((staticIndex + steps) % 10 + 10) % 10; startTransition(staticAnims[staticIndex]); }
        else if (currentMode == AUDIO_MODE)  { audioIndex  = ((audioIndex  + steps) % 10 + 10) % 10; startTransition(audioAnims[audioIndex]);  }
        else                                 { voiceIndex  = ((voiceIndex  + steps) % 10 + 10) % 10; startTransition(voiceAnims[voiceIndex]);  }
        saveSettings(); printModeStatus();
      } else if (topRow == TOP_PALETTE) {
        uint8_t newIdx = ((int)(currentPaletteIndex + steps) % (int)NUM_PALETTES + NUM_PALETTES) % NUM_PALETTES;
        startPaletteFade(newIdx);
        saveSettings(); printModeStatus();
      }
      break;
    case FFT_NAV:
      fftRow = ((fftRow + steps) % FFT_ROW_COUNT + FFT_ROW_COUNT) % FFT_ROW_COUNT;
      break;
    case FFT_EDIT:
      fftParamRef(fftRow) = constrain(fftParamRef(fftRow) + steps * 0.05f, 0.1f, 10.0f);
      break;
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
      if (fftRow == FFT_ROW_BACK) { menuState = TOP_NAV; topRow = TOP_FFT; }
      else                        { menuState = FFT_EDIT; }
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
  Serial.println(F("gain <v>  bass <v>  mid <v>  high <v>  voice <v>  smooth <v>"));
  Serial.println(F("status  save  help  mode"));
  Serial.println(F("fftdebug on/off  cal on/off/reset/cal"));
  Serial.println(F("demo — toggle demo mode\n"));
}

void printStatus() {
  Serial.println(F("\nFFT Settings:"));
  Serial.print(F("Gain: "));       Serial.println(fftGain);
  Serial.print(F("Bass Gain: "));  Serial.println(bassGain);
  Serial.print(F("Mid Gain: "));   Serial.println(midGain);
  Serial.print(F("High Gain: "));  Serial.println(highGain);
  Serial.print(F("Voice Gain: ")); Serial.println(voiceGain);
  Serial.print(F("Smoothing: "));  Serial.println(smoothingFactor);
}

void printFFTDebug() {
  if (!fftDebugEnabled) return;
  if (millis()-lastDebugPrint<DEBUG_INTERVAL_MS) return;
  lastDebugPrint=millis();
  Serial.println(F("\n--- FFT debug ---"));
  Serial.print(F("BASS  2-5 : ")); for(int i=2;i<=5;i++){Serial.print(20.0f*log10f(fftBins[i]+1e-6f),1);Serial.print(F("dB "));} Serial.println();
  Serial.print(F("MID   6-20: ")); for(int i=6;i<=20;i+=2){Serial.print(20.0f*log10f(fftBins[i]+1e-6f),1);Serial.print(F("dB "));} Serial.println();
  Serial.print(F("HIGH 21-80: ")); for(int i=21;i<=80;i+=10){Serial.print(20.0f*log10f(fftBins[i]+1e-6f),1);Serial.print(F("dB "));} Serial.println();
  Serial.print(F("bass=")); Serial.print(bass,3); Serial.print(F(" mid=")); Serial.print(mid,3);
  Serial.print(F(" high=")); Serial.print(high,3); Serial.print(F(" voice=")); Serial.print(voiceLevel,3);
  Serial.print(F(" speech=")); Serial.print(speechEnergy,3); Serial.print(F(" sparkle=")); Serial.println(sparkle,3);
  Serial.print(F("BASS bar: [")); int bars=constrain((int)(bass*40),0,40); for(int i=0;i<40;i++) Serial.print(i<bars?'|':' '); Serial.println(F("]"));
}

////////////////////////////////////////////////////////////
// ================= DEMO MODE =================
////////////////////////////////////////////////////////////

unsigned long demoLastChange = 0;
#define DEMO_INTERVAL_MS 30000

const int demoList[][2] = {
  {STATIC_MODE,0},{STATIC_MODE,1},{STATIC_MODE,2},{STATIC_MODE,3},{STATIC_MODE,4},
  {STATIC_MODE,5},{STATIC_MODE,6},{STATIC_MODE,7},{STATIC_MODE,8},{STATIC_MODE,9},
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
      if      (serialBuffer.startsWith("gain "))   fftGain         = serialBuffer.substring(5).toFloat();
      else if (serialBuffer.startsWith("bass "))   bassGain        = serialBuffer.substring(5).toFloat();
      else if (serialBuffer.startsWith("mid "))    midGain         = serialBuffer.substring(4).toFloat();
      else if (serialBuffer.startsWith("high "))   highGain        = serialBuffer.substring(5).toFloat();
      else if (serialBuffer.startsWith("voice "))  voiceGain       = serialBuffer.substring(6).toFloat();
      else if (serialBuffer.startsWith("smooth ")) smoothingFactor = serialBuffer.substring(7).toFloat();
      else if (serialBuffer=="status")             printStatus();
      else if (serialBuffer=="mode")               printModeStatus();
      else if (serialBuffer=="save")             { saveSettings(); Serial.println(F("Saved.")); }
      else if (serialBuffer=="help")               printHelp();
      else if (serialBuffer=="fftdebug on")      { fftDebugEnabled=true;  Serial.println(F("FFT debug ON"));  }
      else if (serialBuffer=="fftdebug off")     { fftDebugEnabled=false; Serial.println(F("FFT debug OFF")); }
      else if (serialBuffer=="cal on")           { autoCalEnabled=true;   Serial.println(F("Auto cal ON"));   }
      else if (serialBuffer=="cal off")          { autoCalEnabled=false;  Serial.println(F("Auto cal OFF"));  }
      else if (serialBuffer=="cal reset")          resetCalibration();
      else if (serialBuffer=="cal")                printCalStatus();
      else if (serialBuffer=="demo")               toggleDemo();
      else Serial.println(F("Unknown command. Type 'help'."));
      serialBuffer = "";
    } else serialBuffer += c;
  }
}

////////////////////////////////////////////////////////////
// ================= SETUP & LOOP =================
////////////////////////////////////////////////////////////

elapsedMicros frameTimer;

void setup() {
  Serial.begin(115200);
  delay(500);
  printHelp();
  AudioMemory(40);

  staticAnims[0] = staticDiagonalFlow;
  staticAnims[1] = staticEquatorTrace;
  staticAnims[2] = staticDroplets;
  staticAnims[3] = staticSparkle;
  staticAnims[4] = staticReactionDiffusion;
  staticAnims[5] = staticCometChase;
  staticAnims[6] = staticEdgeBreathe;
  staticAnims[7] = staticPlasmaCube;
  staticAnims[8] = staticZipper;
  staticAnims[9] = staticNoiseWorms;

  audioAnims[0] = audioTriAxis;
  audioAnims[1] = audioImpact;
  audioAnims[2] = audioStorm;
  audioAnims[3] = audioFreqBands;
  audioAnims[4] = audioBassBloom;
  audioAnims[5] = audioVortex;
  audioAnims[6] = audioResonanceNodes;
  audioAnims[7] = audioPulseWeb;
  audioAnims[8] = audioSpectrumHelix;
  audioAnims[9] = audioEarthquake;

  voiceAnims[0] = voiceBreathe;
  voiceAnims[1] = voiceFormant;
  voiceAnims[2] = voiceHarmonicRings;
  voiceAnims[3] = voiceSyllableSparks;
  for (int i=4; i<10; i++) voiceAnims[i] = placeholderAnim;

  EEPROM.begin();
  loadSettings();
  printModeStatus();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_MIN);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(POWER_VOLTAGE, POWER_LIMIT_MA);

  buildCubeGeometry();
  buildRDNeighbors();
  initParticles();
  initOLEDEncoder();
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
  printFFTDebug();
  handleSerial();
  handleEncoder();
  handleEncoderButton();
  renderFrame(t);
  FastLED.show();
  updateBrightness();
  while (frameTimer < (1000000UL / TARGET_FPS));
  frameTimer = 0;
}