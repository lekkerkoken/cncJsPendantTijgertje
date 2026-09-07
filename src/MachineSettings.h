#ifndef MACHINE_SETTINGS_H
#define MACHINE_SETTINGS_H

#include "Position.h"
#include "ControllerType.h"

struct MachineSettings
{
    Position maxFeedrate;
    ControllerType controllerType =
        CONTROLLER_UNKNOWN;
};

#endif