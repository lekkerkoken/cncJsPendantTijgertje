#ifndef MACHINE_STATE_H
#define MACHINE_STATE_H


enum MachineStatus
{
    MACHINE_DISCONNECTED,
    MACHINE_IDLE,
    MACHINE_RUN,
    MACHINE_HOLD,
    MACHINE_ALARM
};


struct Position
{
    float x = 0;
    float y = 0;
    float z = 0;
};


struct MachineState
{
    MachineStatus machineStatus = MACHINE_DISCONNECTED;


    bool connected = false;


    Position machinePosition;


    Position workPosition;


    Position maxFeedrate;


    float feedrate = 0;


    int spindleSpeed = 0;
};


#endif