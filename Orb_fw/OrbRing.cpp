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
#include "States.h"

OrbRing_t OrbRing;
extern Neopixels_t Leds;
static int32_t LedCnt;

static int32_t FadeTable[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,97,100};
static const int32_t kTable[MAX_TAIL_LEN+1] = { 0, 2500, 3300, 3640, 3780, 3860, 3920, 3960, 3990, 4011 };

// ==== Thread ====
static THD_WORKING_AREA(waOrbRingThread, 256);
__noreturn
static void OrbRingThread(void *arg) {
    chRegSetThreadName("OrbRing");
    while(true) OrbRing.IDraw();
}

static void MixInto(uint32_t Indx, ColorHSV_t ClrHSV) {
    Color_t Clr = ClrHSV.ToRGB();
    Clr.Brt = 100;
    Leds.ClrBuf[Indx].MixWith(Clr);
}

#if 1 // ============================== OrbRing ================================
void OrbRing_t::Init() {
    LedCnt = Leds.BandSetup[0].Length;
    Flares[0].StartRandom(0);
    // Create and start thread
    chThdCreateStatic(waOrbRingThread, sizeof(waOrbRingThread), NORMALPRIO, (tfunc_t)OrbRingThread, nullptr);
}

void OrbRing_t::Blink() {
    Leds.SetAll(clBlack);
    Leds.SetCurrentColors();
    chThdSleepMilliseconds(450);
}

