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
static const int32_t kTable[MAX_TAIL_LEN+1] = {
        0, 2500, 3300, 3640, 3780, 3860, 3920, 3960, 3990, 4011
};

static void ITmrCallback(void *p) {
    chSysLockFromISR();
    chThdResumeI(&ThdRef, MSG_OK);
    chSysUnlockFromISR();
}

void OrbRing_t::Init() {
    LedCnt = Leds.BandSetup[0].Length;
    for(Flare_t &Flare : Flares) {
        Flare.TickPeriod_ms = 18;
        Flare.x0 = 0;
        Flare.Clr = hsvBlue;
        Flare.Start(1, 3, kTable[3]);
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

void OrbRing_t::SetLen(int32_t Len, int32_t TailLen, int32_t ak1) {
    for(Flare_t &Flare : Flares) {
        Flare.Start(Len, TailLen, ak1);
    }
}
void OrbRing_t::SetPeriod(uint32_t Per) {
    for(Flare_t &Flare : Flares) {
        Flare.TickPeriod_ms = Per;
    }
}

void OrbRing_t::Start(uint32_t x0) {
    for(Flare_t &Flare : Flares) {
        Flare.x0 = x0;
        Flare.Start(1, 3, kTable[3]);
    }
}
#endif

#if 1 // =============================== Flare =================================
static void IFlareTmrCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnTickI();
    chSysUnlockFromISR();
}

void Flare_t::StartRandom() {
//    int32_t LenTail = 3;
//    Clr = hsvGreen;
//    TickPeriod_ms = 27;

}

void Flare_t::Start(int32_t Len, int32_t LenTail, int32_t k1) {
    chVTReset(&ITmr);
    // Choose params
//    x0 = 0;
    ConstructBrt(Len, LenTail, k1);
    // Start
    HyperX = 0;
    WasOver = false;
    chVTSet(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::ConstructBrt(int32_t Len, int32_t LenTail, int32_t k1) {
    LenTail *= FLARE_FACTOR;
    Len *= FLARE_FACTOR;
    TotalLen = LenTail + Len + LenTail;
    // Draw Front Tail
    int32_t xi = LenTail-1, V = Clr.V;
    for(int32_t i=0; i<LenTail; i++) {
        V  = (V * k1) / FLARE_K2;
        IBrt[xi--] = V;
    }
    // Draw body
    xi = LenTail;
    for(int32_t i=0; i<Len; i++) IBrt[xi++] = Clr.V;
    // Draw Back Tail
    V = Clr.V;
    for(int32_t i=0; i<LenTail; i++) {
        V  = (V * k1) / FLARE_K2;
        IBrt[xi++] = V;
    }
    for(int i=0; i<TotalLen; i++) Printf("%d,", IBrt[i]);
    PrintfEOL();
}

void Flare_t::OnTickI() {
    HyperX++;
    chVTSetI(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::Draw() {
    int32_t HyperLedCnt = LedCnt * FLARE_FACTOR;
    if(WasOver and HyperX >= TotalLen) return;
    if(HyperX >= HyperLedCnt) {
        HyperX -= HyperLedCnt;
        WasOver = true;
    }
    ColorHSV_t IClr = Clr;
    int32_t xi = HyperX / FLARE_FACTOR;
    int32_t indx = HyperX % FLARE_FACTOR;
    while(indx < TotalLen) {
        IClr.V = IBrt[indx];
        if(!WasOver) {
            if(xi >= 0) {
                int32_t RealX = xi;// + x0;
                if(RealX >= LedCnt) RealX -= LedCnt;
                MixInto(RealX, IClr);
            }
            xi--;
        }
        else {
            if(xi <= 0) MixInto(xi + LedCnt, IClr);
            xi--;
        }

        indx += FLARE_FACTOR;
    }
}
#endif
