/*
 * OrbRing.cpp
 *
 *  Created on: Jun 14, 2020
 *      Author: layst
 */

#include "OrbRing.h"
#include "EvtMsgIDs.h"
#include "MsgQ.h"
#include <math.h>

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
    for(Flare_t &Flare : Flares) {
        Flare.Len = 1;
        Flare.LenTail = 3;
        Flare.Clr = hsvGreen;
        Flare.TickPeriod_ms = 27;
        Flare.k1 = 3;
        Flare.k2 = 7;
        Flare.Construct();
    }
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

void OrbRing_t::SetLen(int32_t Len, int32_t TailLen, int32_t ak1, int32_t ak2) {
    for(Flare_t &Flare : Flares) {
        Flare.LenTail = TailLen;
        Flare.Len = Len;
        Flare.k1 = ak1;
        Flare.k2 = ak2;
        Flare.Construct();
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
    HyperX = 0;
    // ==== Construct brts ====
    LenTail *= FLARE_FACTOR;
    Len *= FLARE_FACTOR;
    TotalLen = LenTail + Len + LenTail;
    // Draw Front
    int32_t xi = LenTail-1, V = Clr.V;
    for(int32_t i=0; i<LenTail; i++) {
        V  = (V * k1) / k2;
        IBrt[xi--] = V;
    }
    // Draw body
    xi = LenTail;
    for(int32_t i=0; i<Len; i++) IBrt[xi++] = Clr.V;
    // Draw tail
    V = Clr.V;
    for(int32_t i=0; i<LenTail; i++) {
        V  = (V * k1) / k2;
        IBrt[xi++] = V;
    }
    // Start tmr
    chVTSet(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
    for(int i=0; i<TotalLen; i++) Printf("%d,", IBrt[i]);
    PrintfEOL();
}

void Flare_t::OnTickI() {
    HyperX++;
    chVTSetI(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::Draw() {
    int32_t HyperLedCnt = LedCnt * FLARE_FACTOR;
    while(HyperX >= HyperLedCnt) HyperX -= HyperLedCnt  ; // Process overflow
    ColorHSV_t IClr = Clr;
    int32_t Avg = 0, div = 0;
    int32_t OldX = HyperX / FLARE_FACTOR;
    for(int i=0; i<TotalLen; i++) {
        int32_t xi = (HyperX - i) / FLARE_FACTOR;
        if(xi < 0) xi += LedCnt;
        if(xi != OldX) {
            IClr.V = Avg / div;
            MixInto(OldX, IClr);
            OldX = xi;
            Avg = 0;
            div = 0;
        }
        Avg += IBrt[i];
        div++;
    }
}
#endif
