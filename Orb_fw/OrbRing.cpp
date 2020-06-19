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
//    Color_t OldClr = Leds.ClrBuf[Indx];
//    ColorHSV_t OldClr;
//    OldClr.FromRGB(Leds.ClrBuf[Indx]);
//    Color_t NewClr = ClrHSV.ToRGB();
//    Color_t OldClr = Leds.ClrBuf[Indx];
//    NewClr.R = ((((uint32_t)NewClr.R) * 100) / ClrHSV.V)  + ((((uint32_t)OldClr.R))
//
//    NewClr.H = (uint32_t)OldClr.H * 100UL + (uint32_t)ClrHSV.H

    Color_t Clr = ClrHSV.ToRGB();
    Clr.Brt = 100;
    Leds.ClrBuf[Indx].MixWith(Clr);
}

#if 1 // ============================== OrbRing ================================
static const int32_t kTable[MAX_TAIL_LEN+1] = { 0, 2500, 3300, 3640, 3780, 3860, 3920, 3960, 3990, 4011 };

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
    }
    Flares[0].StartRandom(0);
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
    for(uint32_t i=0; i<FLARE_CNT; i++) {
        if(Flares[i].State != Flare_t::flstNone) Flares[i].Draw();
        if(Flares[i].TimeToStartNext) {
            Flares[i].TimeToStartNext = false;
            // Construct next flare
            uint32_t j = i+1;
            if(j >= FLARE_CNT) j=0;
            if(j != i) {
                int32_t x0 = Flares[i].CurrX + LedCnt / FLARE_CNT + Flares[i].LenTail + 4;
                if(x0 >= LedCnt) x0 -= LedCnt;
                Flares[j].StartRandom(x0);
            }
        }
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
static int32_t FadeTable[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,97,100};

static void IFlareTmrCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnTickI();
    chSysUnlockFromISR();
}

void Flare_t::StartRandom(uint32_t ax0) {
    chVTReset(&ITmr);
    // Choose params
    LenTail = 3;//Random::Generate(2, 4);
    ConstructBrt(1, LenTail, kTable[LenTail]);
    TickPeriod_ms = Random::Generate(27, 36);
    x0 = ax0;//Random::Generate(0, 17);
    Clr.H = Random::Generate(120, 330);
    MaxDuration = Random::Generate(180, 360);
    // Start
    TimeToStartNext = false;
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
    TimeToStartNext = false;
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
                TimeToStartNext = true;
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
    int RealX = 0;
    while(indx < TotalLen) {
        IClr.V = (IBrt[indx] * FadeBrt) / 100;
        RealX = x0 + xi;
        if(RealX >= LedCnt) RealX -= LedCnt;
        MixInto(RealX, IClr);
        if(--xi < 0) xi += LedCnt;
        indx += FLARE_FACTOR;
    } // while
    CurrX = RealX;
}
#endif
