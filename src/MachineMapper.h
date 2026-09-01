#ifndef MACHINE_MAPPER_H
#define MACHINE_MAPPER_H

#include "JogCommand.h"
#include "MachineCommand.h"
#include "MachineSettings.h"

#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"

class MachineMapper
{
public:

    MachineCommand map(
        const JogCommand& jog
    );

    MachineSettings map(
        const ControllerSettingsSnapshot& snapshot
    ) const;

private:

    MachineCommand mapJogMove(
        const JogCommand& jog
    );

};

#endif