/*
 * States.h
 *
 *  Created on: 24 июн. 2020 г.
 *      Author: layst
 */

#pragma once

enum State_t {
    stateOn=1, stateFading=2, stateOff=3,
};

extern State_t State;
