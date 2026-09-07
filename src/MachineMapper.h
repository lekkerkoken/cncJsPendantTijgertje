#ifndef MACHINE_MAPPER_H
#define MACHINE_MAPPER_H

#include "JogCommand.h"
#include "MachineCommand.h"
#include "MachineSettings.h"
#include "ControllerType.h"

#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"

class MachineMapper
{
public:

    MachineCommand map(
        const JogCommand& jog,
        ControllerType type
    );

    MachineSettings map(
        const ControllerSettingsSnapshot& snapshot
    ) const;

    MachineState map(
        const ControllerStateSnapshot& snapshot,
        bool& valid
    ) const;

    MachineCommand mapHome(
        Axis axis,
        ControllerType type
    );

private:

    MachineCommand mapJogMove(
        const JogCommand& jog,
        ControllerType type

    );

    MachineCommand mapGrblJog(
        const JogCommand& jog
    );

    ControllerType controllerTypeFromString(
        const String& type
    ) const;

    MachineCommand unsupported() const;


};

#endif