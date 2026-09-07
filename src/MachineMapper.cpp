#include "MachineMapper.h"

#include <cstdio>

ControllerType MachineMapper::controllerTypeFromString(
    const String& type
) const
{
    if(type == "Grbl")
    {
        return CONTROLLER_GRBL;
    }

    if(type == "TinyG")
    {
        return CONTROLLER_TINYG;
    }

    return CONTROLLER_UNKNOWN;
}
// ============================================================
// MAP
// ============================================================

MachineCommand MachineMapper::map(
    const JogCommand& jog,
    ControllerType type

)
{
    MachineCommand command;


    switch(jog.type)
    {
        case JOG_MOVE:

            return mapJogMove(
                jog,
                type
            );


        case JOG_NONE:

        default:

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }
}


MachineSettings MachineMapper::map(
    const ControllerSettingsSnapshot& snapshot
) const
{
    MachineSettings settings;

    if(snapshot.settings.isNull())
    {
        return settings;
    }

    settings.controllerType =
        controllerTypeFromString(
            snapshot.controllerType
        );

    switch(settings.controllerType)
    {
        case CONTROLLER_GRBL:

            settings.maxFeedrate.x =
                snapshot.settings["$110"].as<float>();

            settings.maxFeedrate.y =
                snapshot.settings["$111"].as<float>();

            settings.maxFeedrate.z =
                snapshot.settings["$112"].as<float>();

            break;


        case CONTROLLER_TINYG:

            // TinyG settings mapping

            break;


        case CONTROLLER_UNKNOWN:

        default:

            break;
    }


    return settings;
}


MachineState MachineMapper::map(
    const ControllerStateSnapshot& snapshot,
    bool& valid
) const
{
    valid = false;

    MachineState state;


    if(snapshot.state.isNull())
    {
        return state;
    }


ControllerType type =
    controllerTypeFromString(
        snapshot.controllerType
    );


    switch(type)
    {
        case CONTROLLER_GRBL:
        {
            JsonObjectConst status =
                snapshot.state["status"].as<JsonObjectConst>();


            if(status.isNull())
            {
                return state;
            }


            String activeState =
                status["activeState"].as<String>();


            if(activeState == "Idle")
            {
                state.machineStatus =
                    MACHINE_IDLE;
            }
            else if(activeState == "Run")
            {
                state.machineStatus =
                    MACHINE_RUN;
            }
            else if(activeState == "Hold")
            {
                state.machineStatus =
                    MACHINE_HOLD;
            }
            else if(activeState == "Alarm")
            {
                state.machineStatus =
                    MACHINE_ALARM;
            }
            else
            {
                return state;
            }


            state.activeWcs =
                snapshot.state["parserstate"]["modal"]["wcs"]
                    .as<String>();


            valid = true;

            break;
        }


        case CONTROLLER_TINYG:

            // TinyG state mapping

            break;


        case CONTROLLER_UNKNOWN:

        default:

            break;
    }


    return state;
}


// ============================================================
// MAP JOG MOVE
// ============================================================

MachineCommand MachineMapper::mapJogMove(
    const JogCommand& jog,
    ControllerType type
)
{
    switch(type)
    {
        case CONTROLLER_GRBL:

            return mapGrblJog(
                jog
            );


        case CONTROLLER_TINYG:

            // TinyG jog mapping

            return unsupported();


        case CONTROLLER_UNKNOWN:

        default:

            return unsupported();
    }
}

// ============================================================
// MAP GRBL JOG
// ============================================================

MachineCommand MachineMapper::mapGrblJog(
    const JogCommand& jog
)
{
    MachineCommand command;


    command.type =
        MACHINE_COMMAND_GCODE;


    if(
        jog.axis ==
        AXIS_NONE
    )
    {
        return unsupported();
    }


    char axisChar =
        'X';


    switch(jog.axis)
    {
        case AXIS_X:

            axisChar =
                'X';

            break;


        case AXIS_Y:

            axisChar =
                'Y';

            break;


        case AXIS_Z:

            axisChar =
                'Z';

            break;


        case AXIS_NONE:

        default:

            return unsupported();
    }


    char buffer[64];


    /*
        De JogPlanner heeft de beweging al gepland.

        delta:
            relatieve afstand voor dit tijdslot.

        feedrate:
            door de JogPlanner bepaalde snelheid.

        Deze functie vertaalt de generieke JogCommand
        naar GRBL-specifieke jog-syntax.
    */

    snprintf(
        buffer,
        sizeof(buffer),
        "$J=G91 %c%.3f F%d",
        axisChar,
        jog.delta,
        jog.feedrate
    );


    command.command =
        buffer;


    return command;
}

MachineCommand MachineMapper::unsupported() const
{
    MachineCommand command;


    command.type =
        MACHINE_COMMAND_NONE;

    command.command =
        "";


    return command;
}