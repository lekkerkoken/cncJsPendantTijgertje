#include "MachineMapper.h"

#include <cstdio>

// ============================================================
// MAP
// ============================================================

MachineCommand MachineMapper::map(
    const JogCommand& jog
)
{
    MachineCommand command;


    switch(jog.type)
    {
        case JOG_MOVE:

            return mapJogMove(
                jog
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


    if(snapshot.controllerType == "Grbl")
    {
        settings.maxFeedrate.x =
            snapshot.settings["$110"].as<float>();

        settings.maxFeedrate.y =
            snapshot.settings["$111"].as<float>();

        settings.maxFeedrate.z =
            snapshot.settings["$112"].as<float>();
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


    if(snapshot.controllerType == "Grbl")
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


        /*
            ----------------------------------------------------
            ACTIVE WCS

            CNCjs levert de actieve work coordinate system
            via:

                parserstate.modal.wcs

            Bijvoorbeeld:
                "G54"
                "G55"

            Als deze informatie ontbreekt, blijft activeWcs
            leeg. Dit maakt de machine state niet ongeldig.
            ----------------------------------------------------
        */

        state.activeWcs =
            snapshot.state["parserstate"]["modal"]["wcs"]
                .as<String>();


        // overige mapping...


        valid = true;
    }


    return state;
}


// ============================================================
// MAP JOG MOVE
// ============================================================

MachineCommand MachineMapper::mapJogMove(
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
        command.type =
            MACHINE_COMMAND_NONE;

        command.command =
            "";

        return command;
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

            command.type =
                MACHINE_COMMAND_NONE;

            command.command =
                "";

            return command;
    }


    char buffer[64];


    /*
        De JogPlanner heeft de beweging al gepland.

        delta:
            relatieve afstand voor dit tijdslot.

        feedrate:
            door de JogPlanner bepaalde snelheid.

        MachineMapper vertaalt dit naar de
        controller-specifieke jog-syntax.
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