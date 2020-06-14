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

#define FRAME_PERIOD_ms     9
#define FLARE_CNT           1
#define FLARE_LEN_MAX       99
#define FLARE_FACTOR        5

class Flare_t {
private:
    int32_t IBrt[FLARE_LEN_MAX];
    virtual_timer_t ITmr;
    int32_t TotalLen;
public:
    int32_t Len, LenTail, k1, k2;
    int32_t HyperX;
    uint32_t TickPeriod_ms;
    ColorHSV_t Clr;
    void OnTickI();
    void Draw();
    void Construct();
};

class OrbRing_t {
private:
    virtual_timer_t ITmr;
    Flare_t Flares[FLARE_CNT];
public:
    void Init();
    void FadeIn() {}
    void FadeOut() {}
    void Blink();
    void SetColor(ColorHSV_t hsv) {}
    void SetLen(int32_t Len, int32_t TailLen, int32_t ak1, int32_t ak2);
    void SetPeriod(uint32_t Per);
    // Inner use
    void Draw();
};

extern OrbRing_t OrbRing;
