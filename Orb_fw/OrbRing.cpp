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
        Flare.Clr = hsvGreen;
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
    for(Flare_t &Flare : Flares) {
        if(Flare.State == Flare_t::flstNone) {
            Flare.StartRandom();
        }
        else Flare.Draw();
    }
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
static int32_t FadeTable[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,19,21,23,25,27,29,31,33,36,39,42,45,48,51,55,59,63,67,72,77,82,88,94,100};

static void IFlareTmrCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnTickI();
    chSysUnlockFromISR();
}

void Flare_t::StartRandom() {
    chVTReset(&ITmr);
    // Choose params
    int32_t LenTail = Random::Generate(1, 6);
    ConstructBrt(1, LenTail, kTable[LenTail]);
    TickPeriod_ms = Random::Generate(18, 36);
    x0 = Random::Generate(0, 17);
    Clr.H = Random::Generate(60, 240);
    MaxDuration = Random::Generate(90, 360);
    // Start
    HyperX = 0;
    FadeIndx = 0;
    FadeTop = countof(FadeTable);
    State = flstFadeIn;
    chVTSet(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::Start(int32_t Len, int32_t LenTail, int32_t k1) {
    chVTReset(&ITmr);
    // Choose params
    ConstructBrt(Len, LenTail, k1);
    // Start
    HyperX = 0;
    FadeIndx = 0;
    FadeTop = countof(FadeTable);
    MaxDuration = 90;
    State = flstFadeIn;
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
//    for(int i=0; i<TotalLen; i++) Printf("%d,", IBrt[i]);
//    PrintfEOL();
}

void Flare_t::OnTickI() {
    if(State == flstNone) return;
    HyperX++;
    chVTSetI(&ITmr, TIME_MS2I(TickPeriod_ms), IFlareTmrCallback, this);
}

void Flare_t::Draw() {
    int32_t FadeBrt = 0;
    // Do fade
    switch(State) {
        case flstFadeIn:
            FadeBrt = FadeTable[FadeIndx];
            FadeIndx++;
            if(FadeIndx >= FadeTop) {
                State = flstSteady;
                FadeIndx = 0;
                FadeTop = MaxDuration;
            }
            break;
        case flstSteady:
            FadeBrt = 100;
            FadeIndx++;
            if(FadeIndx >= FadeTop) {
                State = flstFadeout;
                FadeIndx = countof(FadeTable) - 1;
            }
            break;
        case flstFadeout:
            FadeBrt = FadeTable[FadeIndx];
            FadeIndx--;
            if(FadeIndx < 0) {
                State = flstNone;
            }
            break;
        case flstNone: return;
    } // switch
    // Draw
    int32_t HyperLedCnt = LedCnt * FLARE_FACTOR;
    if(HyperX >= HyperLedCnt) HyperX -= HyperLedCnt;
    ColorHSV_t IClr = Clr;
    int32_t xi = HyperX / FLARE_FACTOR;
    int32_t indx = HyperX % FLARE_FACTOR;
    while(indx < TotalLen) {
        IClr.V = (IBrt[indx] * FadeBrt) / 100;
        int RealX = x0 + xi;
        if(RealX >= LedCnt) RealX -= LedCnt;
        MixInto(RealX, IClr);
        if(--xi < 0) xi += LedCnt;
        indx += FLARE_FACTOR;
    } // while
}
#endif
