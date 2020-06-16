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
#define MAX_TAIL_LEN        9
#define FLARE_FACTOR        8
#define FLARE_LEN_MAX       (FLARE_FACTOR * 20)

#define FLARE_K2            4096

class Flare_t {
private:
    int8_t IBrt[FLARE_LEN_MAX];
    virtual_timer_t ITmr;
    int32_t TotalLen;
    void ConstructBrt(int32_t Len, int32_t LenTail, int32_t k1);
    int32_t HyperX;
public:
    uint32_t TickPeriod_ms;
    ColorHSV_t Clr;
    void OnTickI();
    void Draw();
    void Start(int32_t Len, int32_t LenTail, int32_t k1);
    void StartRandom();
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
    void SetLen(int32_t Len, int32_t TailLen, int32_t ak1);
    void SetPeriod(uint32_t Per);
    // Inner use
    void Draw();
};

extern OrbRing_t OrbRing;
