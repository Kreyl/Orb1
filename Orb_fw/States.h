/*
 * States.h
 *
 *  Created on: 24 июн. 2020 г.
 *      Author: layst
 */

#pragma once

enum State_t {
    stateIdle=0, stateStart3min=1, stateStart5min=2, stateStart7min=3,
    stateYellow=4, stateOrange=5
};

extern State_t State;
