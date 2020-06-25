/*
 * OrbRing.h
 *
 *  Created on: Jun 14, 2020
 *      Author: layst
 */

#pragma once

#include <inttypes.h>
#include "color.h"
#include "ws2812b.h"
#include "kl_lib.h"
#include "ch.h"

// Initial Color
#define CLR_H_BOTTOM            100
#define CLR_H_TOP               280

// On-off layer
#define SMOOTH_VAR              180

// Flares
#define FRAME_PERIOD_ms         9
#define FLARE_CNT               2
#define FADE_CHANGE_PERIOD_ms   36

// Consts
#define MAX_TAIL_LEN            9
#define FLARE_FACTOR            8
#define FLARE_LEN_MAX           (FLARE_FACTOR * 20)
#define FLARE_K2                4096
#define BRT_MAX                 255L // Do not touch

class Flare_t {
private:
    int8_t IBrt[FLARE_LEN_MAX];
    virtual_timer_t ITmrMove, ITmrFade;
    int32_t TotalLen;
    void ConstructBrt(int32_t Len, int32_t LenTail, int32_t k1);
    int32_t HyperX;
    void StartFadeTmr(uint32_t Duration_ms);
    int32_t FadeIndx, FadeBrt;
    int32_t MaxDuration_ms;
public:
    enum State_t { flstNone, flstFadeIn, flstSteady, flstFadeout } State = flstNone;
    uint32_t MoveTick_ms;
    int32_t x0, CurrX, LenTail;
    ColorHSV_t Clr{120, 100, 100};
    bool NeedToStartNext;
    void OnMoveTickI();
    void OnFadeTickI();
    void Draw();
    void StartRandom(uint32_t ax0);
};

#define CHARGING_FADE_ms            45
#define CLR_CHARGING_IN_PROGRESS    hsvYellow
#define CLR_CHARGING_DONE           hsvGreen
class Charging_t {
private:
    systime_t IStartTime = 0;
    int32_t VIndx = 0;
    RiseFall_t VFadeDir = rfRising;
public:
    ColorHSV_t ClrInProgress = CLR_CHARGING_IN_PROGRESS;
    ColorHSV_t ClrDone = CLR_CHARGING_DONE;
    void OnTick();
};


void OnOffTmrCallback(void *p);

class OrbRing_t {
private:
    virtual_timer_t IOnOffTmr;
    Flare_t Flares[FLARE_CNT];
    int32_t OnOffBrt = 0;
    enum PhaseState_t {stIdle, stFadingOut, stFadingIn} PhaseState;
    void StartTimerI(uint32_t ms) { chVTSetI(&IOnOffTmr, TIME_MS2I(ms), OnOffTmrCallback, nullptr); }
    Charging_t Charging;
    // Discharged
    systime_t IStartTime = 0;
    int32_t BlinkCnt = 9;
public:
    uint32_t HBottom = CLR_H_BOTTOM, HTop = CLR_H_TOP;
    void Init();
    void FadeIn(bool FromZero = false);
    void FadeOut();
    void Blink();
    void SetColor(ColorHSV_t hsv);
    void SetLen(int32_t Len, int32_t TailLen, int32_t ak1);
    void SetPeriod(uint32_t Per);
    void Start(uint32_t x0);
    void DecreaseColorBounds();
    void IncreaseColorBounds();
    // Inner use
    void IDraw();
    void OnOnOffTmrTick();
};

extern OrbRing_t OrbRing;
