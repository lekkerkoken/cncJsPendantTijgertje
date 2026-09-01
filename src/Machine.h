#ifndef MACHINE_H
#define MACHINE_H

#include "MachineState.h"
#include "MachineSettings.h"


struct Machine
{
    MachineState state;
    MachineSettings settings;
};


#endif