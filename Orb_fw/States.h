/*
 * States.h
 *
 *  Created on: 24 июн. 2020 г.
 *      Author: layst
 */

#pragma once

enum State_t {
    stateFlaring=1, stateShowBounds=2, stateChargingStatus=3, stateDischarged=4,
};

extern State_t State;

bool IsCharging();
//enum ChargingStatus_t {Charging, ChargingDone};
//extern ChargingStatus_t ChargingStatus;

