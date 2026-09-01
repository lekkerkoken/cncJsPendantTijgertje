#ifndef MACHINE_STATE_H
#define MACHINE_STATE_H

#include "Position.h"

enum MachineStatus
{
    MACHINE_DISCONNECTED,
    MACHINE_IDLE,
    MACHINE_RUN,
    MACHINE_HOLD,
    MACHINE_ALARM
};


struct MachineState
{
    MachineStatus machineStatus = MACHINE_DISCONNECTED;


    bool connected = false;


    Position machinePosition;


    Position workPosition;


    float feedrate = 0;


    int spindleSpeed = 0;
};


#endif