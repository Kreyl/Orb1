/*
 * States.h
 *
 *  Created on: 24 июн. 2020 г.
 *      Author: layst
 */

#pragma once

enum State_t {
    stateOff, stateFlaring, stateShowBounds, stateCharging, stateCharged, stateDischarged,
};

extern State_t State;