void OrbRing_t::IDraw() {
    chThdSleepMilliseconds(FRAME_PERIOD_ms);
    Leds.SetAll((Color_t){0,0,0,0});
    switch(State) {
        case stateFlaring:
            // ==== Draw flares ====
            for(uint32_t i=0; i<FLARE_CNT; i++) {
                if(Flares[i].State != Flare_t::flstNone) Flares[i].Draw();
                if(Flares[i].NeedToStartNext) {
                    Flares[i].NeedToStartNext = false;
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
            // ==== On-Off layer ====
            if(PhaseState != stIdle) {
                for(int32_t i=0; i<LedCnt; i++) {
                    ColorHSV_t ClrH;
                    ClrH.FromRGB(Leds.ClrBuf[i]);
                    ClrH.V = (ClrH.V * OnOffBrt) / BRT_MAX;
                    Leds.ClrBuf[i].FromHSV(ClrH.H, ClrH.S, ClrH.V);
                }
            }
            break;

        case stateShowBounds:
            Flares[0].SteadyColor(0, ClrHL);
            Flares[0].Draw();
            Flares[1].SteadyColor(9, ClrHR);
            Flares[1].Draw();
            break;

        case stateChargingStatus:
            if(IsCharging()) {
                Charging.OnTick();
                Leds.ClrBuf[0] = Charging.ClrInProgress.ToRGB();
            }
            else { // Charging done
                Leds.ClrBuf[0] = Charging.ClrDone.ToRGB();
            }
            break;

        case stateDischarged:
            if(BlinkCnt & 1) Leds.ClrBuf[0] = clBlack;
            else Leds.ClrBuf[0] = clRed;
            if(chVTTimeElapsedSinceX(IStartTime) >= TIME_MS2I(207)) {
                IStartTime = chVTGetSystemTimeX();
                if(--BlinkCnt < 0) EvtQMain.SendNowOrExit(EvtMsg_t(evtIdLedsDone));
            }
            break;
    } // switch

    // ==== Draw it ====
    Leds.SetCurrentColors();
}

void OrbRing_t::StartShowBounds() {
    for(Flare_t &Flare: Flares) Flare.Stop();
}

void OrbRing_t::StopShowBounds() {
    Flares[0].StartRandom(0);
    Flares[1].State = Flare_t::flstNone;
    OnOffBrt = 0;
    FadeIn();
}


void OrbRing_t::DecreaseColorBounds() {
    if(--ClrHL < 0) ClrHL = 0;
    else {
        if(--ClrHR < 0) ClrHR = 0;
    }
    Printf("L: %d; R: %d\r", ClrHL, ClrHR);
}

void OrbRing_t::IncreaseColorBounds() {
    if(++ClrHR > 360) ClrHR = 360;
    else {
        if(++ClrHL > 360) ClrHL = 360;
    }
    Printf("L: %d; R: %d\r", ClrHL, ClrHR);
}

#if 1 // ============================ On-Off Layer =============================
void OnOffTmrCallback(void *p) {
    chSysLockFromISR();
    OrbRing.OnOnOffTmrTick();
    chSysUnlockFromISR();
}

void OrbRing_t::FadeIn() {
    PhaseState = stFadingIn;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
    chSysUnlock();
}

void OrbRing_t::FadeOut() {
    PhaseState = stFadingOut;
    chSysLock();
    StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
    chSysUnlock();
}

void OrbRing_t::OnOnOffTmrTick() {
    switch(PhaseState) {
        case stFadingIn:
            if(OnOffBrt == BRT_MAX) {
                PhaseState = stIdle;
            }
            else {
                OnOffBrt++;
                StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
            }
            break;

        case stFadingOut:
            if(OnOffBrt == 0) {
                PhaseState = stIdle;
                EvtQMain.SendNowOrExitI(EvtMsg_t(evtIdLedsDone));
            }
            else {
                OnOffBrt--;
                StartTimerI(ClrCalcDelay(OnOffBrt, SMOOTH_VAR));
            }
            break;

        default: break;
    }
}

#endif

void OrbRing_t::SetLen(int32_t Len, int32_t TailLen, int32_t ak1) {
//    for(Flare_t &Flare : Flares) {
//        Flare.Start(Len, TailLen, ak1);
//    }
}
void OrbRing_t::SetPeriod(uint32_t Per) {
//    for(Flare_t &Flare : Flares) {
//        Flare.TickPeriod_ms = Per;
//    }
}

void OrbRing_t::Start(uint32_t x0) {
//    for(Flare_t &Flare : Flares) {
//        Flare.x0 = x0;
//        Flare.Start(1, 3, kTable[3]);
//    }
}

void OrbRing_t::SetColor(ColorHSV_t hsv) {
    for(Flare_t &Flare : Flares) Flare.Clr = hsv;
}
#endif

#if 1 // ============================ Charging =================================
void Charging_t::OnTick() {
    ClrInProgress.V = FadeTable[VIndx];
    // Check if change required
    if(chVTTimeElapsedSinceX(IStartTime) >= TIME_MS2I(CHARGING_FADE_ms)) {
        IStartTime = chVTGetSystemTimeX();
        if(VFadeDir == rfRising) {
            if(++VIndx >= ((int32_t)countof(FadeTable) - 1)) {
                VIndx = countof(FadeTable) - 1;
                VFadeDir = rfFalling;
            }
        }
        else {
            if(--VIndx < 0) {
                VIndx = 0;
                VFadeDir = rfRising;
            }
        }
    } // if time elapsed
}
#endif

#if 1 // =============================== Flare =================================
static void IFlareTmrMoveCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnMoveTickI();
    chSysUnlockFromISR();
}

static void IFlareTmrFadeCallback(void *p) {
    chSysLockFromISR();
    ((Flare_t*)p)->OnFadeTickI();
    chSysUnlockFromISR();
}

void Flare_t::StartFadeTmr(uint32_t Duration_ms) {
    chVTSetI(&ITmrFade, TIME_MS2I(Duration_ms), IFlareTmrFadeCallback, this);
}

void Flare_t::StartRandom(uint32_t ax0) {
    chSysLock();
    chVTResetI(&ITmrMove);
    chVTResetI(&ITmrFade);
    chSysUnlock();
    // Choose params
    LenTail = 3;//Random::Generate(2, 4);
    MoveTick_ms = Random::Generate(45, 63);
    x0 = ax0;
//    Clr.H = Random::Generate(120, 330);
    if(OrbRing.ClrHL == OrbRing.ClrHR) Clr.H = OrbRing.ClrHL;
    else Clr.H = Random::Generate(OrbRing.ClrHL, OrbRing.ClrHR);
    Printf("%d\r", Clr.H);
    ConstructBrt(1, LenTail, kTable[LenTail]);
    MaxDuration_ms = Random::Generate(2007, 3600);
    // Start
    NeedToStartNext = false;
    HyperX = 0;
    FadeIndx = 0;
    FadeBrt = 0;
    State = flstFadeIn;
    chSysLock();
    chVTSetI(&ITmrMove, TIME_MS2I(MoveTick_ms), IFlareTmrMoveCallback, this);
    StartFadeTmr(FADE_CHANGE_PERIOD_ms);
    chSysUnlock();
}

void Flare_t::Stop() {
    chSysLock();
    chVTResetI(&ITmrMove);
    chVTResetI(&ITmrFade);
    chSysUnlock();
}

void Flare_t::SteadyColor(uint32_t ax0, int32_t ClrH) {
    LenTail = 3;
    x0 = ax0;
    Clr.H = ClrH;
    ConstructBrt(1, LenTail, kTable[LenTail]);
    HyperX = 0;
    FadeBrt = 100;
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

void Flare_t::OnMoveTickI() {
    if(State == flstNone) return;
    HyperX++;
    chVTSetI(&ITmrMove, TIME_MS2I(MoveTick_ms), IFlareTmrMoveCallback, this);
}

void Flare_t::OnFadeTickI() {
//    PrintfI("ft st=%d i=%d\r", State, FadeIndx);
    switch(State) {
        case flstNone: break;
        case flstFadeIn:
            FadeBrt = FadeTable[FadeIndx];
            FadeIndx++;
            if(FadeIndx >= (int32_t)countof(FadeTable)) {
                State = flstSteady;
                StartFadeTmr(MaxDuration_ms);
            }
            else StartFadeTmr(FADE_CHANGE_PERIOD_ms);
            break;
        case flstSteady:
            NeedToStartNext = true; // Steady ended
            State = flstFadeout;
            FadeIndx = countof(FadeTable) - 1;
            StartFadeTmr(FADE_CHANGE_PERIOD_ms);
            break;
        case flstFadeout:
            FadeBrt = FadeTable[FadeIndx];
            FadeIndx--;
            if(FadeIndx >= 0) StartFadeTmr(FADE_CHANGE_PERIOD_ms);
            else State = flstNone;
            break;
    } // switch
}

void Flare_t::Draw() {
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
