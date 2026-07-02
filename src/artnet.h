#pragma once

#include "globals.h"
#include <Ethernet.h>
#include <EthernetUdp.h>

////////////////////////////////////////////////////////////
// ================= ARTNET CONFIG =================
////////////////////////////////////////////////////////////

#define ARTNET_CS_PIN            10
#define ARTNET_RST_PIN           9
#define ARTNET_PORT              6454
#define ARTNET_UNIVERSE_COUNT    3
#define ARTNET_LEDS_PER_UNIVERSE 170

////////////////////////////////////////////////////////////
// ================= STATE =================
////////////////////////////////////////////////////////////

byte           mac[]         = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
static EthernetUDP artnetUdp;
static bool        artnetReady    = false;
static uint8_t     artnetBuf[530];
static unsigned long artnetLastPacket = 0;
#define ARTNET_TIMEOUT_MS 5000

////////////////////////////////////////////////////////////
// ================= INIT =================
////////////////////////////////////////////////////////////

void artnetInit() {
    pinMode(ARTNET_CS_PIN,  OUTPUT);
    digitalWrite(ARTNET_CS_PIN, HIGH);   // deselect first

    pinMode(ARTNET_RST_PIN, OUTPUT);
    digitalWrite(ARTNET_RST_PIN, LOW);
    delay(10);
    digitalWrite(ARTNET_RST_PIN, HIGH);
    delay(200);   // give W5500 time to come up

    Ethernet.init(ARTNET_CS_PIN);        // tell library which CS pin

    IPAddress ip     (192, 168, 1, 177);
    IPAddress gateway(192, 168, 1,   1);
    IPAddress subnet (255, 255, 255,  0);

    Serial.println(F("ArtNet: starting Ethernet (static)..."));
    Ethernet.begin(mac, ip, gateway, gateway, subnet);

    // Confirm W5500 is detected
    Serial.print(F("ArtNet: hardware = "));
    Serial.println(Ethernet.hardwareStatus());   // 2 = W5500

    Serial.print(F("ArtNet: IP = "));
    Serial.println(Ethernet.localIP());

    artnetUdp.begin(ARTNET_PORT);
    artnetReady = true;
    Serial.println(F("ArtNet: ready on port 6454"));
}

////////////////////////////////////////////////////////////
// ================= PACKET HANDLER =================
////////////////////////////////////////////////////////////

static void artnetHandlePacket(uint8_t* data, uint16_t len) {
    if (len < 18) return;
    if (memcmp(data, "Art-Net\0", 8) != 0) return;

    uint16_t opcode  = data[8] | ((uint16_t)data[9] << 8);
    if (opcode != 0x5000) return;

    uint16_t universe = data[14] | ((uint16_t)data[15] << 8);
    if (universe >= ARTNET_UNIVERSE_COUNT) return;

    uint16_t dmxLen = ((uint16_t)data[16] << 8) | data[17];
    uint8_t* dmx    = data + 18;

    int ledOffset = universe * ARTNET_LEDS_PER_UNIVERSE;
    int count     = min((int)(dmxLen / 3), NUM_LEDS - ledOffset);
    if (count <= 0) return;

    for (int i = 0; i < count; i++)
        leds[ledOffset + i] = CRGB(dmx[i*3], dmx[i*3+1], dmx[i*3+2]);

    artnetLastPacket = millis();
}

////////////////////////////////////////////////////////////
// ================= RECEIVE LOOP =================
////////////////////////////////////////////////////////////

void artnetReceive() {
    if (!artnetReady || currentMode != ARTNET_MODE) return;
    int size = artnetUdp.parsePacket();
    if (size < 18 || size > (int)sizeof(artnetBuf)) { artnetUdp.flush(); return; }
    artnetUdp.read(artnetBuf, size);
    artnetHandlePacket(artnetBuf, size);
}

////////////////////////////////////////////////////////////
// ================= STATUS =================
////////////////////////////////////////////////////////////

void artnetPrintStatus() {
    Serial.println(F("\n--- ArtNet status ---"));
    Serial.print(F("Ready    : ")); Serial.println(artnetReady ? F("YES") : F("NO"));
    Serial.print(F("Hardware : ")); Serial.println(Ethernet.hardwareStatus());
    if (artnetReady) {
        Serial.print(F("IP       : ")); Serial.println(Ethernet.localIP());
        Serial.print(F("Port     : ")); Serial.println(ARTNET_PORT);
        Serial.print(F("Universes: 0-")); Serial.println(ARTNET_UNIVERSE_COUNT - 1);
        Serial.print(F("Last pkt : "));
        if (artnetLastPacket == 0) Serial.println(F("none"));
        else { Serial.print(millis()-artnetLastPacket); Serial.println(F("ms ago")); }
    }
}

bool artnetIsReceiving() {
    return artnetReady && artnetLastPacket > 0 &&
           (millis() - artnetLastPacket < ARTNET_TIMEOUT_MS);
}