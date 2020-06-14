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

#define FRAME_PERIOD_ms     18
#define FLARE_CNT           1

class Flare_t {
private:
    virtual_timer_t ITmr;
public:
    int32_t Len, LenFront, LenBack;
    int32_t x;
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
    void SetTailLen(uint32_t Len);
    void SetPeriod(uint32_t Per);
    // Inner use
    void Draw();
};

extern OrbRing_t OrbRing;
