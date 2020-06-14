/*
 * OrbRing.cpp
 *
 *  Created on: Jun 14, 2020
 *      Author: layst
 */

#include "OrbRing.h"
#include "EvtMsgIDs.h"
#include "MsgQ.h"
//#include

OrbRing_t OrbRing;
extern Neopixels_t Leds;
static thread_reference_t ThdRef = nullptr;
static int32_t LedCnt;

static THD_WORKING_AREA(waOrbRingThread, 256);
__noreturn
static void OrbRingThread(void *arg) {
    chRegSetThreadName("OrbRing");
    while(true) {
        chSysLock();
        chThdSuspendS(&ThdRef);
        chSysUnlock();
        OrbRing.Draw();
    }
}

static void MixInto(uint32_t Indx, ColorHSV_t ClrHSV) {
    Color_t Clr = ClrHSV.ToRGB();
    Clr.Brt = 100;
    Leds.ClrBuf[Indx].MixWith(Clr);
}

#if 1 // ============================== OrbRing ================================
static void ITmrCallback(void *p) {
    chSysLockFromISR();
    chThdResumeI(&ThdRef, MSG_OK);
    chSysUnlockFromISR();
}

void OrbRing_t::Init() {
    LedCnt = Leds.BandSetup[0].Length;
    for(Flare_t &Flare : Flares) Flare.Construct();
    // Create and start thread
    chThdCreateStatic(waOrbRingThread, sizeof(waOrbRingThread), NORMALPRIO, (tfunc_t)OrbRingThread, nullptr);
    chVTSet(&ITmr, TIME_MS2I(FRAME_PERIOD_ms), ITmrCallback, nullptr);
}

void OrbRing_t::Blink() {
    chVTReset(&ITmr);
    Leds.SetAll(clBlack);
    Leds.SetCurrentColors();
    chThdSleepMilliseconds(450);
    Draw();
}

void OrbRing_t::Draw() {
    Leds.SetAll((Color_t){0,0,0,0});
    for(Flare_t &Flare : Flares) Flare.Draw();
    Leds.SetCurrentColors();
    chVTSet(&ITmr, TIME_MS2I(FRAME_PERIOD_ms), ITmrCallback, nullptr);
}

void OrbRing_t::SetTailLen(uint32_t Len) {
    for(Flare_t &Flare : Flares) {
        Flare.LenBack = Len;
        Flare.LenFront = Len;
    }
}
void OrbRing_t::SetPeriod(uint32_t Per) {
    for(Flare_t &Flare : Flares) {
        Flare.TickPeriod_ms = Per;
    }
}
#endif

#if 1 // =============================== Flare =================================
static void IFlareTmrCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnTickI();
    chSysUnlockFromISR();
}

void Flare_t::Construct() {
    chVTReset(&ITmr);
    Len = 1;
    LenFront = 0;
    LenBack = 0;
    x = 0;
    Clr = hsvGreen;
    TickPeriod_ms = 45;
    chVTSet(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::OnTickI() {
    x++;
    chVTSetI(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::Draw() {
    // Process overflow
    while(x >= LedCnt) x -= LedCnt;
    ColorHSV_t IClr = Clr;
    // Draw Front
    int32_t xi = x - LenFront;
    if(xi < 0) xi += LedCnt;
    IClr.V = Clr.V;
    for(int32_t i=0; i<LenFront; i++) {
        int32_t k = (LenFront < 4) ? 7 : LenFront + 4;
        IClr.V  = (IClr.V * LenFront) / k;
        MixInto(xi, IClr);
        if(++xi >= LedCnt) xi = 0;
    }
    // Draw body
    xi = x - LenFront;
    if(xi < 0) xi += LedCnt;
    for(int32_t i=0; i<Len; i++) {
        MixInto(xi, Clr);
        if(--xi < 0) xi = LedCnt-1;
    }
    // Draw tail
    IClr.V = Clr.V;
    for(int32_t i=0; i<LenBack; i++) {
        int32_t k = (LenBack < 4) ? 7 : LenBack + 4;
        IClr.V  = (IClr.V * LenBack) / k;
        MixInto(xi, IClr);
        if(--xi < 0) xi = LedCnt-1;
    }
}
#endif
