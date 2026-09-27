#ifndef MACHINE_DATA_H
#define MACHINE_DATA_H


#include "MachineSettings.h"
#include "MachineState.h"

struct MachineData
{
    enum Type
    {
        MACHINE_DATA_NONE,
        MACHINE_DATA_SETTINGS,
        MACHINE_DATA_STATE        //ControllerIdentification,MachineAlarm
    };

    Type type = MACHINE_DATA_NONE;

    MachineSettings settings;
    MachineState state;
};

#endif