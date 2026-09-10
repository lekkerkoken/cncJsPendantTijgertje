#ifndef MACHINE_MAPPER_H
#define MACHINE_MAPPER_H

#include "JogCommand.h"
#include "MachineSettings.h"
#include "MachineState.h"
#include "ControllerType.h"

#include "ControllerSettingsSnapshot.h"
#include "ControllerStateSnapshot.h"


class MachineMapper
{
public:

    // ========================================================
    // JOG
    // ========================================================

    String map(
        const JogCommand& jog,
        ControllerType type
    );


    // ========================================================
    // SETTINGS
    // ========================================================

    MachineSettings map(
        const ControllerSettingsSnapshot& snapshot
    ) const;


    // ========================================================
    // STATE
    // ========================================================

    MachineState map(
        const ControllerStateSnapshot& snapshot,
        bool& valid
    ) const;


    // ========================================================
    // HOME
    // ========================================================

    String mapHome(
        Axis axis,
        ControllerType type
    );


    // ========================================================
    // ZERO WCS AXIS
    // ========================================================

    String mapZeroAxis(
        Axis axis,
        ControllerType type
    );


private:

    String mapJogMove(
        const JogCommand& jog,
        ControllerType type
    );


    String mapGrblJog(
        const JogCommand& jog
    );


    ControllerType controllerTypeFromString(
        const String& type
    ) const;


    String unsupported() const;
};

#endif